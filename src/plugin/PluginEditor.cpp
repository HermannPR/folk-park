#include "PluginEditor.h"

#include <BinaryData.h>
#include <cstddef>

namespace folkpark
{
namespace
{
juce::WebBrowserComponent::Resource makeResource(const void* data, int size, const char* mime)
{
    const auto* begin = static_cast<const std::byte*>(data);
    return {{begin, begin + size}, mime};
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
    : AudioProcessorEditor(&owner)
{
    fallback.setText("folk park M0 — native fallback editor", juce::dontSendNotification);
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
        .withNativeFunction("getProductInfo", [](const auto&, auto complete)
        {
            auto info = juce::DynamicObject::Ptr(new juce::DynamicObject());
            info->setProperty("product", "folk park");
            info->setProperty("version", FOLK_PARK_VERSION);
            info->setProperty("architecture", "x86_64");
            complete(juce::var(info.get()));
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
    constexpr int maximumSnapshotAttempts = 25;
    if (browser == nullptr || snapshotAttempts >= maximumSnapshotAttempts)
        return;

    auto snapshot = juce::DynamicObject::Ptr(new juce::DynamicObject());
    snapshot->setProperty("product", "folk park");
    snapshot->setProperty("version", FOLK_PARK_VERSION);
    snapshot->setProperty("state", "bundled bridge online");
    browser->emitEventIfBrowserIsVisible("processorSnapshot", juce::var(snapshot.get()));
    ++snapshotAttempts;
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
