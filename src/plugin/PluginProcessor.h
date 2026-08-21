#pragma once

#include "synth/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace folkpark
{
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "folk park"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    void requestPanic() noexcept { panicRequested.store(true, std::memory_order_release); }
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return engine.getActiveVoiceCount(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] synth::ParameterSnapshot readSynthParameters() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    synth::SynthEngine engine;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;
    std::atomic<bool> panicRequested{false};

    std::atomic<float>* masterGainParameter = nullptr;
    std::atomic<float>* waveformParameter = nullptr;
    std::atomic<float>* oscillatorLevelParameter = nullptr;
    std::atomic<float>* subLevelParameter = nullptr;
    std::atomic<float>* cutoffParameter = nullptr;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;

    double activeSampleRate = 0.0;
    int activeBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}
