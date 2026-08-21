#include "PluginEditor.h"

#include <BinaryData.h>
#include <cstddef>
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
    MidiDragButton() : TextButton("Drag M0 MIDI proof into FL Studio") {}

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStarted = false;
        TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragStarted && event.getDistanceFromDragStart() > 4)
        {
            temporaryFile = midi::writeM0ProofMidiToTemporaryFile();
            if (temporaryFile.existsAsFile())
            {
                dragStarted = juce::DragAndDropContainer::performExternalDragDropOfFiles(
                    {temporaryFile.getFullPathName()}, false, this);
            }
        }
        TextButton::mouseDrag(event);
    }

private:
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

    midiDrag = std::make_unique<MidiDragButton>();
    addAndMakeVisible(*midiDrag);

    setResizable(true, true);
    setResizeLimits(720, 480, 1600, 1000);
    setSize(1180, 720);
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
