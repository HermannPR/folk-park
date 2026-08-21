#include "midi/MidiProof.h"
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
    parameters.attackSeconds = 0.001f;
    parameters.releaseSeconds = 0.01f;

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

    if (failures == 0)
        std::cout << "PASS: M0 MIDI proof and M1 real-time synth engine invariants\n";
    return failures == 0 ? 0 : 1;
}
