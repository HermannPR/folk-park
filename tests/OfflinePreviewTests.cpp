#include "render/OfflinePreviewRenderer.h"

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

folkpark::midi::CompositionBundle acceptedFixture()
{
    using namespace folkpark::midi;
    MusicIntent intent;
    intent.seed = 5081;
    intent.requestId = deterministicUuid(intent.seed, "m5-offline-preview");
    intent.tempoBpm = 120.0;
    intent.lengthBars = 1;
    intent.partCount = 1;
    intent.parts[0] = PartType::melody;
    GeneratedClip clip;
    clip.id = deterministicUuid(intent.seed, "m5-offline-preview-clip");
    clip.part = PartType::melody;
    clip.lengthTicks = compositionPpq;
    clip.tempoBpm = intent.tempoBpm;
    clip.timeSignature = intent.timeSignature;
    clip.key = intent.key;
    clip.scale = intent.scale;
    clip.seed = intent.seed;
    clip.createdUnixMs = 1;
    clip.events.push_back({0, compositionPpq / 2, 60, 108, 1, 1.0f,
                           Articulation::normal});
    return {intent, {clip}};
}

bool buffersMatch(const juce::AudioBuffer<float>& first,
                  const juce::AudioBuffer<float>& second,
                  float tolerance)
{
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            if (std::abs(first.getSample(channel, sample)
                         - second.getSample(channel, sample)) > tolerance)
                return false;
    return true;
}

folkpark::render::OfflinePreviewSnapshot renderSnapshot(
    const std::shared_ptr<const folkpark::synth::WavetableBank>& bank)
{
    folkpark::render::OfflinePreviewSnapshot snapshot;
    snapshot.wavetableA = bank;
    snapshot.wavetableB = bank;
    snapshot.masterGainDb = -6.0f;
    snapshot.effectParameters.distortionBypass = false;
    snapshot.effectParameters.distortionDriveDb = 12.0f;
    snapshot.effectParameters.delayBypass = false;
    snapshot.effectParameters.delayDivision = 4;
    snapshot.effectParameters.delayMix = 0.2f;
    return snapshot;
}

void testWavContractAndOverwriteSafety()
{
    auto owned = folkpark::synth::WavetableBank::createBuiltIn();
    const auto bank = std::shared_ptr<const folkpark::synth::WavetableBank>(std::move(owned));
    const auto bundle = acceptedFixture();
    const auto snapshot = renderSnapshot(bank);
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("folk-park-m5-offline-tests", {}, true);
    expect(directory.createDirectory(), "Offline test directory must be created");
    const auto destination = directory.getChildFile("accepted-preview.wav");
    folkpark::render::OfflinePreviewRenderer renderer;
    const auto result = renderer.render(bundle, snapshot, destination);
    if (result.status.failed())
        std::cerr << "Render error: " << result.status.getErrorMessage() << '\n';
    const auto expectedSamples = static_cast<std::int64_t>(std::ceil(
        (0.5 + folkpark::render::OfflinePreviewRenderer::tailSeconds) * 48000.0));
    expect(result.succeeded(), "An accepted isolated composition must render successfully");
    expect(result.sampleCount == expectedSamples && result.sampleRate == 48000.0,
           "Offline render metadata must expose deterministic length and sample rate");
    expect(folkpark::render::OfflinePreviewRenderer::validateWav(
               destination, expectedSamples, 48000.0).wasOk(),
           "Rendered file must reopen with the required WAV header, rate, depth, and length");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(destination));
    juce::AudioBuffer<float> audio(2, 24000);
    expect(reader != nullptr && reader->read(&audio, 0, audio.getNumSamples(), 0, true, true),
           "Rendered WAV must expose readable stereo sample data");
    expect(audio.getMagnitude(0, 0, audio.getNumSamples()) > 1.0e-5f,
           "Accepted WAV must contain the isolated synth rather than silence");

    const auto sizeBefore = destination.getSize();
    const auto refused = renderer.render(bundle, snapshot, destination, false);
    expect(refused.status.failed() && destination.getSize() == sizeBefore,
           "A render must not overwrite an existing destination without explicit authorization");
    expect(directory.deleteRecursively(), "Offline test artifacts must be cleaned up");
}

void testLiveEngineIsolation()
{
    auto owned = folkpark::synth::WavetableBank::createBuiltIn();
    const auto bank = std::shared_ptr<const folkpark::synth::WavetableBank>(std::move(owned));
    folkpark::synth::SynthEngine live;
    folkpark::synth::SynthEngine control;
    folkpark::synth::ParameterSnapshot parameters;
    live.prepare(48000.0, 512);
    control.prepare(48000.0, 512);
    juce::MidiBuffer liveMidi;
    juce::MidiBuffer controlMidi;
    liveMidi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
    controlMidi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
    juce::AudioBuffer<float> liveAudio(2, 512);
    juce::AudioBuffer<float> controlAudio(2, 512);
    live.process(liveAudio, liveMidi, parameters);
    control.process(controlAudio, controlMidi, parameters);
    expect(buffersMatch(liveAudio, controlAudio, 0.0f),
           "Live-isolation fixture must begin from identical active voices");

    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("folk-park-m5-isolation-tests", {}, true);
    expect(directory.createDirectory(), "Isolation test directory must be created");
    folkpark::render::OfflinePreviewRenderer renderer;
    const auto isolated = renderer.render(acceptedFixture(), renderSnapshot(bank),
                                          directory.getChildFile("isolated.wav"));
    if (isolated.status.failed())
        std::cerr << "Isolation render error: " << isolated.status.getErrorMessage() << '\n';
    expect(isolated.succeeded(),
           "Isolation fixture must complete its separate render");

    liveMidi.clear();
    controlMidi.clear();
    live.process(liveAudio, liveMidi, parameters);
    control.process(controlAudio, controlMidi, parameters);
    expect(live.getActiveVoiceCount() == control.getActiveVoiceCount()
               && buffersMatch(liveAudio, controlAudio, 0.0f),
           "Offline rendering must not seek, reset, release, or mutate live synth voices");
    expect(directory.deleteRecursively(), "Isolation test artifacts must be cleaned up");
}
}

int main()
{
    testWavContractAndOverwriteSafety();
    testLiveEngineIsolation();
    if (failures == 0)
        std::cout << "PASS: M5 isolated WAV rendering preserves live voices and validates file contracts\n";
    return failures == 0 ? 0 : 1;
}
