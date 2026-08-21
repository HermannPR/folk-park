#include "plugin/ParameterIds.h"
#include "plugin/PluginProcessor.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <set>

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
    auto* oscillatorBLevel = source.state().getParameter(folkpark::parameterIds::oscillatorBLevel);
    auto* filterMode = source.state().getParameter(folkpark::parameterIds::filterMode);
    auto* lfo4Shape = source.state().getParameter(folkpark::parameterIds::lfoShape[3]);
    auto* lfo1Rate = source.state().getParameter(folkpark::parameterIds::lfoRate[0]);
    expect(cutoff != nullptr && waveform != nullptr && oscillatorBLevel != nullptr
               && filterMode != nullptr && lfo4Shape != nullptr && lfo1Rate != nullptr,
           "Required M1/M2 parameters must exist");
    if (cutoff == nullptr || waveform == nullptr || oscillatorBLevel == nullptr
        || filterMode == nullptr || lfo4Shape == nullptr || lfo1Rate == nullptr)
        return;

    expect(source.getParameters().size() == 73,
           "M2 append-only public parameter layout must contain exactly 73 parameters");
    std::set<juce::String> stableIds;
    for (const auto* parameter : source.getParameters())
    {
        const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        expect(identified != nullptr, "Every public parameter must expose a stable ID");
        if (identified != nullptr)
            expect(stableIds.insert(identified->paramID).second, "Every public parameter ID must be unique");
    }

    cutoff->setValueNotifyingHost(0.37f);
    waveform->setValueNotifyingHost(1.0f);
    oscillatorBLevel->setValueNotifyingHost(0.65f);
    filterMode->setValueNotifyingHost(1.0f);
    lfo4Shape->setValueNotifyingHost(1.0f);
    lfo1Rate->setValueNotifyingHost(0.43f);
    const std::array routes{
        folkpark::synth::ModulationRoute{folkpark::synth::ModulationSource::lfo1,
                                        folkpark::synth::ModulationDestination::oscillatorAPosition,
                                        0.42f, folkpark::synth::ModulationCurve::sCurve, true},
        folkpark::synth::ModulationRoute{folkpark::synth::ModulationSource::filterEnvelope,
                                        folkpark::synth::ModulationDestination::filterCutoff,
                                        -0.6f, folkpark::synth::ModulationCurve::linear, true},
    };
    expect(source.setModulationRoutes(routes).wasOk(),
           "Validated modulation routes must be accepted before serialization");
    expect(source.setModulationRoutes(routes).wasOk(),
           "Reapplying an identical route snapshot must be an idempotent success");
    juce::MemoryBlock state;
    source.getStateInformation(state);
    expect(!state.isEmpty(), "Serialized plug-in state must not be empty");

    folkpark::PluginProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto* restoredCutoff = restored.state().getParameter(folkpark::parameterIds::filterCutoff);
    const auto* restoredWaveform = restored.state().getParameter(folkpark::parameterIds::oscillatorWaveform);
    const auto* restoredOscillatorBLevel
        = restored.state().getParameter(folkpark::parameterIds::oscillatorBLevel);
    const auto* restoredFilterMode = restored.state().getParameter(folkpark::parameterIds::filterMode);
    const auto* restoredLfo4Shape = restored.state().getParameter(folkpark::parameterIds::lfoShape[3]);
    const auto* restoredLfo1Rate = restored.state().getParameter(folkpark::parameterIds::lfoRate[0]);
    expect(std::abs(restoredCutoff->getValue() - cutoff->getValue()) <= 1.0e-7f,
           "Filter cutoff must survive state round trip");
    expect(std::abs(restoredWaveform->getValue() - waveform->getValue()) <= 1.0e-7f,
           "Waveform must survive state round trip");
    expect(std::abs(restoredOscillatorBLevel->getValue() - oscillatorBLevel->getValue()) <= 1.0e-7f,
           "Oscillator B level must survive state round trip");
    expect(std::abs(restoredFilterMode->getValue() - filterMode->getValue()) <= 1.0e-7f,
           "Multimode filter choice must survive state round trip");
    expect(std::abs(restoredLfo4Shape->getValue() - lfo4Shape->getValue()) <= 1.0e-7f,
           "LFO 4 shape must survive state round trip");
    expect(std::abs(restoredLfo1Rate->getValue() - lfo1Rate->getValue()) <= 1.0e-7f,
           "LFO 1 rate must survive state round trip");
    const auto restoredRoutes = restored.getConfiguredModulationRoutes();
    expect(restoredRoutes.routeCount == routes.size(),
           "Validated modulation route count must survive state round trip");
    if (restoredRoutes.routeCount == routes.size())
    {
        expect(restoredRoutes.routes[0].source == routes[0].source
                   && restoredRoutes.routes[0].destination == routes[0].destination
                   && std::abs(restoredRoutes.routes[0].amount - routes[0].amount) <= 1.0e-7f
                   && restoredRoutes.routes[0].curve == routes[0].curve
                   && restoredRoutes.routes[0].enabled == routes[0].enabled,
               "Complete modulation route fields must survive state round trip");
    }

    auto invalidRoute = routes[0];
    invalidRoute.destination = static_cast<folkpark::synth::ModulationDestination>(255);
    expect(restored.setModulationRoutes(std::span{&invalidRoute, 1}).failed(),
           "Processor boundary must reject unsupported modulation destinations");
    expect(restored.getConfiguredModulationRoutes().routeCount == routes.size(),
           "Rejected route candidate must not partially replace configured modulation state");
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
        std::cout << "PASS: M1/M2 parameter and route state, editor independence, and panic handoff\n";
    return failures == 0 ? 0 : 1;
}
