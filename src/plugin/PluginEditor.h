#pragma once

#include "PluginProcessor.h"
#include "ParameterIds.h"

#include <juce_gui_extra/juce_gui_extra.h>

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

    PluginProcessor& ownerProcessor;
    juce::WebSliderRelay masterGainRelay{parameterIds::masterGain};
    juce::WebSliderRelay cutoffRelay{parameterIds::filterCutoff};
    juce::WebSliderRelay attackRelay{parameterIds::ampAttack};
    juce::WebSliderRelay releaseRelay{parameterIds::ampRelease};
    juce::WebSliderRelay oscillatorAPositionRelay{parameterIds::oscillatorAPosition};
    juce::WebSliderRelay oscillatorBPositionRelay{parameterIds::oscillatorBPosition};
    juce::WebSliderRelay oscillatorBLevelRelay{parameterIds::oscillatorBLevel};
    juce::WebSliderRelay filterResonanceRelay{parameterIds::filterResonance};
    juce::WebSliderRelay filterDriveRelay{parameterIds::filterDrive};
    juce::WebSliderRelay lfo1RateRelay{parameterIds::lfoRate[0]};
    juce::WebComboBoxRelay waveformRelay{parameterIds::oscillatorWaveform};
    juce::WebComboBoxRelay filterModeRelay{parameterIds::filterMode};

    juce::Label fallback;
    std::unique_ptr<LocalBrowser> browser;
    std::unique_ptr<MidiDragButton> midiDrag;
    juce::WebSliderParameterAttachment masterGainAttachment;
    juce::WebSliderParameterAttachment cutoffAttachment;
    juce::WebSliderParameterAttachment attackAttachment;
    juce::WebSliderParameterAttachment releaseAttachment;
    juce::WebSliderParameterAttachment oscillatorAPositionAttachment;
    juce::WebSliderParameterAttachment oscillatorBPositionAttachment;
    juce::WebSliderParameterAttachment oscillatorBLevelAttachment;
    juce::WebSliderParameterAttachment filterResonanceAttachment;
    juce::WebSliderParameterAttachment filterDriveAttachment;
    juce::WebSliderParameterAttachment lfo1RateAttachment;
    juce::WebComboBoxParameterAttachment waveformAttachment;
    juce::WebComboBoxParameterAttachment filterModeAttachment;
    std::unique_ptr<juce::FileChooser> wavetableChooser;
    bool wavetableChooserActive = false;
    std::unique_ptr<juce::FileChooser> midiExportChooser;
    bool midiExportChooserActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
}
