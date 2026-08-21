#pragma once

#include "PluginProcessor.h"
#include "midi/MidiProof.h"

#include <optional>

namespace folkpark
{
class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    class LocalBrowser;
    class MidiDragButton;

    static std::optional<juce::WebBrowserComponent::Resource> resourceFor(const juce::String& url);
    juce::WebBrowserComponent::Options browserOptions();
    void timerCallback() override;

    juce::Label fallback;
    std::unique_ptr<LocalBrowser> browser;
    std::unique_ptr<MidiDragButton> midiDrag;
    int snapshotAttempts = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
}
