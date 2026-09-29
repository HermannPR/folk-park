#include "effects/EffectChain.h"
#include "midi/Composition.h"
#include "midi/MidiDelivery.h"
#include "midi/PreviewMidi.h"
#include "synth/SynthEngine.h"

#include <cmath>
#include <iostream>

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

bool finite(const juce::AudioBuffer<float>& audio)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            if (!std::isfinite(audio.getSample(channel, sample)))
                return false;
    return true;
}

folkpark::midi::CompositionBundle directMidiFixture()
{
    using namespace folkpark::midi;
    MusicIntent intent;
    intent.seed = 81001;
    intent.requestId = deterministicUuid(intent.seed, "m8-runtime-direct");
    intent.partCount = 1;
    intent.parts[0] = PartType::melody;
    intent.constraints.maxPolyphony = 1;
    intent.constraints.maximumEvents = 4;

    GeneratedClip clip;
    clip.id = deterministicUuid(intent.seed, "m8-runtime-direct-clip");
    clip.part = PartType::melody;
    clip.lengthTicks = compositionPpq * 4;
    clip.tempoBpm = intent.tempoBpm;
    clip.timeSignature = intent.timeSignature;
    clip.key = intent.key;
    clip.scale = intent.scale;
    clip.seed = intent.seed;
    clip.createdUnixMs = 1;
    clip.events.push_back({0, compositionPpq * 4, 60, 100, 1, 1.0f,
                           Articulation::normal});
    return {intent, {clip}};
}

void testLongRunFiniteAudioAndPanic()
{
    using namespace folkpark;
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 512;
    const auto simulatedSeconds = juce::jlimit(5, 600,
        juce::SystemStats::getEnvironmentVariable(
            "FOLK_PARK_RUNTIME_SECONDS", "12").getIntValue());
    const auto blockCount = static_cast<int>(
        simulatedSeconds * sampleRate / static_cast<double>(blockSize));

    synth::SynthEngine engine;
    synth::ParameterSnapshot synthParameters;
    synthParameters.oscillatorA.unisonVoices = 2;
    synthParameters.oscillatorB.unisonVoices = 2;
    synthParameters.oscillatorB.levelDb = -12.0f;
    engine.prepare(sampleRate, blockSize);

    effects::EffectChain effects;
    effects::Parameters effectParameters;
    effectParameters.distortionBypass = false;
    effectParameters.chorusBypass = false;
    effectParameters.delayBypass = false;
    effectParameters.reverbBypass = false;
    effectParameters.compressorBypass = false;
    effectParameters.eqBypass = false;
    effects.prepare(sampleRate, blockSize);

    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    midi.ensureSize(2048);
    const std::array notes{48, 55, 60, 64};
    auto maximumMagnitude = 0.0f;
    auto finiteThroughout = true;
    auto panicChecks = 0;
    const auto started = juce::Time::getMillisecondCounterHiRes();

    for (int block = 0; block < blockCount; ++block)
    {
        midi.clear();
        const auto cycle = block % 192;
        if (cycle == 0)
            for (const auto note : notes)
                midi.addEvent(juce::MidiMessage::noteOn(
                    1, note, static_cast<juce::uint8>(96)), 0);
        if (cycle == 96)
            for (const auto note : notes)
                midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);

        if (block == blockCount / 2)
        {
            engine.panic();
            expect(engine.getActiveVoiceCount() == 0,
                   "Repeated panic must synchronously clear every active synth voice");
            ++panicChecks;
        }

        engine.process(audio, midi, synthParameters);
        effects.process(audio, effectParameters);
        finiteThroughout = finiteThroughout && finite(audio);
        maximumMagnitude = juce::jmax(maximumMagnitude,
            audio.getMagnitude(0, 0, audio.getNumSamples()),
            audio.getMagnitude(1, 0, audio.getNumSamples()));
    }

    midi.clear();
    for (const auto note : notes)
        midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
    for (int releaseBlock = 0;
         releaseBlock < 512 && engine.getActiveVoiceCount() != 0; ++releaseBlock)
    {
        engine.process(audio, midi, synthParameters);
        effects.process(audio, effectParameters);
        finiteThroughout = finiteThroughout && finite(audio);
        midi.clear();
    }

    const auto elapsedMilliseconds = juce::Time::getMillisecondCounterHiRes() - started;
    const auto simulatedMilliseconds = static_cast<double>(blockCount * blockSize)
        * 1000.0 / sampleRate;
    expect(finiteThroughout, "Every long-run synth/effect output sample must remain finite");
    expect(maximumMagnitude > 1.0e-6f,
           "The long-run fixture must prove an audible non-silent signal path");
    expect(panicChecks == 1, "The runtime fixture must execute its scheduled panic check");
    expect(engine.getActiveVoiceCount() == 0,
           "Explicit final note-offs must decay to zero active voices within the bounded tail");
    std::cout << "M8 runtime evidence: " << blockCount << " blocks, "
              << simulatedMilliseconds / 1000.0 << " simulated seconds at 48 kHz/512, "
              << "four notes, 2x2 unison, all six effects enabled, " << panicChecks
              << " panic checks, elapsed=" << elapsedMilliseconds << " ms, ratio="
              << elapsedMilliseconds / simulatedMilliseconds << "x realtime\n";
}

void testPreviewOverflowAndReleaseRecovery()
{
    using namespace folkpark::midi;
    PreviewMidiQueue queue;
    juce::MidiBuffer output;
    output.ensureSize(4096);

    for (std::size_t index = 0; index < PreviewMidiQueue::maximumQueuedCommands; ++index)
        expect(queue.enqueueNoteOn(67, 100),
               "The fixed preview queue must accept every documented command slot");
    expect(queue.renderBlock(output) == 1,
           "Held-key repeat must collapse queued duplicate note-ons to one audible event");
    output.clear();
    queue.requestReleaseAll();
    expect(queue.renderBlock(output) == 1,
           "Focus/editor release-all must emit one note-off for the held preview note");
    output.clear();
    expect(queue.renderBlock(output) == 0,
           "Repeated release-all recovery must not emit or retain a stuck note");

    for (std::size_t index = 0; index < PreviewMidiQueue::maximumQueuedCommands; ++index)
        expect(queue.enqueueNoteOn(static_cast<int>(index), 90),
               "Distinct overflow fixture commands must fill the fixed queue exactly");
    expect(!queue.enqueueNoteOff(0),
           "A full preview queue must reject the extra release and request release-all safety");
    output.clear();
    expect(queue.renderBlock(output) == 0,
           "Overflow recovery must discard pending note-ons before they can become stuck");
    expect(queue.enqueueNoteOn(70, 100),
           "The preview queue must be usable immediately after overflow recovery");
    output.clear();
    expect(queue.renderBlock(output) == 1,
           "The recovered preview queue must publish a new note normally");
    queue.requestReleaseAll();
    output.clear();
    expect(queue.renderBlock(output) == 1,
           "The recovered preview note must retain an exact release path");
}

void testDirectMidiStopRecovery()
{
    using namespace folkpark::midi;
    DirectMidiPlayer player;
    const auto bundle = directMidiFixture();
    expect(validateBundle(bundle).wasOk() && player.publish(bundle).wasOk(),
           "The direct-MIDI stop fixture must validate and publish transactionally");
    juce::MidiBuffer output;
    output.ensureSize(4096);
    const auto first = player.renderBlock(output, 512, 48000.0);
    expect(first.emittedEvents == 1 && player.isPlaying(),
           "Direct MIDI must begin with the scheduled note-on at the next block");
    player.requestStop();
    output.clear();
    const auto stopped = player.renderBlock(output, 512, 48000.0);
    expect(stopped.completed && stopped.emittedEvents == 1 && !stopped.overflow
               && !player.isPlaying() && !player.hasPendingSchedule(),
           "Stop must emit the tracked note-off and clear active/pending direct-MIDI state");
    output.clear();
    const auto afterStop = player.renderBlock(output, 512, 48000.0);
    expect(afterStop.emittedEvents == 0 && !player.isPlaying(),
           "A stopped direct-MIDI player must stay silent and idempotent on later blocks");
}
}

int main()
{
    testLongRunFiniteAudioAndPanic();
    testPreviewOverflowAndReleaseRecovery();
    testDirectMidiStopRecovery();
    if (failures == 0)
        std::cout << "PASS: M8 finite long-run, panic, preview overflow, and direct-MIDI Stop recovery\n";
    return failures == 0 ? 0 : 1;
}
