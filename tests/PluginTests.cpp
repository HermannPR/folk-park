#include "plugin/ParameterIds.h"
#include "plugin/PluginProcessor.h"

#include <juce_events/juce_events.h>

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

juce::MidiBuffer noteOnBuffer(int note = 60)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    return midi;
}

bool buffersMatch(const juce::AudioBuffer<float>& first,
                  const juce::AudioBuffer<float>& second,
                  float tolerance)
{
    if (first.getNumChannels() != second.getNumChannels()
        || first.getNumSamples() != second.getNumSamples())
        return false;

    for (int channel = 0; channel < first.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
        {
            if (std::abs(first.getSample(channel, sample) - second.getSample(channel, sample)) > tolerance)
                return false;
        }
    }
    return true;
}

void testStateRoundTrip()
{
    folkpark::PluginProcessor source;
    auto* cutoff = source.state().getParameter(folkpark::parameterIds::filterCutoff);
    auto* waveform = source.state().getParameter(folkpark::parameterIds::oscillatorWaveform);
    expect(cutoff != nullptr && waveform != nullptr, "Required M1 parameters must exist");
    if (cutoff == nullptr || waveform == nullptr)
        return;

    cutoff->setValueNotifyingHost(0.37f);
    waveform->setValueNotifyingHost(1.0f);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    expect(!state.isEmpty(), "Serialized plug-in state must not be empty");

    folkpark::PluginProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto* restoredCutoff = restored.state().getParameter(folkpark::parameterIds::filterCutoff);
    const auto* restoredWaveform = restored.state().getParameter(folkpark::parameterIds::oscillatorWaveform);
    expect(std::abs(restoredCutoff->getValue() - cutoff->getValue()) <= 1.0e-7f,
           "Filter cutoff must survive state round trip");
    expect(std::abs(restoredWaveform->getValue() - waveform->getValue()) <= 1.0e-7f,
           "Waveform must survive state round trip");
}

void testUiIndependenceAndPanic()
{
    folkpark::PluginProcessor closedEditorProcessor;
    folkpark::PluginProcessor openEditorProcessor;
    closedEditorProcessor.prepareToPlay(48000.0, 512);
    openEditorProcessor.prepareToPlay(48000.0, 512);

    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(openEditorProcessor.createEditor());
    expect(editor != nullptr, "M1 editor must be constructible");

    juce::AudioBuffer<float> closedAudio(2, 512);
    juce::AudioBuffer<float> openAudio(2, 512);
    auto closedMidi = noteOnBuffer();
    auto openMidi = noteOnBuffer();
    closedEditorProcessor.processBlock(closedAudio, closedMidi);
    openEditorProcessor.processBlock(openAudio, openMidi);

    expect(closedAudio.getMagnitude(0, 0, closedAudio.getNumSamples()) > 1.0e-6f,
           "Plugin processor must produce audio after note-on");
    expect(buffersMatch(closedAudio, openAudio, 1.0e-7f),
           "Opening the editor must not change deterministic audio rendering");

    openEditorProcessor.requestPanic();
    juce::MidiBuffer noMidi;
    openEditorProcessor.processBlock(openAudio, noMidi);
    expect(openEditorProcessor.getActiveVoiceCount() == 0,
           "Message-thread panic request must clear voices on the audio callback");
    expect(openAudio.getMagnitude(0, 0, openAudio.getNumSamples()) <= 1.0e-9f,
           "Panic must return plug-in output to silence");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;
    testStateRoundTrip();
    testUiIndependenceAndPanic();

    if (failures == 0)
        std::cout << "PASS: M1 state, editor independence, and panic handoff\n";
    return failures == 0 ? 0 : 1;
}
