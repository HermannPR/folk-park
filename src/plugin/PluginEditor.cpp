#include "PluginEditor.h"

#include <BinaryData.h>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace folkpark
{
namespace
{
juce::WebBrowserComponent::Resource makeResource(const void* data, int size, const char* mime)
{
    const auto* begin = static_cast<const std::byte*>(data);
    return {{begin, begin + size}, mime};
}

juce::String importStatusName(synth::WavetableImportService::Status status)
{
    using Status = synth::WavetableImportService::Status;
    switch (status)
    {
        case Status::idle: return "idle";
        case Status::processing: return "processing";
        case Status::awaitingConfirmation: return "awaiting-confirmation";
        case Status::loaded: return "loaded";
        case Status::failed: return "failed";
        case Status::cancelled: return "cancelled";
    }
    return "unknown";
}

juce::String renderStatusName(render::OfflinePreviewService::Status status)
{
    using Status = render::OfflinePreviewService::Status;
    switch (status)
    {
        case Status::idle: return "idle";
        case Status::rendering: return "rendering";
        case Status::rendered: return "rendered";
        case Status::failed: return "failed";
        case Status::cancelled: return "cancelled";
    }
    return "unknown";
}

bool boundedNumber(const juce::var& value, double minimum, double maximum, double& output) noexcept
{
    if (!value.isInt() && !value.isInt64() && !value.isDouble())
        return false;
    output = static_cast<double>(value);
    return std::isfinite(output) && output >= minimum && output <= maximum;
}

bool strictBoolean(const juce::var& value, bool& output) noexcept
{
    if (!value.isBool())
        return false;
    output = static_cast<bool>(value);
    return true;
}

juce::var compositionPayload(const PluginProcessor& processor)
{
    const auto session = processor.getCompositionSnapshot();
    const auto preview = processor.getCompositionPreview();
    auto payload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    payload->setProperty("ok", session.hasCandidate);
    payload->setProperty("status", session.status);
    payload->setProperty("hasCandidate", session.hasCandidate);
    payload->setProperty("hasAccepted", session.hasAccepted);
    payload->setProperty("candidateMatchesAccepted", session.candidateMatchesAccepted);
    payload->setProperty("candidateClips", session.candidateClipCount);
    payload->setProperty("candidateNotes", session.candidateNoteCount);
    payload->setProperty("acceptedNotes", session.acceptedNoteCount);
    payload->setProperty("directPlaying", processor.isDirectMidiPlaying());
    payload->setProperty("previewTruncated", preview.truncated);

    juce::Array<juce::var> notes;
    notes.ensureStorageAllocated(static_cast<int>(preview.notes.size()));
    for (const auto& note : preview.notes)
    {
        auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
        object->setProperty("part", midi::stableId(note.part));
        object->setProperty("start", note.normalisedStart);
        object->setProperty("duration", note.normalisedDuration);
        object->setProperty("pitch", note.normalisedPitch);
        object->setProperty("velocity", note.normalisedVelocity);
        notes.add(juce::var(object.get()));
    }
    payload->setProperty("notes", juce::var(notes));
    return juce::var(payload.get());
}

juce::var wavetablePayload(const WavetableUiSnapshot& snapshot)
{
    auto payload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    payload->setProperty("frameCount", snapshot.frameCount);
    payload->setProperty("samplesPerFrame", WavetableUiSnapshot::samplesPerFrame);
    juce::Array<juce::var> samples;
    const auto count = juce::jlimit(0,
        WavetableUiSnapshot::maximumFrames * WavetableUiSnapshot::samplesPerFrame,
        snapshot.frameCount * WavetableUiSnapshot::samplesPerFrame);
    samples.ensureStorageAllocated(count);
    for (auto index = 0; index < count; ++index)
        samples.add(snapshot.samples[static_cast<std::size_t>(index)]);
    payload->setProperty("samples", juce::var(samples));
    return juce::var(payload.get());
}

juce::var modulationPayload(const synth::ModulationSnapshot& snapshot)
{
    juce::Array<juce::var> routes;
    routes.ensureStorageAllocated(static_cast<int>(snapshot.routeCount));
    for (std::size_t index = 0; index < snapshot.routeCount; ++index)
    {
        const auto& route = snapshot.routes[index];
        auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
        object->setProperty("source", static_cast<int>(route.source));
        object->setProperty("destination", static_cast<int>(route.destination));
        object->setProperty("amount", route.amount);
        object->setProperty("curve", static_cast<int>(route.curve));
        object->setProperty("enabled", route.enabled);
        routes.add(juce::var(object.get()));
    }
    return juce::var(routes);
}

juce::var completeUiSnapshot(PluginProcessor& processor)
{
    const auto import = processor.getWavetableImportSnapshot();
    const auto routes = processor.getConfiguredModulationRoutes();
    const auto rendered = processor.getAcceptedWavRenderSnapshot();
    auto snapshot = juce::DynamicObject::Ptr(new juce::DynamicObject());
    snapshot->setProperty("schemaVersion", 1);
    snapshot->setProperty("product", "folk park");
    snapshot->setProperty("version", FOLK_PARK_VERSION);
    snapshot->setProperty("architecture", "x86_64");
    snapshot->setProperty("activeVoices", processor.getActiveVoiceCount());
    snapshot->setProperty("importStatus", importStatusName(import.status));
    snapshot->setProperty("importMessage", import.message);
    snapshot->setProperty("renderStatus", renderStatusName(rendered.status));
    snapshot->setProperty("renderMessage", rendered.message);
    snapshot->setProperty("renderDestination", rendered.destination);
    snapshot->setProperty("renderDuration", rendered.durationSeconds);
    snapshot->setProperty("modulationRouteCount", static_cast<int>(routes.routeCount));
    snapshot->setProperty("modulationRoutes", modulationPayload(routes));
    snapshot->setProperty("composition", compositionPayload(processor));
    snapshot->setProperty("wavetableA", wavetablePayload(processor.getWavetableUiSnapshot(0)));
    snapshot->setProperty("wavetableB", wavetablePayload(processor.getWavetableUiSnapshot(1)));

    juce::Array<juce::var> parameters;
    parameters.ensureStorageAllocated(processor.getParameters().size());
    for (const auto* parameter : processor.getParameters())
    {
        const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter);
        if (identified == nullptr)
            continue;
        auto entry = juce::DynamicObject::Ptr(new juce::DynamicObject());
        entry->setProperty("id", identified->paramID);
        entry->setProperty("normalized", parameter->getValue());
        parameters.add(juce::var(entry.get()));
    }
    snapshot->setProperty("parameters", juce::var(parameters));
    return juce::var(snapshot.get());
}
}

class PluginEditor::LocalBrowser final : public juce::WebBrowserComponent
{
public:
    using WebBrowserComponent::WebBrowserComponent;

    bool pageAboutToLoad(const juce::String& url) override
    {
        return url.startsWith(getResourceProviderRoot());
    }
};

class PluginEditor::MidiDragButton final : public juce::TextButton
{
public:
    explicit MidiDragButton(PluginProcessor& owner)
        : TextButton("Generate and accept MIDI before dragging"), processor(owner)
    {
        setEnabled(false);
    }

    void updateAvailability(bool available)
    {
        setEnabled(available);
        setButtonText(available ? "Drag accepted M3 MIDI into FL Studio"
                                : "Generate and accept MIDI before dragging");
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStarted = false;
        TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragStarted && event.getDistanceFromDragStart() > 4)
        {
            temporaryFile = processor.writeAcceptedMidiToTemporaryFile();
            if (temporaryFile.existsAsFile())
            {
                dragStarted = juce::DragAndDropContainer::performExternalDragDropOfFiles(
                    {temporaryFile.getFullPathName()}, false, this);
            }
        }
        TextButton::mouseDrag(event);
    }

private:
    PluginProcessor& processor;
    juce::File temporaryFile;
    bool dragStarted = false;
};

PluginEditor::PluginEditor(PluginProcessor& owner)
    : AudioProcessorEditor(&owner),
      ownerProcessor(owner),
      masterGainAttachment(*owner.state().getParameter(parameterIds::masterGain),
                           masterGainRelay,
                           owner.state().undoManager),
      cutoffAttachment(*owner.state().getParameter(parameterIds::filterCutoff),
                       cutoffRelay,
                       owner.state().undoManager),
      attackAttachment(*owner.state().getParameter(parameterIds::ampAttack),
                       attackRelay,
                       owner.state().undoManager),
      releaseAttachment(*owner.state().getParameter(parameterIds::ampRelease),
                        releaseRelay,
                        owner.state().undoManager),
      oscillatorAPositionAttachment(*owner.state().getParameter(parameterIds::oscillatorAPosition),
                                    oscillatorAPositionRelay,
                                    owner.state().undoManager),
      oscillatorBPositionAttachment(*owner.state().getParameter(parameterIds::oscillatorBPosition),
                                    oscillatorBPositionRelay,
                                    owner.state().undoManager),
      oscillatorBLevelAttachment(*owner.state().getParameter(parameterIds::oscillatorBLevel),
                                 oscillatorBLevelRelay,
                                 owner.state().undoManager),
      filterResonanceAttachment(*owner.state().getParameter(parameterIds::filterResonance),
                                filterResonanceRelay,
                                owner.state().undoManager),
      filterDriveAttachment(*owner.state().getParameter(parameterIds::filterDrive),
                            filterDriveRelay,
                            owner.state().undoManager),
      lfo1RateAttachment(*owner.state().getParameter(parameterIds::lfoRate[0]),
                         lfo1RateRelay,
                         owner.state().undoManager),
      waveformAttachment(*owner.state().getParameter(parameterIds::oscillatorWaveform),
                         waveformRelay,
                         owner.state().undoManager),
      filterModeAttachment(*owner.state().getParameter(parameterIds::filterMode),
                           filterModeRelay,
                           owner.state().undoManager),
      oscillatorALevelAttachment(*owner.state().getParameter(parameterIds::oscillatorLevel), oscillatorALevelRelay, owner.state().undoManager),
      subLevelAttachment(*owner.state().getParameter(parameterIds::subLevel), subLevelRelay, owner.state().undoManager),
      noiseLevelAttachment(*owner.state().getParameter(parameterIds::noiseLevel), noiseLevelRelay, owner.state().undoManager),
      ampDecayAttachment(*owner.state().getParameter(parameterIds::ampDecay), ampDecayRelay, owner.state().undoManager),
      ampSustainAttachment(*owner.state().getParameter(parameterIds::ampSustain), ampSustainRelay, owner.state().undoManager),
      filterKeyTrackingAttachment(*owner.state().getParameter(parameterIds::filterKeyTracking), filterKeyTrackingRelay, owner.state().undoManager),
      filterEnvelopeAmountAttachment(*owner.state().getParameter(parameterIds::filterEnvelopeAmount), filterEnvelopeAmountRelay, owner.state().undoManager),
      filterEnvelopeAttackAttachment(*owner.state().getParameter(parameterIds::filterEnvelopeAttack), filterEnvelopeAttackRelay, owner.state().undoManager),
      filterEnvelopeDecayAttachment(*owner.state().getParameter(parameterIds::filterEnvelopeDecay), filterEnvelopeDecayRelay, owner.state().undoManager),
      filterEnvelopeSustainAttachment(*owner.state().getParameter(parameterIds::filterEnvelopeSustain), filterEnvelopeSustainRelay, owner.state().undoManager),
      filterEnvelopeReleaseAttachment(*owner.state().getParameter(parameterIds::filterEnvelopeRelease), filterEnvelopeReleaseRelay, owner.state().undoManager),
      auxiliaryEnvelopeAttackAttachment(*owner.state().getParameter(parameterIds::auxiliaryEnvelopeAttack), auxiliaryEnvelopeAttackRelay, owner.state().undoManager),
      auxiliaryEnvelopeDecayAttachment(*owner.state().getParameter(parameterIds::auxiliaryEnvelopeDecay), auxiliaryEnvelopeDecayRelay, owner.state().undoManager),
      auxiliaryEnvelopeSustainAttachment(*owner.state().getParameter(parameterIds::auxiliaryEnvelopeSustain), auxiliaryEnvelopeSustainRelay, owner.state().undoManager),
      auxiliaryEnvelopeReleaseAttachment(*owner.state().getParameter(parameterIds::auxiliaryEnvelopeRelease), auxiliaryEnvelopeReleaseRelay, owner.state().undoManager),
      lfo2RateAttachment(*owner.state().getParameter(parameterIds::lfoRate[1]), lfo2RateRelay, owner.state().undoManager),
      lfo3RateAttachment(*owner.state().getParameter(parameterIds::lfoRate[2]), lfo3RateRelay, owner.state().undoManager),
      lfo4RateAttachment(*owner.state().getParameter(parameterIds::lfoRate[3]), lfo4RateRelay, owner.state().undoManager),
      subWaveformAttachment(*owner.state().getParameter(parameterIds::subWaveform), subWaveformRelay, owner.state().undoManager),
      noiseTypeAttachment(*owner.state().getParameter(parameterIds::noiseType), noiseTypeRelay, owner.state().undoManager),
      lfo1ShapeAttachment(*owner.state().getParameter(parameterIds::lfoShape[0]), lfo1ShapeRelay, owner.state().undoManager),
      lfo2ShapeAttachment(*owner.state().getParameter(parameterIds::lfoShape[1]), lfo2ShapeRelay, owner.state().undoManager),
      lfo3ShapeAttachment(*owner.state().getParameter(parameterIds::lfoShape[2]), lfo3ShapeRelay, owner.state().undoManager),
      lfo4ShapeAttachment(*owner.state().getParameter(parameterIds::lfoShape[3]), lfo4ShapeRelay, owner.state().undoManager),
      distortionBypassAttachment(*owner.state().getParameter(parameterIds::distortionBypass), distortionBypassRelay, owner.state().undoManager),
      distortionDriveAttachment(*owner.state().getParameter(parameterIds::distortionDrive), distortionDriveRelay, owner.state().undoManager),
      distortionMixAttachment(*owner.state().getParameter(parameterIds::distortionMix), distortionMixRelay, owner.state().undoManager),
      distortionOutputAttachment(*owner.state().getParameter(parameterIds::distortionOutput), distortionOutputRelay, owner.state().undoManager),
      chorusBypassAttachment(*owner.state().getParameter(parameterIds::chorusBypass), chorusBypassRelay, owner.state().undoManager),
      chorusRateAttachment(*owner.state().getParameter(parameterIds::chorusRate), chorusRateRelay, owner.state().undoManager),
      chorusDepthAttachment(*owner.state().getParameter(parameterIds::chorusDepth), chorusDepthRelay, owner.state().undoManager),
      chorusMixAttachment(*owner.state().getParameter(parameterIds::chorusMix), chorusMixRelay, owner.state().undoManager),
      delayBypassAttachment(*owner.state().getParameter(parameterIds::delayBypass), delayBypassRelay, owner.state().undoManager),
      delayDivisionAttachment(*owner.state().getParameter(parameterIds::delayDivision), delayDivisionRelay, owner.state().undoManager),
      delayFeedbackAttachment(*owner.state().getParameter(parameterIds::delayFeedback), delayFeedbackRelay, owner.state().undoManager),
      delayMixAttachment(*owner.state().getParameter(parameterIds::delayMix), delayMixRelay, owner.state().undoManager),
      reverbBypassAttachment(*owner.state().getParameter(parameterIds::reverbBypass), reverbBypassRelay, owner.state().undoManager),
      reverbRoomSizeAttachment(*owner.state().getParameter(parameterIds::reverbRoomSize), reverbRoomSizeRelay, owner.state().undoManager),
      reverbDampingAttachment(*owner.state().getParameter(parameterIds::reverbDamping), reverbDampingRelay, owner.state().undoManager),
      reverbMixAttachment(*owner.state().getParameter(parameterIds::reverbMix), reverbMixRelay, owner.state().undoManager),
      compressorBypassAttachment(*owner.state().getParameter(parameterIds::compressorBypass), compressorBypassRelay, owner.state().undoManager),
      compressorThresholdAttachment(*owner.state().getParameter(parameterIds::compressorThreshold), compressorThresholdRelay, owner.state().undoManager),
      compressorRatioAttachment(*owner.state().getParameter(parameterIds::compressorRatio), compressorRatioRelay, owner.state().undoManager),
      compressorAttackAttachment(*owner.state().getParameter(parameterIds::compressorAttack), compressorAttackRelay, owner.state().undoManager),
      compressorReleaseAttachment(*owner.state().getParameter(parameterIds::compressorRelease), compressorReleaseRelay, owner.state().undoManager),
      compressorMakeupAttachment(*owner.state().getParameter(parameterIds::compressorMakeup), compressorMakeupRelay, owner.state().undoManager),
      compressorMixAttachment(*owner.state().getParameter(parameterIds::compressorMix), compressorMixRelay, owner.state().undoManager),
      eqBypassAttachment(*owner.state().getParameter(parameterIds::eqBypass), eqBypassRelay, owner.state().undoManager),
      eqLowGainAttachment(*owner.state().getParameter(parameterIds::eqLowGain), eqLowGainRelay, owner.state().undoManager),
      eqMidFrequencyAttachment(*owner.state().getParameter(parameterIds::eqMidFrequency), eqMidFrequencyRelay, owner.state().undoManager),
      eqMidGainAttachment(*owner.state().getParameter(parameterIds::eqMidGain), eqMidGainRelay, owner.state().undoManager),
      eqMidQAttachment(*owner.state().getParameter(parameterIds::eqMidQ), eqMidQRelay, owner.state().undoManager),
      eqHighGainAttachment(*owner.state().getParameter(parameterIds::eqHighGain), eqHighGainRelay, owner.state().undoManager)
{
    fallback.setText("folk park M2 - native fallback editor", juce::dontSendNotification);
    fallback.setJustificationType(juce::Justification::centred);
    fallback.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(fallback);

    const auto options = browserOptions();
    if (juce::WebBrowserComponent::areOptionsSupported(options))
    {
        browser = std::make_unique<LocalBrowser>(options);
        addAndMakeVisible(*browser);
        browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    }

    midiDrag = std::make_unique<MidiDragButton>(ownerProcessor);
    addAndMakeVisible(*midiDrag);

    setResizable(true, true);
    setResizeLimits(720, 560, 1600, 1100);
    setSize(1180, 900);
    startTimerHz(5);
}

PluginEditor::~PluginEditor()
{
    ownerProcessor.releasePreviewNotes();
    stopTimer();
}

juce::WebBrowserComponent::Options PluginEditor::browserOptions()
{
    return juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withOptionsFrom(masterGainRelay)
        .withOptionsFrom(cutoffRelay)
        .withOptionsFrom(attackRelay)
        .withOptionsFrom(releaseRelay)
        .withOptionsFrom(oscillatorAPositionRelay)
        .withOptionsFrom(oscillatorBPositionRelay)
        .withOptionsFrom(oscillatorBLevelRelay)
        .withOptionsFrom(filterResonanceRelay)
        .withOptionsFrom(filterDriveRelay)
        .withOptionsFrom(lfo1RateRelay)
        .withOptionsFrom(waveformRelay)
        .withOptionsFrom(filterModeRelay)
        .withOptionsFrom(oscillatorALevelRelay)
        .withOptionsFrom(subLevelRelay)
        .withOptionsFrom(noiseLevelRelay)
        .withOptionsFrom(ampDecayRelay)
        .withOptionsFrom(ampSustainRelay)
        .withOptionsFrom(filterKeyTrackingRelay)
        .withOptionsFrom(filterEnvelopeAmountRelay)
        .withOptionsFrom(filterEnvelopeAttackRelay)
        .withOptionsFrom(filterEnvelopeDecayRelay)
        .withOptionsFrom(filterEnvelopeSustainRelay)
        .withOptionsFrom(filterEnvelopeReleaseRelay)
        .withOptionsFrom(auxiliaryEnvelopeAttackRelay)
        .withOptionsFrom(auxiliaryEnvelopeDecayRelay)
        .withOptionsFrom(auxiliaryEnvelopeSustainRelay)
        .withOptionsFrom(auxiliaryEnvelopeReleaseRelay)
        .withOptionsFrom(lfo2RateRelay)
        .withOptionsFrom(lfo3RateRelay)
        .withOptionsFrom(lfo4RateRelay)
        .withOptionsFrom(subWaveformRelay)
        .withOptionsFrom(noiseTypeRelay)
        .withOptionsFrom(lfo1ShapeRelay)
        .withOptionsFrom(lfo2ShapeRelay)
        .withOptionsFrom(lfo3ShapeRelay)
        .withOptionsFrom(lfo4ShapeRelay)
        .withOptionsFrom(distortionBypassRelay)
        .withOptionsFrom(distortionDriveRelay)
        .withOptionsFrom(distortionMixRelay)
        .withOptionsFrom(distortionOutputRelay)
        .withOptionsFrom(chorusBypassRelay)
        .withOptionsFrom(chorusRateRelay)
        .withOptionsFrom(chorusDepthRelay)
        .withOptionsFrom(chorusMixRelay)
        .withOptionsFrom(delayBypassRelay)
        .withOptionsFrom(delayDivisionRelay)
        .withOptionsFrom(delayFeedbackRelay)
        .withOptionsFrom(delayMixRelay)
        .withOptionsFrom(reverbBypassRelay)
        .withOptionsFrom(reverbRoomSizeRelay)
        .withOptionsFrom(reverbDampingRelay)
        .withOptionsFrom(reverbMixRelay)
        .withOptionsFrom(compressorBypassRelay)
        .withOptionsFrom(compressorThresholdRelay)
        .withOptionsFrom(compressorRatioRelay)
        .withOptionsFrom(compressorAttackRelay)
        .withOptionsFrom(compressorReleaseRelay)
        .withOptionsFrom(compressorMakeupRelay)
        .withOptionsFrom(compressorMixRelay)
        .withOptionsFrom(eqBypassRelay)
        .withOptionsFrom(eqLowGainRelay)
        .withOptionsFrom(eqMidFrequencyRelay)
        .withOptionsFrom(eqMidGainRelay)
        .withOptionsFrom(eqMidQRelay)
        .withOptionsFrom(eqHighGainRelay)
        .withNativeFunction("getUiSnapshot", [this](const auto&, auto complete)
        {
            complete(completeUiSnapshot(ownerProcessor));
        })
        .withNativeFunction("getProductInfo", [](const auto&, auto complete)
        {
            auto info = juce::DynamicObject::Ptr(new juce::DynamicObject());
            info->setProperty("product", "folk park");
            info->setProperty("version", FOLK_PARK_VERSION);
            info->setProperty("architecture", "x86_64");
            complete(juce::var(info.get()));
        })
        .withNativeFunction("panic", [this](const auto&, auto complete)
        {
            ownerProcessor.requestPanic();
            complete("Panic queued safely for the next audio block");
        })
        .withNativeFunction("undo", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty()) { complete("Undo takes no arguments"); return; }
            complete(ownerProcessor.undoLastParameterChange()
                         ? juce::var("Last parameter gesture undone")
                         : juce::var("Nothing to undo"));
        })
        .withNativeFunction("redo", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty()) { complete("Redo takes no arguments"); return; }
            complete(ownerProcessor.redoLastParameterChange()
                         ? juce::var("Last undone parameter gesture restored")
                         : juce::var("Nothing to redo"));
        })
        .withNativeFunction("previewNoteOn", [this](const auto& arguments, auto complete)
        {
            double note = 0.0;
            double velocity = 0.0;
            if (arguments.size() != 2 || !boundedNumber(arguments[0], 0.0, 127.0, note)
                || !boundedNumber(arguments[1], 1.0, 127.0, velocity)
                || std::floor(note) != note || std::floor(velocity) != velocity)
            {
                complete("Preview note-on requires integer note 0–127 and velocity 1–127");
                return;
            }
            complete(ownerProcessor.previewNoteOn(static_cast<int>(note), static_cast<int>(velocity))
                         ? juce::var(true) : juce::var("Preview MIDI queue is full"));
        })
        .withNativeFunction("previewNoteOff", [this](const auto& arguments, auto complete)
        {
            double note = 0.0;
            if (arguments.size() != 1 || !boundedNumber(arguments[0], 0.0, 127.0, note)
                || std::floor(note) != note)
            {
                complete("Preview note-off requires an integer note from 0–127");
                return;
            }
            complete(ownerProcessor.previewNoteOff(static_cast<int>(note))
                         ? juce::var(true) : juce::var("Preview release-all safety was requested"));
        })
        .withNativeFunction("releasePreviewNotes", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty())
            {
                complete("Release preview notes takes no arguments");
                return;
            }
            ownerProcessor.releasePreviewNotes();
            complete(true);
        })
        .withNativeFunction("chooseWavetable", [this](const auto& arguments, auto finish)
        {
            if (arguments.size() != 1)
            {
                finish("Choose wavetable requires oscillator 0 or 1");
                return;
            }

            const auto oscillatorIndex = static_cast<int>(arguments[0]);
            if (oscillatorIndex < 0 || oscillatorIndex > 1)
            {
                finish("Invalid oscillator target");
                return;
            }
            if (wavetableChooserActive)
            {
                finish("A wavetable chooser is already open");
                return;
            }

            wavetableChooserActive = true;
            wavetableChooser = std::make_unique<juce::FileChooser>(
                "Choose a WAV wavetable to review", juce::File{}, "*.wav");
            const auto safeEditor = juce::Component::SafePointer<PluginEditor>(this);
            wavetableChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeEditor, oscillatorIndex, completionHandler = std::move(finish)](
                    const juce::FileChooser& chooser) mutable
                {
                    if (safeEditor == nullptr)
                        return;
                    safeEditor->wavetableChooserActive = false;
                    const auto file = chooser.getResult();
                    if (file == juce::File{})
                    {
                        completionHandler("Wavetable selection cancelled");
                        return;
                    }
                    const auto result = safeEditor->ownerProcessor.requestWavetableImport(
                        file, oscillatorIndex);
                    completionHandler(result.wasOk() ? juce::String("Conversion queued for review")
                                                     : result.getErrorMessage());
                });
        })
        .withNativeFunction("confirmWavetableImport", [this](const auto&, auto complete)
        {
            const auto result = ownerProcessor.confirmWavetableImport();
            complete(result.wasOk() ? juce::String("Wavetable published with a click-safe crossfade")
                                    : result.getErrorMessage());
        })
        .withNativeFunction("cancelWavetableImport", [this](const auto&, auto complete)
        {
            ownerProcessor.cancelWavetableImport();
            complete("Pending wavetable import cancelled");
        })
        .withNativeFunction("setModulationRoute", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 5)
            {
                complete("A modulation route requires source, destination, amount, curve, and enabled");
                return;
            }
            synth::ModulationRoute route;
            route.source = static_cast<synth::ModulationSource>(static_cast<int>(arguments[0]));
            route.destination = static_cast<synth::ModulationDestination>(static_cast<int>(arguments[1]));
            route.amount = static_cast<float>(static_cast<double>(arguments[2]));
            route.curve = static_cast<synth::ModulationCurve>(static_cast<int>(arguments[3]));
            route.enabled = static_cast<bool>(arguments[4]);
            const auto result = ownerProcessor.setModulationRoutes(std::span{&route, 1});
            complete(result.wasOk() ? juce::String("One bounded modulation route applied")
                                    : result.getErrorMessage());
        })
        .withNativeFunction("setModulationRoutes", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 1 || !arguments[0].isArray())
            {
                complete("Modulation routes require one bounded array");
                return;
            }
            const auto* input = arguments[0].getArray();
            if (input == nullptr || input->size() > static_cast<int>(synth::ModulationSnapshot::maximumRoutes))
            {
                complete("Modulation route array exceeds 32 entries");
                return;
            }
            std::array<synth::ModulationRoute, synth::ModulationSnapshot::maximumRoutes> parsed{};
            for (int index = 0; index < input->size(); ++index)
            {
                const auto* object = (*input)[index].getDynamicObject();
                if (object == nullptr || !object->hasProperty("source")
                    || !object->hasProperty("destination") || !object->hasProperty("amount")
                    || !object->hasProperty("curve") || !object->hasProperty("enabled"))
                {
                    complete("Every modulation route requires source, destination, amount, curve, and enabled");
                    return;
                }
                double source = 0.0;
                double destination = 0.0;
                double amount = 0.0;
                double curve = 0.0;
                bool enabled = false;
                if (!boundedNumber(object->getProperty("source"), 0.0, 9.0, source)
                    || !boundedNumber(object->getProperty("destination"), 0.0, 12.0, destination)
                    || !boundedNumber(object->getProperty("amount"), -1.0, 1.0, amount)
                    || !boundedNumber(object->getProperty("curve"), 0.0, 2.0, curve)
                    || !strictBoolean(object->getProperty("enabled"), enabled)
                    || std::floor(source) != source || std::floor(destination) != destination
                    || std::floor(curve) != curve)
                {
                    complete("Modulation route fields are malformed or outside their bounds");
                    return;
                }
                parsed[static_cast<std::size_t>(index)] = {
                    static_cast<synth::ModulationSource>(static_cast<int>(source)),
                    static_cast<synth::ModulationDestination>(static_cast<int>(destination)),
                    static_cast<float>(amount),
                    static_cast<synth::ModulationCurve>(static_cast<int>(curve)), enabled};
            }
            const auto span = std::span{parsed}.first(static_cast<std::size_t>(input->size()));
            const auto result = ownerProcessor.setModulationRoutes(span);
            complete(result.wasOk() ? completeUiSnapshot(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("clearModulationRoutes", [this](const auto&, auto complete)
        {
            const auto result = ownerProcessor.setModulationRoutes({});
            complete(result.wasOk() ? juce::String("Modulation routes cleared")
                                    : result.getErrorMessage());
        })
        .withNativeFunction("generateComposition", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 15)
            {
                complete("Composition generation requires 15 bounded intent fields");
                return;
            }
            double seed = 0.0;
            double tempo = 0.0;
            double bars = 0.0;
            std::array<double, 6> macros{};
            std::array<bool, 4> requestedParts{};
            if (!boundedNumber(arguments[0], 0.0,
                               static_cast<double>(std::numeric_limits<std::uint32_t>::max()), seed)
                || !arguments[1].isString() || !arguments[2].isString()
                || !boundedNumber(arguments[3], 20.0, 400.0, tempo)
                || !boundedNumber(arguments[4], 1.0, 64.0, bars))
            {
                complete("Composition seed, key, scale, tempo, or bars are invalid");
                return;
            }
            for (std::size_t index = 0; index < macros.size(); ++index)
            {
                if (!boundedNumber(arguments[static_cast<int>(5 + index)],
                                   0.0, 1.0, macros[index]))
                {
                    complete("Every composition macro must be a number from 0 to 1");
                    return;
                }
            }
            for (std::size_t index = 0; index < requestedParts.size(); ++index)
            {
                if (!strictBoolean(arguments[static_cast<int>(11 + index)],
                                   requestedParts[index]))
                {
                    complete("Every requested composition part must be true or false");
                    return;
                }
            }

            const auto key = midi::parseKeyRoot(arguments[1].toString());
            const auto scale = midi::parseScaleType(arguments[2].toString());
            if (!key.has_value() || !scale.has_value())
            {
                complete("Unsupported key or scale");
                return;
            }
            midi::MusicIntent intent;
            intent.seed = static_cast<std::uint32_t>(seed);
            intent.requestId = midi::deterministicUuid(intent.seed, "ui-composition-request");
            intent.key = *key;
            intent.scale = *scale;
            intent.tempoBpm = tempo;
            intent.lengthBars = static_cast<int>(bars);
            intent.density = static_cast<float>(macros[0]);
            intent.rhythmComplexity = static_cast<float>(macros[1]);
            intent.tension = static_cast<float>(macros[2]);
            intent.humanization = static_cast<float>(macros[3]);
            intent.repetition = static_cast<float>(macros[4]);
            intent.variation = static_cast<float>(macros[5]);
            intent.partCount = 0;
            constexpr std::array parts{midi::PartType::chords, midi::PartType::melody,
                                       midi::PartType::bass, midi::PartType::arp};
            for (std::size_t index = 0; index < requestedParts.size(); ++index)
                if (requestedParts[index])
                    intent.parts[intent.partCount++] = parts[index];
            if (intent.partCount == 0)
            {
                complete("Select at least one composition part");
                return;
            }
            const auto result = ownerProcessor.generateCompositionCandidate(std::move(intent));
            complete(result.wasOk() ? compositionPayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("moreLikeComposition", [this](const auto& arguments, auto complete)
        {
            double index = 0.0;
            if (arguments.size() != 1 || !boundedNumber(arguments[0], 1.0, 4294967295.0, index))
            {
                complete("More Like This requires a positive bounded variation index");
                return;
            }
            const auto result = ownerProcessor.generateMoreLikeComposition(
                static_cast<std::uint32_t>(index));
            complete(result.wasOk() ? compositionPayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("surpriseComposition", [this](const auto& arguments, auto complete)
        {
            double index = 0.0;
            if (arguments.size() != 1 || !boundedNumber(arguments[0], 1.0, 4294967295.0, index))
            {
                complete("Surprise Me requires a positive bounded surprise index");
                return;
            }
            const auto result = ownerProcessor.generateSurpriseComposition(
                static_cast<std::uint32_t>(index));
            complete(result.wasOk() ? compositionPayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("editCompositionNote", [this](const auto& arguments, auto complete)
        {
            std::array<double, 5> values{};
            constexpr std::array minimums{0.0, -24.0, -3840.0, -3840.0, -127.0};
            constexpr std::array maximums{4095.0, 24.0, 3840.0, 3840.0, 127.0};
            if (arguments.size() != static_cast<int>(values.size()))
            {
                complete("A note edit requires index, pitch, start, duration, and velocity deltas");
                return;
            }
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (!boundedNumber(arguments[static_cast<int>(index)], minimums[index],
                                   maximums[index], values[index])
                    || std::floor(values[index]) != values[index])
                {
                    complete("Every note edit field must be a bounded integer");
                    return;
                }
            }
            const auto result = ownerProcessor.adjustCompositionCandidateNote(
                static_cast<std::size_t>(values[0]), static_cast<int>(values[1]),
                static_cast<std::int64_t>(values[2]), static_cast<std::int64_t>(values[3]),
                static_cast<int>(values[4]));
            complete(result.wasOk() ? compositionPayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("acceptComposition", [this](const auto&, auto complete)
        {
            const auto result = ownerProcessor.acceptCompositionCandidate();
            complete(result.wasOk() ? compositionPayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("getComposition", [this](const auto&, auto complete)
        {
            complete(compositionPayload(ownerProcessor));
        })
        .withNativeFunction("routeAcceptedMidi", [this](const auto&, auto complete)
        {
            const auto result = ownerProcessor.routeAcceptedMidi();
            complete(result.wasOk() ? juce::String("Accepted MIDI starts on the next audio block")
                                    : result.getErrorMessage());
        })
        .withNativeFunction("stopDirectMidi", [this](const auto&, auto complete)
        {
            ownerProcessor.stopDirectMidi();
            complete("Direct MIDI Stop queued with tracked note-offs");
        })
        .withNativeFunction("exportAcceptedMidi", [this](const auto&, auto finish)
        {
            if (!ownerProcessor.getCompositionSnapshot().hasAccepted)
            {
                finish("Accept a composition before exporting MIDI");
                return;
            }
            if (midiExportChooserActive)
            {
                finish("A MIDI export chooser is already open");
                return;
            }
            midiExportChooserActive = true;
            midiExportChooser = std::make_unique<juce::FileChooser>(
                "Export accepted folk park MIDI",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("folk-park.mid"),
                "*.mid");
            const auto safeEditor = juce::Component::SafePointer<PluginEditor>(this);
            midiExportChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::warnAboutOverwriting,
                [safeEditor, completionHandler = std::move(finish)](
                    const juce::FileChooser& chooser) mutable
                {
                    if (safeEditor == nullptr)
                        return;
                    safeEditor->midiExportChooserActive = false;
                    auto file = chooser.getResult();
                    if (file == juce::File{})
                    {
                        completionHandler("MIDI export cancelled");
                        return;
                    }
                    if (file.getFileExtension().isEmpty())
                        file = file.withFileExtension(".mid");
                    const auto result = safeEditor->ownerProcessor.writeAcceptedMidiFile(file);
                    completionHandler(result.wasOk() ? "Accepted MIDI exported to " + file.getFullPathName()
                                                     : result.getErrorMessage());
                });
        })
        .withNativeFunction("renderAcceptedWav", [this](const auto& arguments, auto finish)
        {
            if (!arguments.isEmpty())
            {
                finish("Render accepted WAV takes no arguments");
                return;
            }
            if (!ownerProcessor.getCompositionSnapshot().hasAccepted)
            {
                finish("Accept a composition before rendering WAV audio");
                return;
            }
            if (wavExportChooserActive)
            {
                finish("A WAV destination chooser is already open");
                return;
            }
            wavExportChooserActive = true;
            wavExportChooser = std::make_unique<juce::FileChooser>(
                "Render accepted folk park WAV",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("folk-park-preview.wav"),
                "*.wav");
            const auto safeEditor = juce::Component::SafePointer<PluginEditor>(this);
            wavExportChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::warnAboutOverwriting,
                [safeEditor, completionHandler = std::move(finish)](
                    const juce::FileChooser& chooser) mutable
                {
                    if (safeEditor == nullptr)
                        return;
                    safeEditor->wavExportChooserActive = false;
                    auto file = chooser.getResult();
                    if (file == juce::File{})
                    {
                        completionHandler("WAV rendering cancelled before any file was written");
                        return;
                    }
                    if (file.getFileExtension().isEmpty())
                        file = file.withFileExtension(".wav");
                    const auto result = safeEditor->ownerProcessor.requestAcceptedWavRender(file, true);
                    completionHandler(result.wasOk()
                        ? "Accepted composition queued for isolated 24-bit WAV rendering"
                        : result.getErrorMessage());
                });
        })
        .withNativeFunction("cancelAcceptedWav", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty())
            {
                complete("Cancel accepted WAV takes no arguments");
                return;
            }
            ownerProcessor.cancelAcceptedWavRender();
            complete("Offline WAV cancellation requested; live voices were not touched");
        })
        .withResourceProvider([](const auto& url)
        {
            return resourceFor(url);
        });
}

std::optional<juce::WebBrowserComponent::Resource> PluginEditor::resourceFor(const juce::String& url)
{
    const auto path = url == "/" ? juce::String("index.html")
                                  : url.fromFirstOccurrenceOf("/", false, false);
    if (path == "index.html")
        return makeResource(FolkParkAssets::index_html, FolkParkAssets::index_htmlSize, "text/html");
    if (path == "app.js")
        return makeResource(FolkParkAssets::app_js, FolkParkAssets::app_jsSize, "text/javascript");
    if (path == "app.css")
        return makeResource(FolkParkAssets::app_css, FolkParkAssets::app_cssSize, "text/css");
    return std::nullopt;
}

void PluginEditor::timerCallback()
{
    if (browser == nullptr)
        return;

    const auto import = ownerProcessor.getWavetableImportSnapshot();
    const auto routes = ownerProcessor.getConfiguredModulationRoutes();
    const auto composition = ownerProcessor.getCompositionSnapshot();
    const auto rendered = ownerProcessor.getAcceptedWavRenderSnapshot();
    midiDrag->updateAvailability(composition.hasAccepted);
    auto snapshot = juce::DynamicObject::Ptr(new juce::DynamicObject());
    snapshot->setProperty("schemaVersion", 1);
    snapshot->setProperty("product", "folk park");
    snapshot->setProperty("version", FOLK_PARK_VERSION);
    snapshot->setProperty("state", "bundled bridge online");
    snapshot->setProperty("activeVoices", ownerProcessor.getActiveVoiceCount());
    snapshot->setProperty("importStatus", importStatusName(import.status));
    snapshot->setProperty("importMessage", import.message);
    snapshot->setProperty("importOscillator", import.oscillatorIndex == 0 ? "A" : "B");
    snapshot->setProperty("importFile", import.metadata.sourceFileName);
    snapshot->setProperty("importFrames", import.metadata.outputFrameCount);
    snapshot->setProperty("importCycleLength", import.metadata.acceptedCycleLength);
    snapshot->setProperty("modulationRouteCount", static_cast<int>(routes.routeCount));
    snapshot->setProperty("compositionStatus", composition.status);
    snapshot->setProperty("compositionHasCandidate", composition.hasCandidate);
    snapshot->setProperty("compositionHasAccepted", composition.hasAccepted);
    snapshot->setProperty("compositionCandidateNotes", composition.candidateNoteCount);
    snapshot->setProperty("compositionAcceptedNotes", composition.acceptedNoteCount);
    snapshot->setProperty("directMidiPlaying", ownerProcessor.isDirectMidiPlaying());
    snapshot->setProperty("renderStatus", renderStatusName(rendered.status));
    snapshot->setProperty("renderMessage", rendered.message);
    snapshot->setProperty("renderDestination", rendered.destination);
    snapshot->setProperty("renderDuration", rendered.durationSeconds);
    browser->emitEventIfBrowserIsVisible("processorSnapshot", juce::var(snapshot.get()));
}

void PluginEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff080a12));
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    const auto dragStrip = bounds.removeFromBottom(52).reduced(10, 7);
    fallback.setBounds(bounds);
    if (browser != nullptr)
        browser->setBounds(bounds);
    midiDrag->setBounds(dragStrip);
}
}
