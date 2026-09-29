#include "plugin/ParameterIds.h"
#include "plugin/PluginProcessor.h"

#include <juce_events/juce_events.h>

#include <algorithm>
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

bool writeWavetableFixture(const juce::File& file)
{
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(
        stream, juce::AudioFormatWriter::Options{}.withSampleRate(48000.0)
                                                      .withNumChannels(1)
                                                      .withBitsPerSample(24));
    if (writer == nullptr)
        return false;
    constexpr int cycleLength = 2048;
    juce::AudioBuffer<float> audio(1, cycleLength * 3);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        for (int sample = 0; sample < cycleLength; ++sample)
        {
            const auto phase = static_cast<float>(sample) / static_cast<float>(cycleLength);
            const auto sine = std::sin(juce::MathConstants<float>::twoPi * phase);
            const auto triangle = 1.0f - 4.0f * std::abs(phase - 0.5f);
            const auto morph = static_cast<float>(cycle) / 2.0f;
            audio.setSample(0, cycle * cycleLength + sample,
                            sine + morph * (triangle - sine));
        }
    }
    return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

bool waitForImportReview(folkpark::PluginProcessor& processor, int timeoutMilliseconds)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes()
        + static_cast<double>(timeoutMilliseconds);
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        const auto status = processor.getWavetableImportSnapshot().status;
        if (status == folkpark::synth::WavetableImportService::Status::awaitingConfirmation)
            return true;
        if (status == folkpark::synth::WavetableImportService::Status::failed)
            return false;
        juce::Thread::sleep(5);
    }
    return processor.getWavetableImportSnapshot().status
        == folkpark::synth::WavetableImportService::Status::awaitingConfirmation;
}

folkpark::PluginProcessor::PersistenceConfiguration disabledPersistence()
{
    return {false, {}};
}

folkpark::assistant::ParameterProposal makeAssistantProposal(
    folkpark::PluginProcessor& processor,
    std::uint64_t seed)
{
    using namespace folkpark;
    assistant::ParameterProposal proposal;
    proposal.proposalId = midi::deterministicUuid(seed, "processor-assistant-proposal");
    proposal.requestId = midi::deterministicUuid(seed, "processor-assistant-request");
    proposal.explanation = "A bounded processor A/B integration fixture";
    proposal.confidence = 0.8f;
    const auto current = processor.getAssistantParameterSnapshot();
    for (const auto& value : current)
    {
        if (value.parameterId == parameterIds::filterCutoff)
            proposal.changes.push_back({value.parameterId, value.normalized,
                                        value.normalized < 0.6f ? 0.72f : 0.32f,
                                        "Auditions a clearly different filter position"});
        if (value.parameterId == parameterIds::reverbMix)
            proposal.changes.push_back({value.parameterId, value.normalized,
                                        value.normalized < 0.5f ? 0.58f : 0.18f,
                                        "Auditions a clearly different spatial mix"});
    }
    return proposal;
}

struct TemporaryDirectory
{
    TemporaryDirectory()
    {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("folk-park-plugin-persistence-tests", {}, false);
        expect(directory.createDirectory(),
               "Temporary processor persistence directory must be created");
    }

    ~TemporaryDirectory()
    {
        if (directory.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
            directory.deleteRecursively(false);
    }

    juce::File directory;
};

void testStateRoundTrip()
{
    folkpark::PluginProcessor source(disabledPersistence());
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

    folkpark::PluginProcessor restored(disabledPersistence());
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

void testAssistantProposalAuditionAndAcceptance()
{
    using namespace folkpark;
    PluginProcessor processor(disabledPersistence());
    processor.prepareToPlay(48000.0, 256);
    const auto parameters = processor.getAssistantParameterSnapshot();
    expect(parameters.size()
               == parameterIds::synthAndModulation.size() + parameterIds::allEffects.size()
               && assistant::validateCurrentParameterValues(parameters).wasOk(),
           "Processor must expose one valid normalized snapshot for all 102 host parameters");

    auto* cutoff = processor.state().getParameter(parameterIds::filterCutoff);
    auto* reverbMix = processor.state().getParameter(parameterIds::reverbMix);
    expect(cutoff != nullptr && reverbMix != nullptr,
           "Assistant processor fixture parameters must exist");
    if (cutoff == nullptr || reverbMix == nullptr)
        return;
    const auto originalCutoff = cutoff->getValue();
    const auto originalReverb = reverbMix->getValue();
    const auto initiallyDirty = processor.getPersistenceStatus().currentPresetDirty;
    const auto proposal = makeAssistantProposal(processor, 71001);

    expect(processor.beginAssistantProposal(proposal).wasOk(),
           "A current catalog-valid proposal must enter processor-owned A/B state");
    expect(processor.beginAssistantProposal(proposal).failed(),
           "An active processor A/B session must reject replacement without a decision");
    expect(processor.getAssistantAuditionSnapshot().active()
               && processor.getAssistantAuditionSnapshot().audibleSide
                    == assistant::AuditionSide::original,
           "Processor A/B must begin on original A without changing the sound");
    const auto effectiveProposal = *processor.getAssistantAuditionSnapshot().proposal;
    expect(processor.auditionAssistantSide(assistant::AuditionSide::proposal).wasOk()
               && std::abs(cutoff->getValue() - effectiveProposal.changes[0].proposedNormalized) < 1.0e-6f
               && std::abs(reverbMix->getValue() - effectiveProposal.changes[1].proposedNormalized) < 1.0e-6f,
           "Audition B must update the exact normalized APVTS values");
    expect(processor.getPersistenceStatus().currentPresetDirty == initiallyDirty,
           "Temporary B audition must not mark the native sound permanently dirty");

    juce::MemoryBlock activeState;
    processor.getStateInformation(activeState);
    PluginProcessor reopened(disabledPersistence());
    reopened.prepareToPlay(48000.0, 256);
    reopened.setStateInformation(activeState.getData(), static_cast<int>(activeState.getSize()));
    const auto* reopenedCutoff = reopened.state().getParameter(parameterIds::filterCutoff);
    const auto reopenedAudition = reopened.getAssistantAuditionSnapshot();
    expect(reopenedCutoff != nullptr && reopenedAudition.active()
               && reopenedAudition.status == assistant::AssistantSessionStatus::previewing
               && reopenedAudition.audibleSide == assistant::AuditionSide::proposal
               && std::abs(reopenedCutoff->getValue()
                           - effectiveProposal.changes[0].proposedNormalized) < 1.0e-6f,
           "Host project reopen must restore active proposal B and retained original A without an editor");
    expect(reopened.auditionAssistantSide(assistant::AuditionSide::original).wasOk()
               && std::abs(reopenedCutoff->getValue() - originalCutoff) < 1.0e-6f
               && reopened.rejectAssistantProposal().wasOk(),
           "A restored project A/B session must still switch and reject reversibly");

    const auto activeXml = PluginProcessor::getXmlFromBinary(
        activeState.getData(), static_cast<int>(activeState.getSize()));
    expect(activeXml != nullptr, "Active assistant project state must reopen as JUCE XML");
    if (activeXml != nullptr)
    {
        auto malformedTree = juce::ValueTree::fromXml(*activeXml);
        auto session = malformedTree.getChildWithName("FolkParkProjectSession");
        auto assistantState = session.getChildWithName("FolkParkAssistantAudition");
        assistantState.setProperty("unexpected", "must reject", nullptr);
        juce::MemoryBlock malformedState;
        if (const auto malformedXml = malformedTree.createXml())
            PluginProcessor::copyXmlToBinary(*malformedXml, malformedState);
        PluginProcessor rejected(disabledPersistence());
        auto* rejectedCutoff = rejected.state().getParameter(parameterIds::filterCutoff);
        if (rejectedCutoff != nullptr)
            rejectedCutoff->setValueNotifyingHost(0.91f);
        const auto beforeRejected = rejectedCutoff != nullptr ? rejectedCutoff->getValue() : -1.0f;
        rejected.setStateInformation(malformedState.getData(),
                                     static_cast<int>(malformedState.getSize()));
        expect(rejectedCutoff != nullptr
                   && std::abs(rejectedCutoff->getValue() - beforeRejected) < 1.0e-7f
                   && !rejected.getAssistantAuditionSnapshot().active(),
               "Malformed assistant project state must reject before any live-sound mutation");
    }

    expect(processor.auditionAssistantSide(assistant::AuditionSide::original).wasOk()
               && std::abs(cutoff->getValue() - originalCutoff) < 1.0e-6f
               && std::abs(reverbMix->getValue() - originalReverb) < 1.0e-6f,
           "Audition A must restore both captured values exactly");
    expect(processor.auditionAssistantSide(assistant::AuditionSide::proposal).wasOk()
               && processor.rejectAssistantProposal().wasOk()
               && std::abs(cutoff->getValue() - originalCutoff) < 1.0e-6f
               && std::abs(reverbMix->getValue() - originalReverb) < 1.0e-6f
               && processor.getAssistantAuditionSnapshot().status
                    == assistant::AssistantSessionStatus::rejected,
           "Reject must restore A exactly and close the processor session");
    expect(processor.getPersistenceStatus().currentPresetDirty == initiallyDirty,
           "A rejected proposal must retain the original dirty-state boundary");

    const auto accepted = makeAssistantProposal(processor, 71002);
    const auto beganAccepted = processor.beginAssistantProposal(accepted);
    const auto acceptedEffective = processor.getAssistantAuditionSnapshot().proposal;
    expect(beganAccepted.wasOk() && acceptedEffective.has_value()
               && processor.acceptAssistantProposal().wasOk()
               && std::abs(cutoff->getValue()
                           - acceptedEffective->changes[0].proposedNormalized) < 1.0e-6f
               && processor.getAssistantAuditionSnapshot().status
                    == assistant::AssistantSessionStatus::accepted,
           "Accept must apply B and close the processor session explicitly");
    expect(processor.getPersistenceStatus().currentPresetDirty,
           "An explicitly accepted proposal must mark the current sound dirty");
    expect(processor.acceptAssistantProposal().failed(),
           "A finished proposal must not be accepted twice");

    const auto invalidated = makeAssistantProposal(processor, 71003);
    expect(processor.beginAssistantProposal(invalidated).wasOk(),
           "A new proposal may begin after the preceding explicit decision");
    cutoff->setValueNotifyingHost(cutoff->getValue() > 0.5f ? 0.22f : 0.82f);
    const auto externallyEdited = cutoff->getValue();
    expect(processor.auditionAssistantSide(assistant::AuditionSide::proposal).failed()
               && processor.getAssistantAuditionSnapshot().status
                    == assistant::AssistantSessionStatus::failed
               && std::abs(cutoff->getValue() - externallyEdited) < 1.0e-6f,
           "An external host edit must invalidate A/B without being overwritten");

    juce::AudioBuffer<float> audio(2, 256);
    auto midi = noteOnBuffer();
    processor.processBlock(audio, midi);
    expect(audio.getMagnitude(0, 0, audio.getNumSamples()) > 1.0e-7f,
           "Assistant failure and A/B decisions must not stop finite active audio");
}

void testProcessorOfflineJarvisOrchestration()
{
    using namespace folkpark;
    PluginProcessor processor(disabledPersistence());

    assistant::AssistantRequest compositionRequest;
    compositionRequest.requestId = midi::deterministicUuid(72001, "processor-jarvis-composition");
    compositionRequest.target = assistant::AssistantTarget::composition;
    compositionRequest.prompt = "Create an 8 bar F# harmonic minor arp and bass at 132 bpm";
    midi::MusicIntent fallbackIntent;
    fallbackIntent.requestId = compositionRequest.requestId;
    fallbackIntent.seed = 72001;
    compositionRequest.compositionFallback = fallbackIntent;
    const auto composition = processor.runOfflineAssistant(compositionRequest);
    expect(composition.status.wasOk() && composition.response.has_value()
               && composition.response->musicIntent.has_value(),
           "Processor must expose the deterministic offline composition assistant without an editor");
    if (composition.response && composition.response->musicIntent)
    {
        expect(processor.generateCompositionCandidate(*composition.response->musicIntent).wasOk()
                   && processor.getCompositionSnapshot().hasCandidate
                   && !processor.getCompositionSnapshot().hasAccepted,
               "Jarvis composition text must create only a reviewable candidate");
    }

    assistant::SoundIntent unanswered;
    unanswered.requestId = midi::deterministicUuid(72002, "processor-jarvis-questions");
    unanswered.entryMode = assistant::SoundEntryMode::guided;
    const auto questions = processor.getAssistantQuestions(unanswered);
    expect(questions.questions.size() == 2 && !questions.readyForProposal,
           "Processor must expose the stable two-at-a-time offline walkthrough");

    assistant::AssistantRequest soundRequest;
    soundRequest.requestId = midi::deterministicUuid(72003, "processor-jarvis-sound");
    soundRequest.target = assistant::AssistantTarget::sound;
    soundRequest.prompt = "Build a bright plucky wide lead with spacious movement";
    assistant::SoundIntent soundIntent;
    soundIntent.requestId = soundRequest.requestId;
    soundIntent.seed = 72003;
    soundIntent.entryMode = assistant::SoundEntryMode::guided;
    soundIntent.answers.musicalRole = "wide lead";
    soundIntent.answers.timbre = "bright glassy";
    soundIntent.answers.articulation = "plucky";
    soundIntent.answers.movement = "moving";
    soundIntent.answers.space = "spacious";
    soundIntent.answers.intensity = 0.75f;
    soundIntent.answers.genreContext = "melodic techno";
    soundRequest.soundIntent = soundIntent;
    const auto sound = processor.runOfflineAssistant(soundRequest);
    expect(sound.status.wasOk() && sound.response.has_value()
               && sound.response->parameterProposal.has_value(),
           "Processor must map a complete guided intent against its real host snapshot");
    if (sound.response && sound.response->parameterProposal)
    {
        expect(processor.beginAssistantProposal(*sound.response->parameterProposal).wasOk()
                   && processor.getAssistantAuditionSnapshot().active()
                   && processor.getAssistantAuditionSnapshot().audibleSide
                        == assistant::AuditionSide::original,
               "An offline sound response must enter review without silently applying B");
    }
}

void testUiIndependenceAndPanic()
{
    folkpark::PluginProcessor closedEditorProcessor(disabledPersistence());
    folkpark::PluginProcessor openEditorProcessor(disabledPersistence());
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
    folkpark::PluginProcessor processor(disabledPersistence());
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
    PluginProcessor processor(disabledPersistence());
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
    PluginProcessor restored(disabledPersistence());
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expect(restored.getCompositionSnapshot().hasAccepted,
           "M6 project state must restore the explicitly accepted composition");
}

void testPreviewKeyboardProcessorPath()
{
    folkpark::PluginProcessor processor(disabledPersistence());
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

void testPresetAndHistoryRestartIntegration()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    PluginProcessor processor({true, temporary.directory});
    processor.prepareToPlay(48000.0, 512);
    expect(processor.initialisePersistence().wasOk(),
           "M6 processor persistence root must initialize outside the audio callback");
    const auto initialStatus = processor.getPersistenceStatus();
    expect(initialStatus.presetAvailable && initialStatus.historyAvailable,
           "M6 temporary preset library and SQLite history must both be available");

    auto* cutoff = processor.state().getParameter(parameterIds::filterCutoff);
    expect(cutoff != nullptr, "M6 preset integration cutoff parameter must exist");
    if (cutoff == nullptr)
        return;
    cutoff->setValueNotifyingHost(0.273f);
    const auto savedCutoff = cutoff->getValue();
    PresetSaveRequest save;
    save.name = "Integration lead";
    save.author = "folk park tests";
    save.tags = {"lead", "m6"};
    save.genre = "house";
    save.emotion = "bright";
    save.description = "Processor-level transactional preset fixture";
    expect(processor.saveCurrentPreset(save).wasOk(),
           "Current processor sound must save through the atomic native preset store");
    expect(!processor.getPersistenceStatus().currentPresetDirty,
           "A successful native preset save must establish a clean parameter revision");
    const auto library = processor.listPresets();
    expect(library.status.wasOk() && library.presets.size() == 1,
           "Saved native preset must appear in the bounded local browser");
    if (library.presets.empty())
        return;
    const auto presetId = library.presets.front().id;
    cutoff->setValueNotifyingHost(0.91f);
    expect(processor.getPersistenceStatus().currentPresetDirty,
           "Host parameter changes must mark the active native sound dirty without a callback lock");
    expect(processor.loadLibraryPreset(presetId).wasOk(),
           "Complete local preset must prepare and publish transactionally");
    expect(std::abs(cutoff->getValue() - savedCutoff) <= 1.0e-6f,
           "Preset recall must restore the exact normalized host parameter value");
    expect(!processor.getPersistenceStatus().currentPresetDirty,
           "Transactional preset recall must establish a clean parameter revision");

    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer emptyMidi;
    processor.processBlock(audio, emptyMidi);

    midi::MusicIntent intent;
    intent.seed = 61001;
    intent.requestId = midi::deterministicUuid(intent.seed, "m6-processor-history");
    expect(processor.generateCompositionCandidate(intent).wasOk()
               && processor.acceptCompositionCandidate().wasOk(),
           "Explicit composition acceptance must remain successful while storing history");
    expect(processor.generateMoreLikeComposition(1).wasOk()
               && processor.acceptCompositionCandidate().wasOk(),
           "Accepted variation must store a second history record with lineage");
    const auto history = processor.searchHistory({"", false, false, 20});
    expect(history.status.wasOk() && history.entries.size() == 2,
           "Processor history search must return both accepted compositions");
    const auto hasLineage = std::any_of(history.entries.begin(), history.entries.end(),
        [](const auto& entry) { return !entry.parentId.isEmpty(); });
    expect(hasLineage, "More Like This acceptance must retain parent history lineage");

    PluginProcessor restarted({true, temporary.directory});
    expect(restarted.initialisePersistence().wasOk(),
           "A new processor must reopen the existing M6 persistence root");
    const auto reopenedHistory = restarted.searchHistory({"", false, false, 20});
    expect(reopenedHistory.status.wasOk() && reopenedHistory.entries.size() == 2,
           "Accepted history must survive processor destruction and restart");
    if (!reopenedHistory.entries.empty())
    {
        expect(restarted.recallHistory(reopenedHistory.entries.back().id).wasOk(),
               "Versioned history recall must restore its linked preset before composition state");
        const auto recalled = restarted.getCompositionSnapshot();
        expect(recalled.hasCandidate && recalled.hasAccepted && recalled.candidateMatchesAccepted,
               "History recall must explicitly restore an accepted, deliverable composition");
        expect(restarted.inspectHistory(reopenedHistory.entries.back().id).has_value(),
               "History comparison inspection must not mutate or lose the stored entry");
    }
}

void testPresetSaveAsAndExplicitOverwrite()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    PluginProcessor processor({true, temporary.directory});
    expect(processor.initialisePersistence().wasOk(),
           "Save As fixture must initialize its isolated preset root");
    PresetSaveRequest first;
    first.name = "First identity";
    expect(processor.saveCurrentPreset(first).wasOk(),
           "First native preset save must create a stable identity");
    const auto firstId = processor.getPersistenceStatus().currentPresetId;

    PresetSaveRequest second = first;
    second.name = "Second identity";
    expect(processor.saveCurrentPreset(second).wasOk(),
           "Save As without overwrite must create a new native preset");
    const auto secondId = processor.getPersistenceStatus().currentPresetId;
    expect(firstId != secondId && midi::isUuid(firstId) && midi::isUuid(secondId),
           "Save As must generate a distinct stable UUID");
    expect(processor.listPresets().presets.size() == 2,
           "Save As must retain both native preset documents");

    expect(processor.saveCurrentPreset(second).failed(),
           "A colliding safe filename must not overwrite implicitly");
    second.allowOverwrite = true;
    expect(processor.saveCurrentPreset(second).wasOk(),
           "Explicit overwrite must update the current stable preset identity");
    expect(processor.getPersistenceStatus().currentPresetId == secondId
               && processor.listPresets().presets.size() == 2,
           "Explicit overwrite must preserve the current UUID and library cardinality");
}

void testProjectStateRestoresAssetsAndAcceptedComposition()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    const auto sourceFile = temporary.directory.getChildFile("owned-project-wavetable.wav");
    expect(writeWavetableFixture(sourceFile),
           "Project-state fixture must create a user-owned WAV source");

    PluginProcessor source({true, temporary.directory});
    source.prepareToPlay(48000.0, 256);
    expect(source.initialisePersistence().wasOk(),
           "Project-state fixture must initialize its isolated persistence root");
    expect(source.requestWavetableImport(sourceFile, 0, 2048).wasOk()
               && waitForImportReview(source, 5000),
           "User-owned WAV must reach explicit import review");
    expect(source.confirmWavetableImport().wasOk(),
           "Confirmed user-owned WAV must publish and retain a content-addressed source");

    auto* cutoff = source.state().getParameter(parameterIds::filterCutoff);
    expect(cutoff != nullptr, "Project-state cutoff parameter must exist");
    if (cutoff == nullptr)
        return;
    cutoff->setValueNotifyingHost(0.314f);
    const auto expectedCutoff = cutoff->getValue();
    midi::MusicIntent intent;
    intent.seed = 63001;
    intent.requestId = midi::deterministicUuid(intent.seed, "m6-project-state");
    expect(source.generateCompositionCandidate(intent).wasOk()
               && source.acceptCompositionCandidate().wasOk(),
           "Project state must have an explicitly accepted composition to restore");
    const auto expectedWavetable = source.getWavetableUiSnapshot(0);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    expect(!state.isEmpty() && state.getSize() < 8U * 1024U * 1024U,
           "Versioned project state must remain non-empty and bounded");

    PluginProcessor restored({true, temporary.directory});
    restored.prepareToPlay(48000.0, 256);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto* restoredCutoff = restored.state().getParameter(parameterIds::filterCutoff);
    const auto restoredWavetable = restored.getWavetableUiSnapshot(0);
    expect(restoredCutoff != nullptr
               && std::abs(restoredCutoff->getValue() - expectedCutoff) <= 1.0e-6f,
           "Host project recall must restore the exact native preset parameters");
    expect(restoredWavetable.frameCount == expectedWavetable.frameCount
               && std::equal(restoredWavetable.samples.begin(), restoredWavetable.samples.end(),
                             expectedWavetable.samples.begin()),
           "Host project recall must reconstruct the imported oscillator wavetable");
    expect(restored.getCompositionSnapshot().hasAccepted
               && restored.getCompositionSnapshot().acceptedRequestId == intent.requestId,
           "Host project recall must restore the exact accepted composition without an editor");

    const auto stateXml = PluginProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    expect(stateXml != nullptr, "Bounded project state must reopen as JUCE XML");
    if (stateXml != nullptr)
    {
        auto malformedTree = juce::ValueTree::fromXml(*stateXml);
        auto session = malformedTree.getChildWithName("FolkParkProjectSession");
        const juce::MemoryBlock malformedComposition("{}", 2);
        session.setProperty("acceptedCompositionPayload", juce::var(malformedComposition), nullptr);
        juce::MemoryBlock malformedState;
        if (const auto malformedXml = malformedTree.createXml())
            PluginProcessor::copyXmlToBinary(*malformedXml, malformedState);
        PluginProcessor rejected({true, temporary.directory});
        auto* rejectedCutoff = rejected.state().getParameter(parameterIds::filterCutoff);
        if (rejectedCutoff != nullptr)
            rejectedCutoff->setValueNotifyingHost(0.88f);
        const auto beforeRejected = rejectedCutoff != nullptr ? rejectedCutoff->getValue() : -1.0f;
        rejected.setStateInformation(malformedState.getData(),
                                     static_cast<int>(malformedState.getSize()));
        expect(rejectedCutoff != nullptr
                   && std::abs(rejectedCutoff->getValue() - beforeRejected) <= 1.0e-7f
                   && !rejected.getCompositionSnapshot().hasAccepted,
               "Malformed custom project payload must reject the complete transaction");
        juce::MemoryBlock oversizedState;
        oversizedState.setSize(8U * 1024U * 1024U + 1U, true);
        rejected.setStateInformation(oversizedState.getData(),
                                     static_cast<int>(oversizedState.getSize()));
        expect(std::abs(rejectedCutoff->getValue() - beforeRejected) <= 1.0e-7f,
               "Oversized host project state must be rejected before parsing or mutation");
    }

    const auto assetsDirectory = temporary.directory.getChildFile("Presets").getChildFile("assets");
    const auto assets = assetsDirectory.findChildFiles(juce::File::findFiles, false, "*.wav");
    expect(assets.size() == 1, "Project-state fixture must retain one content-addressed WAV");
    if (assets.size() != 1)
        return;
    const auto recoveryFile = temporary.directory.getChildFile("matching-recovery.wav");
    expect(assets[0].copyFileTo(recoveryFile),
           "Missing-asset fixture must retain a matching explicit recovery source");
    expect(assets[0].deleteFile(),
           "Missing-asset fixture must remove only its isolated temporary stored asset");

    PluginProcessor missing({true, temporary.directory});
    missing.prepareToPlay(48000.0, 256);
    auto* missingCutoff = missing.state().getParameter(parameterIds::filterCutoff);
    const auto unchangedCutoff = missingCutoff != nullptr ? missingCutoff->getValue() : -1.0f;
    missing.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expect(missingCutoff != nullptr
               && std::abs(missingCutoff->getValue() - unchangedCutoff) <= 1.0e-7f
               && !missing.getCompositionSnapshot().hasAccepted,
           "Missing project asset must leave the complete live sound and composition unchanged");
    const auto missingStatus = missing.getPersistenceStatus();
    expect(missingStatus.missingAssets.size() == 1,
           "Missing project asset must remain visible for explicit recovery");
    if (!missingStatus.missingAssets.empty())
    {
        const auto wrongRecovery = temporary.directory.getChildFile("wrong-recovery.wav");
        expect(wrongRecovery.replaceWithText("not the requested wavetable"),
               "Wrong recovery fixture must be created inside the isolated test root");
        expect(missing.relinkPendingPresetAsset(missingStatus.missingAssets.front().slot,
                                                wrongRecovery).failed(),
               "Wrong hash-and-size relink must be rejected");
        expect(std::abs(missingCutoff->getValue() - unchangedCutoff) <= 1.0e-7f
                   && !missing.getCompositionSnapshot().hasAccepted
                   && missing.getPersistenceStatus().missingAssets.size() == 1,
               "Rejected relink must retain the pending transaction and unchanged live state");
        expect(missing.relinkPendingPresetAsset(missingStatus.missingAssets.front().slot,
                                                recoveryFile).wasOk(),
               "Exact hash-and-size relink must finish the pending project transaction");
        expect(std::abs(missingCutoff->getValue() - expectedCutoff) <= 1.0e-6f
                   && missing.getCompositionSnapshot().hasAccepted,
               "Successful relink must atomically finish sound and accepted-composition recall");
    }
}

void testConfirmedImportRetryAndExternalLocalization()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    const auto localRoot = temporary.directory.getChildFile("local-store");
    const auto retrySource = temporary.directory.getChildFile("retry-source.wav");
    expect(writeWavetableFixture(retrySource),
           "Confirmed-import retry fixture must create a user-owned WAV");
    PluginProcessor retrying({true, localRoot});
    retrying.prepareToPlay(48000.0, 256);
    expect(retrying.initialisePersistence().wasOk(),
           "Confirmed-import retry fixture must initialize local persistence");
    const auto builtIn = synth::WavetableBank::createBuiltIn();
    expect(builtIn != nullptr && retrying.publishWavetable(0, *builtIn),
           "Retry fixture must occupy oscillator A's next block-boundary exchange");
    expect(retrying.requestWavetableImport(retrySource, 0, 2048).wasOk()
               && waitForImportReview(retrying, 5000),
           "Retry fixture WAV must reach explicit confirmation");
    expect(retrying.confirmWavetableImport().failed()
               && retrying.getWavetableImportSnapshot().status
                    == synth::WavetableImportService::Status::awaitingConfirmation,
           "Busy audio publication must retain the reviewed import for retry");
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midiBuffer;
    retrying.processBlock(audio, midiBuffer);
    expect(retrying.confirmWavetableImport().wasOk()
               && retrying.getWavetableImportSnapshot().status
                    == synth::WavetableImportService::Status::loaded,
           "Reviewed import must succeed after the occupied exchange advances");

    const auto externalRoot = temporary.directory.getChildFile("external-preset");
    expect(externalRoot.createDirectory(),
           "External preset fixture directory must be isolated and created");
    const auto externalSource = temporary.directory.getChildFile("external-source.wav");
    expect(writeWavetableFixture(externalSource),
           "External preset fixture must create its user-owned WAV");
    persistence::AssetReference externalReference;
    expect(persistence::PresetAssetStore::importWavetableSource(
               externalSource, externalRoot, persistence::AssetSlot::oscillatorB,
               externalReference).wasOk(),
           "External fixture WAV must enter its own content-addressed asset root");
    const auto externalId = midi::deterministicUuid(64001, "m6-external-preset");
    auto externalDocument = persistence::makePresetTemplate(
        FOLK_PARK_VERSION, externalId, "Localized external sound");
    externalDocument.assets.push_back(externalReference);
    const auto externalPreset = externalRoot.getChildFile("localized.folkparkpreset");
    expect(persistence::PresetStore::save(externalDocument, externalPreset, false).wasOk(),
           "External native preset fixture must save deterministically");

    const auto importedRoot = temporary.directory.getChildFile("imported-store");
    PluginProcessor importing({true, importedRoot});
    importing.prepareToPlay(48000.0, 256);
    expect(importing.initialisePersistence().wasOk()
               && importing.importExternalPreset(externalPreset).wasOk(),
           "External preset must validate, localize its asset, save locally, and apply");
    const auto localizedAsset = importedRoot.getChildFile("Presets")
        .getChildFile(externalReference.relativePath);
    expect(localizedAsset.existsAsFile(),
           "External preset import must retain an independent content-addressed asset");
    importing.processBlock(audio, midiBuffer);
    expect(externalRoot.deleteRecursively(false),
           "External localization test must remove only its isolated source directory");
    expect(importing.loadLibraryPreset(externalId).wasOk(),
           "Localized preset must reload after its external source directory disappears");
}

void testHistorySymlinkFailureIsolation()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    const auto target = temporary.directory.getChildFile("unrelated-target.sqlite3");
    expect(target.replaceWithText("must never be opened as folk park history"),
           "History symlink fixture target must be created inside the temporary root");
    expect(target.createSymbolicLink(temporary.directory.getChildFile("history.sqlite3"), false),
           "History symlink fixture must create a database-path link");
    PluginProcessor processor({true, temporary.directory});
    expect(processor.initialisePersistence().wasOk(),
           "Unsafe history symlink must degrade history without failing preset storage");
    const auto status = processor.getPersistenceStatus();
    expect(status.presetAvailable && !status.historyAvailable
               && status.message.containsIgnoreCase("symbolic link"),
           "History symlink rejection must be visible and isolated from native presets");
    PresetSaveRequest save;
    save.name = "Preset despite unsafe history";
    expect(processor.saveCurrentPreset(save).wasOk(),
           "Unsafe history path must not block atomic native preset saving");
}

void testHistoryDatabaseFailureIsolation()
{
    using namespace folkpark;
    TemporaryDirectory temporary;
    const auto databaseBlocker = temporary.directory.getChildFile("history.sqlite3");
    expect(databaseBlocker.createDirectory(),
           "Database-unavailable fixture must reserve the SQLite path as a directory");
    PluginProcessor processor({true, temporary.directory});
    expect(processor.initialisePersistence().wasOk(),
           "Preset storage initialization must survive an unavailable history database");
    const auto status = processor.getPersistenceStatus();
    expect(status.presetAvailable && !status.historyAvailable,
           "Database failure must be visible without disabling native presets");
    midi::MusicIntent intent;
    intent.seed = 62001;
    intent.requestId = midi::deterministicUuid(intent.seed, "m6-history-failure");
    expect(processor.generateCompositionCandidate(intent).wasOk()
               && processor.acceptCompositionCandidate().wasOk(),
           "Database failure must never turn valid explicit composition acceptance into failure");
    expect(processor.searchHistory({"", false, false, 20}).status.failed(),
           "Unavailable database must return a typed history error");

    processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> audio(2, 512);
    auto midi = noteOnBuffer();
    processor.processBlock(audio, midi);
    expect(audio.getMagnitude(0, 0, audio.getNumSamples()) > 1.0e-7f,
           "Unavailable history database must not stop finite active audio");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseGui;
    testStateRoundTrip();
    testAssistantProposalAuditionAndAcceptance();
    testProcessorOfflineJarvisOrchestration();
    testHostAwareUndoRedo();
    testUiIndependenceAndPanic();
    testCompositionAcceptanceAndProcessorRouting();
    testPreviewKeyboardProcessorPath();
    testPresetAndHistoryRestartIntegration();
    testPresetSaveAsAndExplicitOverwrite();
    testProjectStateRestoresAssetsAndAcceptedComposition();
    testConfirmedImportRetryAndExternalLocalization();
    testHistorySymlinkFailureIsolation();
    testHistoryDatabaseFailureIsolation();

    if (failures == 0)
        std::cout << "PASS: M1–M6 processor paths plus M7 reversible assistant A/B recovery\n";
    return failures == 0 ? 0 : 1;
}
