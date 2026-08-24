#include "midi/MidiProof.h"
#include "synth/Modulation.h"
#include "synth/SynthEngine.h"
#include "synth/WavetableBank.h"
#include "synth/WavetableConverter.h"
#include "synth/WavetableExchange.h"
#include "synth/WavetableImportService.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <vector>

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

bool isFiniteAndSilent(const juce::AudioBuffer<float>& audio)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto value = audio.getSample(channel, sample);
            if (!std::isfinite(value) || value != 0.0f)
                return false;
        }
    }
    return true;
}

float maximumMagnitude(const juce::AudioBuffer<float>& audio)
{
    auto magnitude = 0.0f;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        magnitude = std::max(magnitude, audio.getMagnitude(channel, 0, audio.getNumSamples()));
    return magnitude;
}

bool isFinite(const juce::AudioBuffer<float>& audio)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            if (!std::isfinite(audio.getSample(channel, sample)))
                return false;
    return true;
}

float maximumStereoDifference(const juce::AudioBuffer<float>& audio)
{
    auto difference = 0.0f;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        difference = std::max(difference,
                              std::abs(audio.getSample(0, sample) - audio.getSample(1, sample)));
    return difference;
}

class TemporaryTestDirectory final
{
public:
    TemporaryTestDirectory()
        : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("folkpark-m2-tests", {}, true))
    {
        expect(directory.createDirectory(), "Temporary wavetable test directory must be created");
    }

    ~TemporaryTestDirectory()
    {
        directory.deleteRecursively();
    }

    juce::File child(const juce::String& name) const { return directory.getChildFile(name); }

private:
    juce::File directory;
};

bool writeWavFixture(const juce::File& file, bool silent)
{
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat format;
    using Options = juce::AudioFormatWriterOptions;
    auto writer = format.createWriterFor(stream, Options{}.withSampleRate(48000.0)
                                                   .withNumChannels(2)
                                                   .withBitsPerSample(32));
    if (writer == nullptr)
        return false;

    constexpr int cycleLength = 2048;
    constexpr int cycleCount = 4;
    juce::AudioBuffer<float> audio(2, cycleLength * cycleCount);
    audio.clear();
    if (!silent)
    {
        for (int cycle = 0; cycle < cycleCount; ++cycle)
        {
            for (int sample = 0; sample < cycleLength; ++sample)
            {
                const auto phase = static_cast<float>(sample) / static_cast<float>(cycleLength);
                const auto sine = std::sin(juce::MathConstants<float>::twoPi * phase);
                const auto saw = 2.0f * phase - 1.0f;
                const auto morph = static_cast<float>(cycle) / static_cast<float>(cycleCount - 1);
                audio.setSample(0, cycle * cycleLength + sample, sine + morph * (saw - sine));
                audio.setSample(1, cycle * cycleLength + sample, 0.5f * sine + 0.25f * saw);
            }
        }
    }
    return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

bool waitForImportStatus(folkpark::synth::WavetableImportService& service,
                         folkpark::synth::WavetableImportService::Status expected,
                         int timeoutMilliseconds)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(timeoutMilliseconds);
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (service.getSnapshot().status == expected)
            return true;
        juce::Thread::sleep(5);
    }
    return service.getSnapshot().status == expected;
}

void testWavetableBankAndConverter()
{
    const auto builtIn = folkpark::synth::WavetableBank::createBuiltIn();
    expect(builtIn != nullptr, "Built-in wavetable bank must be created");
    expect(builtIn->getFrameCount() == 4, "Built-in bank must expose four morph frames");
    expect(builtIn->isFiniteAndNormalised(), "Built-in mipmapped bank must be finite and normalised");
    expect(builtIn->mipLevelForFrequency(8000.0f, 48000.0)
               > builtIn->mipLevelForFrequency(100.0f, 48000.0),
           "High notes must select a more strongly band-limited mip level");

    TemporaryTestDirectory temporary;
    const auto validFile = temporary.child("four-cycle-stereo.wav");
    const auto silentFile = temporary.child("silent.wav");
    const auto corruptFile = temporary.child("corrupt.wav");
    expect(writeWavFixture(validFile, false), "Valid stereo WAV fixture must be written");
    expect(writeWavFixture(silentFile, true), "Silent WAV fixture must be written");
    expect(corruptFile.replaceWithText("not a RIFF WAVE file"), "Corrupt WAV fixture must be written");

    folkpark::synth::WavetableConverter converter;
    const auto converted = converter.convertWavFile(validFile, 2048);
    expect(converted.succeeded(), "Valid user WAV must convert successfully");
    if (converted.succeeded())
    {
        expect(converted.bank->getFrameCount() == 4, "Four accepted cycles must become four frames");
        expect(converted.bank->isFiniteAndNormalised(), "Converted bank must be finite and normalised");
        expect(converted.metadata.sourceChannels == 2, "Converter metadata must retain source channel count");
        expect(converted.metadata.acceptedCycleLength == 2048,
               "Converter metadata must retain accepted cycle length");
        expect(converted.metadata.sourceSha256.length() == 64,
               "Converter metadata must include a SHA-256 content hash");
        expect(std::abs(converted.bank->getSample(0, 0, 0)
                        - converted.bank->getSample(0, 0, folkpark::synth::WavetableBank::tableSize - 1))
                   <= 1.0e-6f,
               "Continuity correction must make cycle endpoints meet");
        for (const auto value : converted.preview)
            expect(std::isfinite(value), "Wavetable preview must contain only finite samples");

        const auto repeated = converter.convertWavFile(validFile, 2048);
        expect(repeated.succeeded(), "Repeated deterministic conversion must succeed");
        if (repeated.succeeded())
        {
            for (int frame = 0; frame < converted.bank->getFrameCount(); ++frame)
            {
                for (int sample = 0; sample < folkpark::synth::WavetableBank::tableSize; ++sample)
                {
                    expect(std::abs(converted.bank->getSample(frame, 0, sample)
                                    - repeated.bank->getSample(frame, 0, sample)) <= 1.0e-7f,
                           "Identical WAV and cycle length must convert deterministically");
                }
            }
        }

        folkpark::synth::WavetableExchange exchange(*builtIn);
        const auto before = exchange.renderView().read(0.5f, 0.31f, 0);
        expect(exchange.publish(*converted.bank), "Validated converted bank must publish to a free slot");
        expect(exchange.hasPendingBank(), "Published bank must wait for an audio block boundary");
        exchange.beginAudioBlock();
        auto previous = before;
        auto maximumStep = 0.0f;
        for (int sample = 0; sample < folkpark::synth::WavetableExchange::crossfadeLengthSamples; ++sample)
        {
            const auto current = exchange.renderView().read(0.5f, 0.31f, 0);
            maximumStep = std::max(maximumStep, std::abs(current - previous));
            previous = current;
            exchange.advanceSample();
        }
        expect(maximumStep <= 0.02f, "Published bank must crossfade without a discontinuous table jump");
        expect(exchange.renderView().previous == nullptr,
               "Previous wavetable slot must retire only after the crossfade completes");

        auto publicationCount = 0;
        auto publishedTarget = -1;
        folkpark::synth::WavetableImportService importService(
            [&publicationCount, &publishedTarget](int target,
                                                  const folkpark::synth::WavetableBank& bank,
                                                  const auto& metadata,
                                                  const juce::File& source)
            {
                ++publicationCount;
                publishedTarget = target;
                return bank.isFiniteAndNormalised() && !metadata.sourceSha256.isEmpty()
                           && source.existsAsFile()
                    ? juce::Result::ok()
                    : juce::Result::fail("Confirmed import publication metadata is incomplete");
            });
        expect(importService.request(validFile, 1, 2048).wasOk(),
               "User-confirmed file selection must queue background conversion");
        expect(waitForImportStatus(importService,
                                   folkpark::synth::WavetableImportService::Status::awaitingConfirmation,
                                   5000),
               "Background conversion must produce a bounded preview awaiting confirmation");
        expect(publicationCount == 0,
               "Converted user WAV must not publish before explicit producer confirmation");
        const auto importPreview = importService.getSnapshot();
        expect(importPreview.metadata.outputFrameCount == 4,
               "Import preview must expose deterministic conversion metadata");
        expect(importService.confirm().wasOk(), "Explicit confirmation must publish the converted bank");
        expect(publicationCount == 1 && publishedTarget == 1,
               "Confirmed import must publish exactly once to the selected oscillator");

        expect(importService.request(corruptFile, 0).wasOk(),
               "Existing corrupt file must be checked on the bounded worker");
        expect(waitForImportStatus(importService,
                                   folkpark::synth::WavetableImportService::Status::failed,
                                   5000),
               "Corrupt background import must end in a recoverable failed state");
        expect(publicationCount == 1,
               "Failed background import must not replace a previously confirmed oscillator bank");
    }

    expect(converter.convertWavFile(corruptFile).status.failed(), "Corrupt WAV header must be rejected");
    expect(converter.convertWavFile(silentFile).status.failed(), "Silent WAV cycle must be rejected");
    expect(converter.convertWavFile(validFile, 8).status.failed(), "Undersized cycle must be rejected");
    expect(converter.convertWavFile(validFile, 65536).status.failed(),
           "Cycle longer than source material must be rejected");
    expect(converter.convertWavFile(temporary.child("missing.wav")).status.failed(),
           "Missing WAV must be rejected");

    juce::AudioBuffer<float> nonFinite(1, 64);
    nonFinite.clear();
    nonFinite.setSample(0, 7, std::numeric_limits<float>::quiet_NaN());
    expect(folkpark::synth::WavetableConverter::validateDecodedAudio(nonFinite).failed(),
           "Non-finite decoded audio must be rejected before conversion");
    juce::AudioBuffer<float> tooManyChannels(9, 64);
    expect(folkpark::synth::WavetableConverter::validateDecodedAudio(tooManyChannels).failed(),
           "Excessive channel count must be rejected before conversion");
    juce::AudioBuffer<float> oversized(1,
        static_cast<int>(folkpark::synth::WavetableConverter::maximumSourceSamples + 1));
    expect(folkpark::synth::WavetableConverter::validateDecodedAudio(oversized).failed(),
           "Excessive decoded sample count must be rejected before scanning samples");
}

void testBandLimitedSpectrum()
{
    using Bank = folkpark::synth::WavetableBank;
    const auto bank = Bank::createBuiltIn();
    const auto mip = bank->mipLevelForFrequency(8000.0f, 48000.0);
    const auto maximumHarmonic = std::max(1, (Bank::tableSize / 2) >> mip);
    juce::dsp::FFT fft(11);
    std::vector<juce::dsp::Complex<float>> input(static_cast<std::size_t>(Bank::tableSize));
    std::vector<juce::dsp::Complex<float>> spectrum(static_cast<std::size_t>(Bank::tableSize));
    for (int sample = 0; sample < Bank::tableSize; ++sample)
        input[static_cast<std::size_t>(sample)] = {bank->getSample(2, mip, sample), 0.0f};
    fft.perform(input.data(), spectrum.data(), false);

    auto retainedEnergy = 0.0;
    auto rejectedEnergy = 0.0;
    for (int bin = 1; bin < Bank::tableSize / 2; ++bin)
    {
        const auto energy = static_cast<double>(std::norm(spectrum[static_cast<std::size_t>(bin)]));
        if (bin <= maximumHarmonic)
            retainedEnergy += energy;
        else
            rejectedEnergy += energy;
    }
    const auto rejectionRatio = rejectedEnergy / std::max(retainedEnergy, 1.0e-30);
    expect(mip >= 8, "An 8 kHz oscillator must select a strongly reduced mip level at 48 kHz");
    expect(rejectionRatio < 1.0e-6,
           "Selected saw mip must suppress harmonics above its Nyquist-safe bound");
    std::cout << "M2 spectral evidence: mip=" << mip
              << ", maximum harmonic=" << maximumHarmonic
              << ", rejected/retained energy=" << rejectionRatio << '\n';
}

void testModulationRegistryAndExchange()
{
    using namespace folkpark::synth;
    expect(ModulationRegistry::sources().size() == static_cast<std::size_t>(ModulationSource::count),
           "Every modulation source must have a central descriptor");
    expect(ModulationRegistry::destinations().size()
               == static_cast<std::size_t>(ModulationDestination::count),
           "Every modulation destination must have a central descriptor");

    const std::array validRoutes{
        ModulationRoute{ModulationSource::lfo1, ModulationDestination::oscillatorAPosition,
                        0.5f, ModulationCurve::linear, true},
        ModulationRoute{ModulationSource::filterEnvelope, ModulationDestination::filterCutoff,
                        -0.75f, ModulationCurve::sCurve, true},
    };
    expect(ModulationRegistry::validate(validRoutes).wasOk(),
           "Supported bounded modulation routes must validate");
    const auto snapshot = ModulationRegistry::makeSnapshot(validRoutes);
    expect(snapshot.routeCount == validRoutes.size(), "Validated route snapshot must retain every route");

    ModulationRegistry::SourceValues sources{};
    sources[static_cast<std::size_t>(ModulationSource::lfo1)] = -0.5f;
    sources[static_cast<std::size_t>(ModulationSource::filterEnvelope)] = 1.0f;
    const auto destinations = ModulationRegistry::evaluate(snapshot, sources);
    expect(std::abs(destinations[static_cast<std::size_t>(ModulationDestination::oscillatorAPosition)]
                    + 0.25f) <= 1.0e-7f,
           "Bipolar LFO route must apply its bounded amount");
    expect(std::abs(destinations[static_cast<std::size_t>(ModulationDestination::filterCutoff)]
                    + 0.75f) <= 1.0e-7f,
           "Unipolar envelope route must apply its bounded amount and curve");

    auto invalidAmount = validRoutes[0];
    invalidAmount.amount = std::numeric_limits<float>::infinity();
    expect(ModulationRegistry::validate(std::span{&invalidAmount, 1}).failed(),
           "Non-finite modulation amount must be rejected");
    auto invalidSource = validRoutes[0];
    invalidSource.source = static_cast<ModulationSource>(255);
    expect(ModulationRegistry::validate(std::span{&invalidSource, 1}).failed(),
           "Unknown modulation source must be rejected");
    std::array<ModulationRoute, ModulationSnapshot::maximumRoutes + 1> excessiveRoutes{};
    expect(ModulationRegistry::validate(excessiveRoutes).failed(),
           "More than 32 modulation routes must be rejected");

    ModulationExchange exchange;
    expect(exchange.publish(validRoutes), "Validated modulation snapshot must publish off-thread");
    expect(exchange.hasPendingSnapshot(), "Published modulation must wait for a block boundary");
    expect(exchange.current().routeCount == 0, "Pending routes must not mutate the active audio snapshot");
    exchange.beginAudioBlock();
    expect(exchange.current().routeCount == validRoutes.size(),
           "Audio block boundary must atomically activate the complete route snapshot");
}

void testSynthSilenceAndFiniteAudio()
{
    folkpark::synth::SynthEngine engine;
    folkpark::synth::ParameterSnapshot parameters;
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    engine.prepare(48000.0, audio.getNumSamples());

    engine.process(audio, midi, parameters);
    expect(isFiniteAndSilent(audio), "Synth must be silent before note-on");

    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    engine.process(audio, midi, parameters);
    expect(maximumMagnitude(audio) > 1.0e-5f, "MIDI note-on must produce audible finite audio");
    expect(engine.getActiveVoiceCount() == 1, "One note-on must activate exactly one voice");
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        expect(std::isfinite(audio.getSample(0, sample)), "Left synth output must remain finite");
        expect(std::abs(audio.getSample(0, sample) - audio.getSample(1, sample)) <= 1.0e-7f,
               "M1 synth output must be centred stereo");
    }
}

void testDeterministicVoiceStealingAndPanic()
{
    folkpark::synth::SynthEngine engine;
    folkpark::synth::ParameterSnapshot parameters;
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    engine.prepare(48000.0, audio.getNumSamples());

    for (int note = 36; note < 36 + folkpark::synth::SynthEngine::maximumVoices + 1; ++note)
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(80)), 0);

    engine.process(audio, midi, parameters);
    expect(engine.getActiveVoiceCount() == folkpark::synth::SynthEngine::maximumVoices,
           "Voice count must remain bounded at 16");
    expect(!engine.isNoteActive(1, 36), "The oldest active voice must be stolen deterministically");
    expect(engine.isNoteActive(1, 52), "The newest note must survive voice stealing");

    engine.panic();
    midi.clear();
    engine.process(audio, midi, parameters);
    expect(engine.getActiveVoiceCount() == 0, "Panic must clear every voice");
    expect(isFiniteAndSilent(audio), "Panic must produce deterministic silence");
}

void testReleaseAndDeterminism()
{
    folkpark::synth::ParameterSnapshot parameters;
    parameters.ampEnvelope.attackSeconds = 0.001f;
    parameters.ampEnvelope.releaseSeconds = 0.01f;

    folkpark::synth::SynthEngine firstEngine;
    folkpark::synth::SynthEngine secondEngine;
    juce::AudioBuffer<float> firstAudio(2, 512);
    juce::AudioBuffer<float> secondAudio(2, 512);
    juce::MidiBuffer noteOn;
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
    firstEngine.prepare(48000.0, 512);
    secondEngine.prepare(48000.0, 512);
    firstEngine.process(firstAudio, noteOn, parameters);
    secondEngine.process(secondAudio, noteOn, parameters);

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int sample = 0; sample < 512; ++sample)
            expect(std::abs(firstAudio.getSample(channel, sample)
                            - secondAudio.getSample(channel, sample)) <= 1.0e-7f,
                   "Identical engine state and MIDI must render deterministically");
    }

    juce::MidiBuffer noteOff;
    noteOff.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
    firstEngine.process(firstAudio, noteOff, parameters);
    noteOff.clear();
    firstEngine.process(firstAudio, noteOff, parameters);
    expect(firstEngine.getActiveVoiceCount() == 0, "Completed release must retire the voice");
    expect(isFiniteAndSilent(firstAudio), "Completed release must return to silence");
}

void testM2DualOscillatorFilterNoiseAndModulation()
{
    using namespace folkpark::synth;
    constexpr auto blockSize = 512;

    {
        SynthEngine engine;
        ParameterSnapshot parameters;
        parameters.oscillatorA.levelDb = -60.0f;
        parameters.oscillatorB.levelDb = -6.0f;
        parameters.oscillatorB.position = 0.85f;
        parameters.oscillatorB.pan = 0.75f;
        parameters.oscillatorB.unisonVoices = 4;
        parameters.subLevelDb = -60.0f;
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(110)), 0);
        engine.prepare(48000.0, blockSize);
        engine.process(audio, midi, parameters);
        midi.clear();
        for (int block = 0; block < 5; ++block)
            engine.process(audio, midi, parameters);
        expect(isFinite(audio), "Oscillator B unison render must remain finite");
        expect(maximumMagnitude(audio) > 1.0e-5f, "Oscillator B must be independently audible");
        expect(maximumStereoDifference(audio) > 1.0e-5f,
               "Oscillator pan and unison spread must produce a stereo result");
    }

    {
        SynthEngine engine;
        ParameterSnapshot parameters;
        parameters.oscillatorA.position = 0.0f;
        parameters.oscillatorA.pan = -1.0f;
        parameters.subLevelDb = -60.0f;
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(110)), 0);
        engine.prepare(48000.0, blockSize);
        engine.process(audio, midi, parameters);
        midi.clear();
        engine.process(audio, midi, parameters);
        const std::array previous{audio.getSample(0, blockSize - 1),
                                  audio.getSample(1, blockSize - 1)};
        parameters.oscillatorA.position = 1.0f;
        parameters.oscillatorA.coarseSemitones = 36.0f;
        parameters.oscillatorA.fineCents = 100.0f;
        parameters.oscillatorA.pan = 1.0f;
        parameters.oscillatorA.unisonVoices = SynthEngine::maximumUnisonVoices;
        parameters.oscillatorA.unisonDetuneCents = 100.0f;
        parameters.oscillatorA.unisonSpread = 1.0f;
        parameters.oscillatorA.unisonBlend = 1.0f;
        engine.process(audio, midi, parameters);
        const auto boundaryStep = std::max(std::abs(audio.getSample(0, 0) - previous[0]),
                                           std::abs(audio.getSample(1, 0) - previous[1]));
        expect(boundaryStep < 0.05f,
               "Aggressive live oscillator automation must cross the block boundary click-safely");
        expect(isFinite(audio), "Smoothed oscillator automation must remain finite");
    }

    for (const auto mode : {FilterMode::lowPass, FilterMode::highPass, FilterMode::bandPass})
    {
        SynthEngine engine;
        ParameterSnapshot parameters;
        parameters.filterMode = mode;
        parameters.filterCutoffHz = mode == FilterMode::highPass ? 20000.0f : 20.0f;
        parameters.filterResonance = 1.0f;
        parameters.filterDriveDb = 24.0f;
        parameters.filterKeyTracking = 1.0f;
        parameters.filterEnvelopeOctaves = 8.0f;
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 120, static_cast<juce::uint8>(127)), 0);
        engine.prepare(48000.0, blockSize);
        engine.process(audio, midi, parameters);
        midi.clear();
        for (int block = 0; block < 8; ++block)
            engine.process(audio, midi, parameters);
        expect(isFinite(audio), "Every multimode filter must remain finite at bounded extremes");
    }

    {
        ParameterSnapshot whiteParameters;
        whiteParameters.oscillatorA.levelDb = -60.0f;
        whiteParameters.oscillatorB.levelDb = -60.0f;
        whiteParameters.subLevelDb = -60.0f;
        whiteParameters.noiseLevelDb = -6.0f;
        whiteParameters.noiseType = 0;
        auto pinkParameters = whiteParameters;
        pinkParameters.noiseType = 1;
        SynthEngine first;
        SynthEngine second;
        SynthEngine pink;
        juce::AudioBuffer<float> firstAudio(2, blockSize);
        juce::AudioBuffer<float> secondAudio(2, blockSize);
        juce::AudioBuffer<float> pinkAudio(2, blockSize);
        juce::MidiBuffer noteOn;
        noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        first.prepare(48000.0, blockSize);
        second.prepare(48000.0, blockSize);
        pink.prepare(48000.0, blockSize);
        first.process(firstAudio, noteOn, whiteParameters);
        second.process(secondAudio, noteOn, whiteParameters);
        pink.process(pinkAudio, noteOn, pinkParameters);
        auto pinkDiffers = false;
        for (int sample = 0; sample < blockSize; ++sample)
        {
            expect(std::abs(firstAudio.getSample(0, sample) - secondAudio.getSample(0, sample))
                       <= 1.0e-7f,
                   "White noise must be deterministic for identical engine state");
            pinkDiffers = pinkDiffers
                || std::abs(firstAudio.getSample(0, sample) - pinkAudio.getSample(0, sample)) > 1.0e-6f;
        }
        expect(pinkDiffers, "Pink noise must be a distinct deterministic source from white noise");
    }

    const std::array shapes{LfoShape::sine, LfoShape::triangle, LfoShape::saw, LfoShape::square};
    for (std::size_t index = 0; index < shapes.size(); ++index)
    {
        SynthEngine engine;
        ParameterSnapshot parameters;
        parameters.lfos[index].shape = shapes[index];
        parameters.lfos[index].rateHz = 30.0f;
        parameters.lfos[index].retrigger = true;
        const ModulationRoute route{
            static_cast<ModulationSource>(static_cast<int>(ModulationSource::lfo1)
                                          + static_cast<int>(index)),
            ModulationDestination::pan, 1.0f, ModulationCurve::linear, true};
        expect(engine.publishModulationRoutes(std::span{&route, 1}),
               "Each LFO source must publish as a bounded modulation route");
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        engine.prepare(48000.0, blockSize);
        auto stereoDifference = 0.0f;
        for (int block = 0; block < 4; ++block)
        {
            engine.process(audio, midi, parameters);
            midi.clear();
            stereoDifference = std::max(stereoDifference, maximumStereoDifference(audio));
        }
        expect(isFinite(audio), "Every LFO shape and source must render finite audio");
        expect(stereoDifference > 1.0e-5f, "Every LFO source must reach its registered destination");
        expect(engine.getActiveModulationSnapshot().routeCount == 1,
               "Complete modulation snapshot must activate at the next audio block");
    }

    {
        SynthEngine engine;
        ParameterSnapshot parameters;
        parameters.ampEnvelope.attackSeconds = 0.001f;
        parameters.ampEnvelope.releaseSeconds = 0.01f;
        parameters.filterEnvelope.releaseSeconds = 2.0f;
        parameters.auxiliaryEnvelope.releaseSeconds = 2.0f;
        const std::array routes{
            ModulationRoute{ModulationSource::filterEnvelope, ModulationDestination::filterCutoff,
                            0.8f, ModulationCurve::sCurve, true},
            ModulationRoute{ModulationSource::auxiliaryEnvelope, ModulationDestination::pan,
                            0.5f, ModulationCurve::linear, true},
        };
        expect(engine.publishModulationRoutes(routes),
               "Filter and auxiliary envelopes must publish through the central matrix");
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
        midi.addEvent(juce::MidiMessage::noteOff(1, 64), 100);
        engine.prepare(48000.0, blockSize);
        engine.process(audio, midi, parameters);
        midi.clear();
        engine.process(audio, midi, parameters);
        expect(isFinite(audio), "Three-envelope render and release must remain finite");
        expect(engine.getActiveVoiceCount() == 0,
               "Long filter/aux releases must not leave a voice stuck after amp release completes");
    }
}

void testM2CpuBaseline()
{
    using namespace folkpark::synth;
    constexpr auto blockSize = 512;
    constexpr auto blockCount = 40;
    SynthEngine engine;
    ParameterSnapshot parameters;
    parameters.oscillatorA.unisonVoices = SynthEngine::maximumUnisonVoices;
    parameters.oscillatorB.unisonVoices = SynthEngine::maximumUnisonVoices;
    parameters.oscillatorB.levelDb = -6.0f;
    parameters.filterResonance = 1.0f;
    parameters.filterDriveDb = 24.0f;
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    for (int note = 48; note < 48 + SynthEngine::maximumVoices; ++note)
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    engine.prepare(48000.0, blockSize);
    engine.process(audio, midi, parameters);
    midi.clear();
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int block = 0; block < blockCount; ++block)
        engine.process(audio, midi, parameters);
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;
    const auto audioDurationMs = 1000.0 * static_cast<double>(blockSize * blockCount) / 48000.0;
    const auto realtimeRatio = elapsedMs / audioDurationMs;
    expect(isFinite(audio), "Maximum M2 voice/unison CPU baseline must remain finite");
    expect(std::isfinite(realtimeRatio) && realtimeRatio > 0.0,
           "Maximum M2 CPU baseline must produce a valid positive measurement");
    // M8 records performance evidence but does not invent a pass/fail budget.
    // A release CPU threshold remains an explicit product-owner decision.
    std::cout << "M2 CPU evidence: 16 voices x 2 oscillators x 8 unison, "
              << blockCount << " blocks, elapsed=" << elapsedMs
              << " ms, audio=" << audioDurationMs << " ms, ratio=" << realtimeRatio << 'x' << '\n';
}
}

int main()
{
    const auto first = folkpark::midi::createM0ProofMidi();
    const auto second = folkpark::midi::createM0ProofMidi();

    expect(first == second, "M0 MIDI output must be deterministic");
    const auto validation = folkpark::midi::validateM0ProofMidi(first);
    expect(validation.wasOk(), validation.getErrorMessage().toRawUTF8());

    testSynthSilenceAndFiniteAudio();
    testDeterministicVoiceStealingAndPanic();
    testReleaseAndDeterminism();
    testWavetableBankAndConverter();
    testBandLimitedSpectrum();
    testModulationRegistryAndExchange();
    testM2DualOscillatorFilterNoiseAndModulation();
    testM2CpuBaseline();

    if (failures == 0)
        std::cout << "PASS: M0 MIDI, M1 synth, and M2 synthesis/import/modulation invariants\n";
    return failures == 0 ? 0 : 1;
}
