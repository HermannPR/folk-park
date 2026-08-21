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
                           owner.state().undoManager)
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
    if (path == "juce.js")
        return makeResource(FolkParkAssets::index_js, FolkParkAssets::index_jsSize, "text/javascript");
    if (path == "check_native_interop.js")
        return makeResource(FolkParkAssets::check_native_interop_js,
                            FolkParkAssets::check_native_interop_jsSize,
                            "text/javascript");
    return std::nullopt;
}

void PluginEditor::timerCallback()
{
    if (browser == nullptr)
        return;

    const auto import = ownerProcessor.getWavetableImportSnapshot();
    const auto routes = ownerProcessor.getConfiguredModulationRoutes();
    const auto composition = ownerProcessor.getCompositionSnapshot();
    midiDrag->updateAvailability(composition.hasAccepted);
    auto snapshot = juce::DynamicObject::Ptr(new juce::DynamicObject());
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
