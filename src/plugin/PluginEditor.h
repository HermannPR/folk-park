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
    juce::WebSliderRelay oscillatorALevelRelay{parameterIds::oscillatorLevel};
    juce::WebSliderRelay subLevelRelay{parameterIds::subLevel};
    juce::WebSliderRelay noiseLevelRelay{parameterIds::noiseLevel};
    juce::WebSliderRelay ampDecayRelay{parameterIds::ampDecay};
    juce::WebSliderRelay ampSustainRelay{parameterIds::ampSustain};
    juce::WebSliderRelay filterKeyTrackingRelay{parameterIds::filterKeyTracking};
    juce::WebSliderRelay filterEnvelopeAmountRelay{parameterIds::filterEnvelopeAmount};
    juce::WebSliderRelay filterEnvelopeAttackRelay{parameterIds::filterEnvelopeAttack};
    juce::WebSliderRelay filterEnvelopeDecayRelay{parameterIds::filterEnvelopeDecay};
    juce::WebSliderRelay filterEnvelopeSustainRelay{parameterIds::filterEnvelopeSustain};
    juce::WebSliderRelay filterEnvelopeReleaseRelay{parameterIds::filterEnvelopeRelease};
    juce::WebSliderRelay auxiliaryEnvelopeAttackRelay{parameterIds::auxiliaryEnvelopeAttack};
    juce::WebSliderRelay auxiliaryEnvelopeDecayRelay{parameterIds::auxiliaryEnvelopeDecay};
    juce::WebSliderRelay auxiliaryEnvelopeSustainRelay{parameterIds::auxiliaryEnvelopeSustain};
    juce::WebSliderRelay auxiliaryEnvelopeReleaseRelay{parameterIds::auxiliaryEnvelopeRelease};
    juce::WebSliderRelay lfo2RateRelay{parameterIds::lfoRate[1]};
    juce::WebSliderRelay lfo3RateRelay{parameterIds::lfoRate[2]};
    juce::WebSliderRelay lfo4RateRelay{parameterIds::lfoRate[3]};
    juce::WebComboBoxRelay subWaveformRelay{parameterIds::subWaveform};
    juce::WebComboBoxRelay noiseTypeRelay{parameterIds::noiseType};
    juce::WebComboBoxRelay lfo1ShapeRelay{parameterIds::lfoShape[0]};
    juce::WebComboBoxRelay lfo2ShapeRelay{parameterIds::lfoShape[1]};
    juce::WebComboBoxRelay lfo3ShapeRelay{parameterIds::lfoShape[2]};
    juce::WebComboBoxRelay lfo4ShapeRelay{parameterIds::lfoShape[3]};

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
    juce::WebSliderParameterAttachment oscillatorALevelAttachment;
    juce::WebSliderParameterAttachment subLevelAttachment;
    juce::WebSliderParameterAttachment noiseLevelAttachment;
    juce::WebSliderParameterAttachment ampDecayAttachment;
    juce::WebSliderParameterAttachment ampSustainAttachment;
    juce::WebSliderParameterAttachment filterKeyTrackingAttachment;
    juce::WebSliderParameterAttachment filterEnvelopeAmountAttachment;
    juce::WebSliderParameterAttachment filterEnvelopeAttackAttachment;
    juce::WebSliderParameterAttachment filterEnvelopeDecayAttachment;
    juce::WebSliderParameterAttachment filterEnvelopeSustainAttachment;
    juce::WebSliderParameterAttachment filterEnvelopeReleaseAttachment;
    juce::WebSliderParameterAttachment auxiliaryEnvelopeAttackAttachment;
    juce::WebSliderParameterAttachment auxiliaryEnvelopeDecayAttachment;
    juce::WebSliderParameterAttachment auxiliaryEnvelopeSustainAttachment;
    juce::WebSliderParameterAttachment auxiliaryEnvelopeReleaseAttachment;
    juce::WebSliderParameterAttachment lfo2RateAttachment;
    juce::WebSliderParameterAttachment lfo3RateAttachment;
    juce::WebSliderParameterAttachment lfo4RateAttachment;
    juce::WebComboBoxParameterAttachment subWaveformAttachment;
    juce::WebComboBoxParameterAttachment noiseTypeAttachment;
    juce::WebComboBoxParameterAttachment lfo1ShapeAttachment;
    juce::WebComboBoxParameterAttachment lfo2ShapeAttachment;
    juce::WebComboBoxParameterAttachment lfo3ShapeAttachment;
    juce::WebComboBoxParameterAttachment lfo4ShapeAttachment;
    std::unique_ptr<juce::FileChooser> wavetableChooser;
    bool wavetableChooserActive = false;
    std::unique_ptr<juce::FileChooser> midiExportChooser;
    bool midiExportChooserActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
}
