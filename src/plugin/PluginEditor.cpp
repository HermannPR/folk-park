#include "PluginEditor.h"

#include <BinaryData.h>
#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
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

bool boundedString(const juce::var& value,
                   int maximumLength,
                   juce::String& output,
                   bool allowEmpty = true)
{
    if (!value.isString())
        return false;
    output = value.toString().trim();
    return output.length() <= maximumLength && (allowEmpty || !output.isEmpty());
}

bool boundedTags(const juce::var& value, std::vector<juce::String>& output)
{
    const auto* array = value.getArray();
    if (array == nullptr || array->size() > 24)
        return false;
    std::set<juce::String> unique;
    for (const auto& item : *array)
    {
        juce::String tag;
        if (!boundedString(item, 48, tag, false) || !unique.insert(tag).second)
            return false;
        output.push_back(tag);
    }
    return true;
}

juce::var stringArrayPayload(const std::vector<juce::String>& values)
{
    juce::Array<juce::var> result;
    result.ensureStorageAllocated(static_cast<int>(values.size()));
    for (const auto& value : values)
        result.add(value);
    return juce::var(result);
}

juce::var persistenceStatusPayload(const persistence::PersistenceStatusSnapshot& status)
{
    auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
    object->setProperty("enabled", status.enabled);
    object->setProperty("presetAvailable", status.presetAvailable);
    object->setProperty("historyAvailable", status.historyAvailable);
    object->setProperty("message", status.message);
    object->setProperty("currentPresetId", status.currentPresetId);
    object->setProperty("currentPresetName", status.currentPresetName);
    object->setProperty("currentPresetDirty", status.currentPresetDirty);
    object->setProperty("retentionDays", status.retentionDays);
    juce::Array<juce::var> missing;
    for (const auto& reference : status.missingAssets)
    {
        auto asset = juce::DynamicObject::Ptr(new juce::DynamicObject());
        asset->setProperty("slot", persistence::stableId(reference.slot));
        asset->setProperty("displayName", reference.recoveryDisplayName);
        asset->setProperty("sha256", reference.sha256);
        asset->setProperty("byteSize", reference.byteSize);
        missing.add(juce::var(asset.get()));
    }
    object->setProperty("missingAssets", juce::var(missing));
    return juce::var(object.get());
}

juce::var presetLibraryPayload(const persistence::PresetLibraryResult& library)
{
    juce::Array<juce::var> presets;
    for (const auto& summary : library.presets)
    {
        auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
        object->setProperty("id", summary.id);
        object->setProperty("name", summary.name);
        object->setProperty("author", summary.author);
        object->setProperty("tags", stringArrayPayload(summary.tags));
        object->setProperty("genre", summary.genre);
        object->setProperty("emotion", summary.emotion);
        object->setProperty("favorite", summary.favorite);
        object->setProperty("missingAssets", summary.missingAssets);
        object->setProperty("fileName", summary.fileName);
        presets.add(juce::var(object.get()));
    }
    return juce::var(presets);
}

juce::var historyPayload(const persistence::HistorySearchResult& history)
{
    juce::Array<juce::var> entries;
    for (const auto& summary : history.entries)
    {
        auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
        object->setProperty("id", summary.id);
        object->setProperty("parentId", summary.parentId);
        object->setProperty("createdUnixMs", summary.createdUnixMs);
        object->setProperty("updatedUnixMs", summary.updatedUnixMs);
        object->setProperty("generatorVersion", summary.generatorVersion);
        object->setProperty("promptSummary", summary.storePromptSummary
            ? summary.promptSummary : juce::String{});
        object->setProperty("presetId", summary.presetId);
        object->setProperty("favorite", summary.favorite);
        object->setProperty("tags", stringArrayPayload(summary.tags));
        object->setProperty("deleted", summary.deleted);
        entries.add(juce::var(object.get()));
    }
    return juce::var(entries);
}

juce::var persistenceWorkspacePayload(PluginProcessor& processor,
                                      const persistence::HistorySearchQuery& query = {})
{
    const auto library = processor.listPresets();
    const auto history = processor.searchHistory(query);
    auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
    object->setProperty("ok", library.status.wasOk());
    object->setProperty("status", persistenceStatusPayload(processor.getPersistenceStatus()));
    object->setProperty("presetError", library.status.wasOk()
        ? juce::String{} : library.status.getErrorMessage());
    object->setProperty("historyError", history.status.wasOk()
        ? juce::String{} : history.status.getErrorMessage());
    object->setProperty("presets", presetLibraryPayload(library));
    object->setProperty("history", historyPayload(history));
    return juce::var(object.get());
}

juce::var historyDetailPayload(const HistoryEntryDetail& detail)
{
    auto object = juce::DynamicObject::Ptr(new juce::DynamicObject());
    object->setProperty("id", detail.summary.id);
    object->setProperty("parentId", detail.summary.parentId);
    object->setProperty("createdUnixMs", detail.summary.createdUnixMs);
    object->setProperty("generatorVersion", detail.summary.generatorVersion);
    object->setProperty("presetId", detail.summary.presetId);
    object->setProperty("favorite", detail.summary.favorite);
    object->setProperty("tags", stringArrayPayload(detail.summary.tags));
    object->setProperty("seed", static_cast<juce::int64>(detail.intent.seed));
    object->setProperty("key", midi::stableId(detail.intent.key));
    object->setProperty("scale", midi::stableId(detail.intent.scale));
    object->setProperty("tempoBpm", detail.intent.tempoBpm);
    object->setProperty("bars", detail.intent.lengthBars);
    object->setProperty("genre", midi::stableId(detail.intent.genreProfile));
    object->setProperty("emotion", midi::stableId(detail.intent.emotion));
    object->setProperty("clipCount", detail.clipCount);
    object->setProperty("noteCount", detail.noteCount);
    return juce::var(object.get());
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

bool hasOnlyObjectProperties(const juce::DynamicObject& object,
                             std::initializer_list<const char*> allowed)
{
    const auto& properties = object.getProperties();
    for (int index = 0; index < properties.size(); ++index)
    {
        const auto name = properties.getName(index).toString();
        auto found = false;
        for (const auto* candidate : allowed)
            found = found || name == candidate;
        if (!found)
            return false;
    }
    return true;
}

bool optionalAnswer(const juce::DynamicObject& object,
                    const char* name,
                    int maximumLength,
                    juce::String& output)
{
    if (!object.hasProperty(name))
        return true;
    return boundedString(object.getProperty(name), maximumLength, output);
}

bool parseJarvisSoundInput(const juce::var& value,
                           bool requirePrompt,
                           assistant::AssistantRequest& request)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr
        || !hasOnlyObjectProperties(*object, {"entryMode", "seed", "prompt", "answers"})
        || !object->hasProperty("entryMode") || !object->hasProperty("seed")
        || !object->hasProperty("prompt") || !object->hasProperty("answers"))
        return false;

    juce::String mode;
    juce::String prompt;
    double seed = 0.0;
    if (!boundedString(object->getProperty("entryMode"), 16, mode, false)
        || (mode != "guided" && mode != "describe")
        || !boundedNumber(object->getProperty("seed"), 0.0,
                          static_cast<double>(std::numeric_limits<std::uint32_t>::max()), seed)
        || std::floor(seed) != seed
        || !boundedString(object->getProperty("prompt"),
                          assistant::AssistantRequest::maximumPromptLength, prompt,
                          !requirePrompt))
        return false;

    const auto* answers = object->getProperty("answers").getDynamicObject();
    if (answers == nullptr
        || !hasOnlyObjectProperties(*answers,
            {"musicalRole", "timbre", "articulation", "movement", "space",
             "intensity", "genreContext", "referenceDescription"}))
        return false;

    assistant::SoundIntent intent;
    intent.seed = static_cast<std::uint32_t>(seed);
    intent.entryMode = mode == "guided"
        ? assistant::SoundEntryMode::guided : assistant::SoundEntryMode::describe;
    if (!optionalAnswer(*answers, "musicalRole", 128, intent.answers.musicalRole)
        || !optionalAnswer(*answers, "timbre", 256, intent.answers.timbre)
        || !optionalAnswer(*answers, "articulation", 128, intent.answers.articulation)
        || !optionalAnswer(*answers, "movement", 128, intent.answers.movement)
        || !optionalAnswer(*answers, "space", 128, intent.answers.space)
        || !optionalAnswer(*answers, "genreContext", 128, intent.answers.genreContext)
        || !optionalAnswer(*answers, "referenceDescription", 512,
                           intent.answers.referenceDescription))
        return false;
    if (answers->hasProperty("intensity"))
    {
        const auto intensityValue = answers->getProperty("intensity");
        if (!intensityValue.isVoid())
        {
            double intensity = 0.0;
            if (!boundedNumber(intensityValue, 0.0, 1.0, intensity))
                return false;
            intent.answers.intensity = static_cast<float>(intensity);
        }
    }
    if (intent.entryMode == assistant::SoundEntryMode::describe
        && intent.answers.referenceDescription.isEmpty())
        intent.answers.referenceDescription = prompt;

    const auto identity = prompt + "|" + mode + "|" + intent.answers.musicalRole + "|"
        + intent.answers.timbre + "|" + intent.answers.articulation + "|"
        + intent.answers.movement + "|" + intent.answers.space + "|"
        + intent.answers.genreContext + "|" + intent.answers.referenceDescription
        + (intent.answers.intensity.has_value()
            ? "|" + juce::String(*intent.answers.intensity, 6) : "|unanswered");
    intent.requestId = midi::deterministicUuid(intent.seed, "jarvis-ui-sound-" + identity);
    request = {};
    request.requestId = intent.requestId;
    request.target = assistant::AssistantTarget::sound;
    request.origin = assistant::AssistantOrigin::offline;
    request.prompt = prompt.isEmpty() ? "Continue the guided sound walkthrough" : prompt;
    request.soundIntent = std::move(intent);
    return true;
}

juce::var guidedProgressPayload(const assistant::GuidedProgress& progress)
{
    auto payload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    payload->setProperty("ok", true);
    payload->setProperty("completion", progress.completion);
    payload->setProperty("readyForProposal", progress.readyForProposal);
    juce::Array<juce::var> questions;
    questions.ensureStorageAllocated(static_cast<int>(progress.questions.size()));
    for (const auto& source : progress.questions)
    {
        auto question = juce::DynamicObject::Ptr(new juce::DynamicObject());
        question->setProperty("id", source.id);
        question->setProperty("prompt", source.prompt);
        question->setProperty("purpose", source.purpose);
        question->setProperty("required", source.required);
        questions.add(juce::var(question.get()));
    }
    payload->setProperty("questions", juce::var(questions));
    return juce::var(payload.get());
}

juce::var assistantAuditionPayload(const assistant::AssistantAuditionSnapshot& snapshot)
{
    auto payload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    payload->setProperty("ok", true);
    payload->setProperty("status", assistant::stableId(snapshot.status));
    payload->setProperty("active", snapshot.active());
    payload->setProperty("audibleSide", assistant::stableId(snapshot.audibleSide));
    payload->setProperty("message", snapshot.message);
    if (!snapshot.proposal.has_value())
    {
        payload->setProperty("proposal", juce::var());
        return juce::var(payload.get());
    }

    const auto& source = *snapshot.proposal;
    auto proposal = juce::DynamicObject::Ptr(new juce::DynamicObject());
    proposal->setProperty("proposalId", source.proposalId);
    proposal->setProperty("requestId", source.requestId);
    proposal->setProperty("explanation", source.explanation);
    proposal->setProperty("confidence", source.confidence);
    proposal->setProperty("requiresExplicitAcceptance", source.requiresExplicitAcceptance);
    proposal->setProperty("assumptions", stringArrayPayload(source.assumptions));
    juce::Array<juce::var> changes;
    changes.ensureStorageAllocated(static_cast<int>(source.changes.size()));
    for (const auto& sourceChange : source.changes)
    {
        auto change = juce::DynamicObject::Ptr(new juce::DynamicObject());
        change->setProperty("parameterId", sourceChange.parameterId);
        change->setProperty("currentNormalized", sourceChange.currentNormalized);
        change->setProperty("proposedNormalized", sourceChange.proposedNormalized);
        change->setProperty("reason", sourceChange.reason);
        changes.add(juce::var(change.get()));
    }
    proposal->setProperty("changes", juce::var(changes));
    payload->setProperty("proposal", juce::var(proposal.get()));
    return juce::var(payload.get());
}

juce::var jarvisCompositionPayload(const PluginProcessor& processor,
                                    const assistant::AssistantResponse& response)
{
    const auto& intent = *response.musicIntent;
    auto intentPayload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    intentPayload->setProperty("requestId", intent.requestId);
    intentPayload->setProperty("seed", static_cast<juce::int64>(intent.seed));
    intentPayload->setProperty("key", midi::stableId(intent.key));
    intentPayload->setProperty("scale", midi::stableId(intent.scale));
    intentPayload->setProperty("tempoBpm", intent.tempoBpm);
    intentPayload->setProperty("bars", intent.lengthBars);
    intentPayload->setProperty("genre", midi::stableId(intent.genreProfile));
    intentPayload->setProperty("emotion", midi::stableId(intent.emotion));
    juce::Array<juce::var> parts;
    for (std::size_t index = 0; index < intent.partCount; ++index)
        parts.add(midi::stableId(intent.parts[index]));
    intentPayload->setProperty("parts", juce::var(parts));

    auto payload = juce::DynamicObject::Ptr(new juce::DynamicObject());
    payload->setProperty("ok", true);
    payload->setProperty("summary", response.summary);
    payload->setProperty("intent", juce::var(intentPayload.get()));
    payload->setProperty("composition", compositionPayload(processor));
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
    snapshot->setProperty("persistence", persistenceStatusPayload(
        processor.getPersistenceStatus()));
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
    const auto persistenceInitialisation = ownerProcessor.initialisePersistence();
    juce::ignoreUnused(persistenceInitialisation);
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
        .withNativeFunction("getJarvisState", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty())
            {
                complete("Jarvis state takes no arguments");
                return;
            }
            complete(assistantAuditionPayload(ownerProcessor.getAssistantAuditionSnapshot()));
        })
        .withNativeFunction("getJarvisQuestions", [this](const auto& arguments, auto complete)
        {
            assistant::AssistantRequest request;
            if (arguments.size() != 1
                || !parseJarvisSoundInput(arguments[0], false, request))
            {
                complete("Jarvis questions require one bounded guided or describe sound-intent object");
                return;
            }
            complete(guidedProgressPayload(
                ownerProcessor.getAssistantQuestions(*request.soundIntent)));
        })
        .withNativeFunction("createJarvisSoundProposal", [this](const auto& arguments,
                                                                 auto complete)
        {
            assistant::AssistantRequest request;
            if (arguments.size() != 1
                || !parseJarvisSoundInput(arguments[0], true, request))
            {
                complete("Jarvis sound proposal requires one bounded guided or describe intent");
                return;
            }
            const auto result = ownerProcessor.runOfflineAssistant(request);
            if (result.status.failed() || !result.response.has_value()
                || !result.response->parameterProposal.has_value())
            {
                complete(result.status.getErrorMessage());
                return;
            }
            if (const auto begun = ownerProcessor.beginAssistantProposal(
                    *result.response->parameterProposal); begun.failed())
            {
                complete(begun.getErrorMessage());
                return;
            }
            auto payload = assistantAuditionPayload(
                ownerProcessor.getAssistantAuditionSnapshot());
            if (auto* object = payload.getDynamicObject())
                object->setProperty("summary", result.response->summary);
            complete(payload);
        })
        .withNativeFunction("auditionJarvisSide", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 1 || !arguments[0].isString())
            {
                complete("Jarvis audition requires side original or proposal");
                return;
            }
            const auto side = arguments[0].toString();
            if (side != "original" && side != "proposal")
            {
                complete("Jarvis audition side must be original or proposal");
                return;
            }
            const auto result = ownerProcessor.auditionAssistantSide(
                side == "original" ? assistant::AuditionSide::original
                                   : assistant::AuditionSide::proposal);
            complete(result.wasOk()
                ? assistantAuditionPayload(ownerProcessor.getAssistantAuditionSnapshot())
                : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("acceptJarvisProposal", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty())
            {
                complete("Jarvis acceptance takes no arguments");
                return;
            }
            const auto result = ownerProcessor.acceptAssistantProposal();
            complete(result.wasOk()
                ? assistantAuditionPayload(ownerProcessor.getAssistantAuditionSnapshot())
                : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("rejectJarvisProposal", [this](const auto& arguments, auto complete)
        {
            if (!arguments.isEmpty())
            {
                complete("Jarvis rejection takes no arguments");
                return;
            }
            const auto result = ownerProcessor.rejectAssistantProposal();
            complete(result.wasOk()
                ? assistantAuditionPayload(ownerProcessor.getAssistantAuditionSnapshot())
                : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("createJarvisComposition", [this](const auto& arguments,
                                                               auto complete)
        {
            if (arguments.size() != 1)
            {
                complete("Jarvis composition requires one bounded prompt and seed object");
                return;
            }
            const auto* object = arguments[0].getDynamicObject();
            juce::String prompt;
            double seed = 0.0;
            if (object == nullptr
                || !hasOnlyObjectProperties(*object, {"prompt", "seed"})
                || !object->hasProperty("prompt") || !object->hasProperty("seed")
                || !boundedString(object->getProperty("prompt"),
                                  assistant::AssistantRequest::maximumPromptLength,
                                  prompt, false)
                || !boundedNumber(object->getProperty("seed"), 0.0,
                    static_cast<double>(std::numeric_limits<std::uint32_t>::max()), seed)
                || std::floor(seed) != seed)
            {
                complete("Jarvis composition prompt or seed is malformed or outside bounds");
                return;
            }

            assistant::AssistantRequest request;
            request.requestId = midi::deterministicUuid(
                static_cast<std::uint32_t>(seed), "jarvis-ui-composition-" + prompt);
            request.target = assistant::AssistantTarget::composition;
            request.origin = assistant::AssistantOrigin::offline;
            request.prompt = prompt;
            midi::MusicIntent fallbackIntent;
            fallbackIntent.requestId = request.requestId;
            fallbackIntent.seed = static_cast<std::uint32_t>(seed);
            request.compositionFallback = fallbackIntent;
            const auto result = ownerProcessor.runOfflineAssistant(request);
            if (result.status.failed() || !result.response.has_value()
                || !result.response->musicIntent.has_value())
            {
                complete(result.status.getErrorMessage());
                return;
            }
            if (const auto generated = ownerProcessor.generateCompositionCandidate(
                    *result.response->musicIntent); generated.failed())
            {
                complete(generated.getErrorMessage());
                return;
            }
            complete(jarvisCompositionPayload(ownerProcessor, *result.response));
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
                    {
                        const auto withExtension = file.withFileExtension(".wav");
                        if (withExtension.existsAsFile())
                        {
                            completionHandler("That .wav file already exists; choose it explicitly so macOS can confirm replacement");
                            return;
                        }
                        file = withExtension;
                    }
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
        .withNativeFunction("getPersistenceWorkspace", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 3)
            {
                complete("Persistence workspace requires search text and two bounded filters");
                return;
            }
            juce::String text;
            bool favoritesOnly = false;
            bool includeDeleted = false;
            if (!boundedString(arguments[0], 128, text)
                || !strictBoolean(arguments[1], favoritesOnly)
                || !strictBoolean(arguments[2], includeDeleted))
            {
                complete("Persistence workspace filters are malformed");
                return;
            }
            complete(persistenceWorkspacePayload(ownerProcessor,
                {text, favoritesOnly, includeDeleted, 100}));
        })
        .withNativeFunction("savePreset", [this](const auto& arguments, auto complete)
        {
            if (arguments.size() != 8)
            {
                complete("Preset save requires name, author, tags, genre, emotion, description, favorite, and overwrite");
                return;
            }
            PresetSaveRequest request;
            if (!boundedString(arguments[0], 96, request.name, false)
                || !boundedString(arguments[1], 96, request.author)
                || !boundedTags(arguments[2], request.tags)
                || !boundedString(arguments[3], 64, request.genre)
                || !boundedString(arguments[4], 64, request.emotion)
                || !boundedString(arguments[5], 1024, request.description)
                || !strictBoolean(arguments[6], request.favorite)
                || !strictBoolean(arguments[7], request.allowOverwrite))
            {
                complete("Preset metadata is malformed or exceeds the native bounds");
                return;
            }
            const auto result = ownerProcessor.saveCurrentPreset(request);
            complete(result.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("loadPreset", [this](const auto& arguments, auto complete)
        {
            juce::String id;
            if (arguments.size() != 1 || !boundedString(arguments[0], 64, id, false)
                || !midi::isUuid(id))
            {
                complete("Preset load requires one valid stable ID");
                return;
            }
            const auto result = ownerProcessor.loadLibraryPreset(id);
            complete(result.wasOk() ? completeUiSnapshot(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("choosePresetFile", [this](const auto& arguments, auto finish)
        {
            if (!arguments.isEmpty())
            {
                finish("Preset import takes no arguments");
                return;
            }
            if (presetImportChooserActive)
            {
                finish("A preset import chooser is already open");
                return;
            }
            presetImportChooserActive = true;
            presetImportChooser = std::make_unique<juce::FileChooser>(
                "Import a folk park preset", juce::File{}, "*.folkparkpreset");
            const auto safeEditor = juce::Component::SafePointer<PluginEditor>(this);
            presetImportChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeEditor, completionHandler = std::move(finish)](
                    const juce::FileChooser& chooser) mutable
                {
                    if (safeEditor == nullptr)
                        return;
                    safeEditor->presetImportChooserActive = false;
                    const auto file = chooser.getResult();
                    if (file == juce::File{})
                    {
                        completionHandler("Preset import cancelled");
                        return;
                    }
                    const auto result = safeEditor->ownerProcessor.importExternalPreset(file);
                    completionHandler(result.wasOk()
                        ? completeUiSnapshot(safeEditor->ownerProcessor)
                        : juce::var(result.getErrorMessage()));
                });
        })
        .withNativeFunction("relinkPresetAsset", [this](const auto& arguments, auto finish)
        {
            double slot = 0.0;
            if (arguments.size() != 1 || !boundedNumber(arguments[0], 0.0, 1.0, slot)
                || std::floor(slot) != slot)
            {
                finish("Preset relink requires oscillator slot 0 or 1");
                return;
            }
            if (presetRelinkChooserActive)
            {
                finish("A missing-asset chooser is already open");
                return;
            }
            presetRelinkChooserActive = true;
            presetRelinkChooser = std::make_unique<juce::FileChooser>(
                "Select the exact matching WAV asset", juce::File{}, "*.wav");
            const auto safeEditor = juce::Component::SafePointer<PluginEditor>(this);
            const auto assetSlot = slot == 0.0 ? persistence::AssetSlot::oscillatorA
                                               : persistence::AssetSlot::oscillatorB;
            presetRelinkChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeEditor, assetSlot, completionHandler = std::move(finish)](
                    const juce::FileChooser& chooser) mutable
                {
                    if (safeEditor == nullptr)
                        return;
                    safeEditor->presetRelinkChooserActive = false;
                    const auto file = chooser.getResult();
                    if (file == juce::File{})
                    {
                        completionHandler("Missing-asset recovery cancelled");
                        return;
                    }
                    const auto result = safeEditor->ownerProcessor.relinkPendingPresetAsset(
                        assetSlot, file);
                    completionHandler(result.wasOk()
                        ? completeUiSnapshot(safeEditor->ownerProcessor)
                        : juce::var(result.getErrorMessage()));
                });
        })
        .withNativeFunction("setPresetFavorite", [this](const auto& arguments, auto complete)
        {
            juce::String id;
            bool favorite = false;
            if (arguments.size() != 2 || !boundedString(arguments[0], 64, id, false)
                || !midi::isUuid(id) || !strictBoolean(arguments[1], favorite))
            {
                complete("Preset favorite requires a valid ID and boolean");
                return;
            }
            const auto result = ownerProcessor.setPresetFavorite(id, favorite);
            complete(result.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("recallHistory", [this](const auto& arguments, auto complete)
        {
            juce::String id;
            if (arguments.size() != 1 || !boundedString(arguments[0], 64, id, false)
                || !midi::isUuid(id))
            {
                complete("History recall requires one valid stable ID");
                return;
            }
            const auto result = ownerProcessor.recallHistory(id);
            complete(result.wasOk() ? completeUiSnapshot(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("setHistoryFavorite", [this](const auto& arguments, auto complete)
        {
            juce::String id;
            bool favorite = false;
            if (arguments.size() != 2 || !boundedString(arguments[0], 64, id, false)
                || !midi::isUuid(id) || !strictBoolean(arguments[1], favorite))
            {
                complete("History favorite requires a valid ID and boolean");
                return;
            }
            const auto result = ownerProcessor.setHistoryFavorite(id, favorite);
            complete(result.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("setHistoryDeleted", [this](const auto& arguments, auto complete)
        {
            juce::String id;
            bool deleted = false;
            if (arguments.size() != 2 || !boundedString(arguments[0], 64, id, false)
                || !midi::isUuid(id) || !strictBoolean(arguments[1], deleted))
            {
                complete("History recovery state requires a valid ID and boolean");
                return;
            }
            const auto result = ownerProcessor.setHistorySoftDeleted(id, deleted);
            complete(result.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("compareHistory", [this](const auto& arguments, auto complete)
        {
            juce::String firstId;
            juce::String secondId;
            if (arguments.size() != 2
                || !boundedString(arguments[0], 64, firstId, false)
                || !boundedString(arguments[1], 64, secondId, false)
                || firstId == secondId || !midi::isUuid(firstId) || !midi::isUuid(secondId))
            {
                complete("History comparison requires two different valid IDs");
                return;
            }
            const auto first = ownerProcessor.inspectHistory(firstId);
            const auto second = ownerProcessor.inspectHistory(secondId);
            if (!first.has_value() || !second.has_value())
            {
                complete("One or both history entries could not be inspected");
                return;
            }
            auto comparison = juce::DynamicObject::Ptr(new juce::DynamicObject());
            comparison->setProperty("first", historyDetailPayload(*first));
            comparison->setProperty("second", historyDetailPayload(*second));
            complete(juce::var(comparison.get()));
        })
        .withNativeFunction("setHistoryRetention", [this](const auto& arguments, auto complete)
        {
            double days = 0.0;
            if (arguments.size() != 1 || !boundedNumber(arguments[0], 1.0, 3650.0, days)
                || std::floor(days) != days)
            {
                complete("History retention requires an integer from 1 to 3650 days");
                return;
            }
            const auto result = ownerProcessor.setHistoryRetentionDays(static_cast<int>(days));
            complete(result.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                    : juce::var(result.getErrorMessage()));
        })
        .withNativeFunction("cleanupHistory", [this](const auto& arguments, auto complete)
        {
            bool keepFavorites = true;
            if (arguments.size() != 1 || !strictBoolean(arguments[0], keepFavorites)
                || !keepFavorites)
            {
                complete("Release 0.1 cleanup permanently removes only expired non-favorites");
                return;
            }
            const auto result = ownerProcessor.cleanupHistory(keepFavorites);
            complete(result.status.wasOk() ? persistenceWorkspacePayload(ownerProcessor)
                                           : juce::var(result.status.getErrorMessage()));
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
    const auto persistenceStatus = ownerProcessor.getPersistenceStatus();
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
    snapshot->setProperty("persistence", persistenceStatusPayload(persistenceStatus));
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
