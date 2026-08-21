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
    auto* distortionBypass = source.state().getParameter(folkpark::parameterIds::distortionBypass);
    auto* delayFeedback = source.state().getParameter(folkpark::parameterIds::delayFeedback);
    auto* eqMidGain = source.state().getParameter(folkpark::parameterIds::eqMidGain);
    expect(cutoff != nullptr && waveform != nullptr && oscillatorBLevel != nullptr
               && filterMode != nullptr && lfo4Shape != nullptr && lfo1Rate != nullptr
               && distortionBypass != nullptr && delayFeedback != nullptr && eqMidGain != nullptr,
           "Required M1–M5 parameters must exist");
    if (cutoff == nullptr || waveform == nullptr || oscillatorBLevel == nullptr
        || filterMode == nullptr || lfo4Shape == nullptr || lfo1Rate == nullptr
        || distortionBypass == nullptr || delayFeedback == nullptr || eqMidGain == nullptr)
        return;

    expect(source.getParameters().size() == 102,
           "M5 append-only public parameter layout must contain exactly 102 parameters");
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
    distortionBypass->setValueNotifyingHost(0.0f);
    delayFeedback->setValueNotifyingHost(0.71f);
    eqMidGain->setValueNotifyingHost(0.82f);
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
    const auto* restoredDistortionBypass
        = restored.state().getParameter(folkpark::parameterIds::distortionBypass);
    const auto* restoredDelayFeedback
        = restored.state().getParameter(folkpark::parameterIds::delayFeedback);
    const auto* restoredEqMidGain = restored.state().getParameter(folkpark::parameterIds::eqMidGain);
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
    expect(std::abs(restoredDistortionBypass->getValue() - distortionBypass->getValue()) <= 1.0e-7f,
           "M5 effect bypass must survive state round trip");
    expect(std::abs(restoredDelayFeedback->getValue() - delayFeedback->getValue()) <= 1.0e-7f,
           "M5 delay feedback must survive state round trip");
    expect(std::abs(restoredEqMidGain->getValue() - eqMidGain->getValue()) <= 1.0e-7f,
           "M5 parametric EQ gain must survive state round trip");
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
    if (editor != nullptr)
    {
        editor->setSize(720, 560);
        expect(editor->getWidth() == 720 && editor->getHeight() == 560,
               "M4 editor must preserve its documented compact size without native clipping");
        editor->setSize(1600, 1100);
        expect(editor->getWidth() == 1600 && editor->getHeight() == 1100,
               "M4 editor must preserve its documented large size");
    }

    const auto wavetableA = openEditorProcessor.getWavetableUiSnapshot(0);
    const auto wavetableB = openEditorProcessor.getWavetableUiSnapshot(1);
    expect(wavetableA.frameCount == 4 && wavetableB.frameCount == 4,
           "M4 complete snapshot must expose both actual built-in four-frame tables");
    for (const auto* preview : {&wavetableA, &wavetableB})
    {
        const auto sampleCount = preview->frameCount * folkpark::WavetableUiSnapshot::samplesPerFrame;
        for (auto index = 0; index < sampleCount; ++index)
            expect(std::isfinite(preview->samples[static_cast<std::size_t>(index)])
                       && std::abs(preview->samples[static_cast<std::size_t>(index)]) <= 1.0001f,
                   "M4 wavetable UI snapshot samples must remain finite and normalized");
    }

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

    editor.reset();
    juce::MidiBuffer editorClosedMidi;
    openEditorProcessor.processBlock(openAudio, editorClosedMidi);
    expect(openEditorProcessor.getActiveVoiceCount() > 0
               && openAudio.getMagnitude(0, 0, openAudio.getNumSamples()) > 1.0e-7f,
           "Closing or losing the WebView must not stop an active synth voice or audio callback");

    openEditorProcessor.requestPanic();
    juce::MidiBuffer noMidi;
    openEditorProcessor.processBlock(openAudio, noMidi);
    expect(openEditorProcessor.getActiveVoiceCount() == 0,
           "Message-thread panic request must clear voices on the audio callback");
    expect(openAudio.getMagnitude(0, 0, openAudio.getNumSamples()) <= 1.0e-9f,
           "Panic must return plug-in output to silence");
}

void testHostAwareUndoRedo()
{
    folkpark::PluginProcessor processor;
    auto* cutoff = processor.state().getParameter(folkpark::parameterIds::filterCutoff);
    expect(cutoff != nullptr, "Undo fixture cutoff parameter must exist");
    if (cutoff == nullptr)
        return;
    const auto original = cutoff->getValue();
    const auto changed = original < 0.5f ? 0.8f : 0.2f;
    expect(processor.state().undoManager != nullptr,
           "M4 parameter attachments must have an undo manager");
    processor.state().undoManager->beginNewTransaction("M4 host-aware parameter gesture");
    cutoff->beginChangeGesture();
    cutoff->setValueNotifyingHost(changed);
    cutoff->endChangeGesture();
    expect(std::abs(cutoff->getValue() - changed) <= 1.0e-3f,
           "Host-aware parameter gesture must reach the APVTS value");
    const auto undone = processor.undoLastParameterChange();
    expect(undone && std::abs(cutoff->getValue() - original) <= 1.0e-3f,
           "M4 Undo must restore the preceding host-aware parameter gesture");
    const auto redone = processor.redoLastParameterChange();
    expect(redone && std::abs(cutoff->getValue() - changed) <= 1.0e-3f,
           "M4 Redo must restore the undone host-aware parameter gesture");
}

void testCompositionAcceptanceAndProcessorRouting()
{
    using namespace folkpark;
    PluginProcessor processor;
    midi::MusicIntent intent;
    intent.seed = 7007;
    intent.requestId = midi::deterministicUuid(intent.seed, "processor-composition-test");
    expect(processor.generateCompositionCandidate(intent).wasOk(),
           "Processor must accept a bounded composition intent on the message thread");
    const auto candidate = processor.getCompositionSnapshot();
    expect(candidate.hasCandidate && !candidate.hasAccepted && candidate.candidateNoteCount > 0,
           "Processor must preserve generation as an unaccepted candidate");
    expect(processor.writeAcceptedMidiToTemporaryFile() == juce::File{},
           "Unaccepted candidate must not be available to drag or export");
    expect(processor.routeAcceptedMidi().failed(),
           "Unaccepted candidate must not route directly to the host");
    expect(processor.acceptCompositionCandidate().wasOk(),
           "Explicit processor acceptance must enable delivery");

    const auto temporary = processor.writeAcceptedMidiToTemporaryFile();
    expect(temporary.existsAsFile() && temporary.getSize() > 0,
           "Accepted processor composition must produce a verified drag file");
    if (temporary.existsAsFile())
        expect(temporary.deleteFile(), "Processor test must clean up its temporary drag file");

    processor.prepareToPlay(48000.0, 512);
    expect(processor.routeAcceptedMidi().wasOk(),
           "Accepted composition must publish a direct-MIDI schedule");
    auto foundGeneratedMidi = false;
    auto foundGeneratedAudio = false;
    juce::AudioBuffer<float> audio(2, 512);
    for (int block = 0; block < 160 && !foundGeneratedMidi; ++block)
    {
        juce::MidiBuffer midiOutput;
        midiOutput.ensureSize(2048);
        processor.processBlock(audio, midiOutput);
        foundGeneratedMidi = !midiOutput.isEmpty();
        foundGeneratedAudio = foundGeneratedAudio
            || audio.getMagnitude(0, 0, audio.getNumSamples()) > 1.0e-7f;
    }
    expect(foundGeneratedMidi && foundGeneratedAudio,
           "Direct accepted MIDI must reach both the host output and the synth engine");
    processor.stopDirectMidi();
    juce::MidiBuffer stopped;
    stopped.ensureSize(2048);
    processor.processBlock(audio, stopped);
    expect(!processor.isDirectMidiPlaying(),
           "Direct MIDI Stop must complete on the next processor block");

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    PluginProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expect(!restored.getCompositionSnapshot().hasAccepted,
           "M3 session MIDI must not pretend to persist before M6 history support");
}

void testPreviewKeyboardProcessorPath()
{
    folkpark::PluginProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    expect(!processor.previewNoteOn(-1, 100) && !processor.previewNoteOn(60, 0)
               && !processor.previewNoteOff(128),
           "Processor preview boundary must reject invalid MIDI input");
    expect(processor.previewNoteOn(60, 104),
           "Playable UI piano note must enter the bounded native queue");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer started;
    started.ensureSize(4096);
    processor.processBlock(audio, started);
    auto foundOn = false;
    for (const auto event : started)
        foundOn = foundOn || (event.getMessage().isNoteOn()
                              && event.getMessage().getNoteNumber() == 60);
    expect(foundOn && audio.getMagnitude(0, 0, audio.getNumSamples()) > 1.0e-7f,
           "UI piano must drive both host MIDI output and the real synth audio path");

    editor.reset();
    juce::MidiBuffer released;
    released.ensureSize(4096);
    processor.processBlock(audio, released);
    auto foundOff = false;
    for (const auto event : released)
        foundOff = foundOff || (event.getMessage().isNoteOff()
                                && event.getMessage().getNoteNumber() == 60);
    expect(foundOff, "Editor close must release every tracked native preview note");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;
    testStateRoundTrip();
    testHostAwareUndoRedo();
    testUiIndependenceAndPanic();
    testCompositionAcceptanceAndProcessorRouting();
    testPreviewKeyboardProcessorPath();

    if (failures == 0)
        std::cout << "PASS: M1–M3 state/delivery plus M4 UI preview keyboard processor path\n";
    return failures == 0 ? 0 : 1;
}
