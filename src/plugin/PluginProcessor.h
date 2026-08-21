#pragma once

#include "synth/SynthEngine.h"
#include "synth/WavetableImportService.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <mutex>
#include <span>

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
    [[nodiscard]] bool publishWavetable(int oscillatorIndex, const synth::WavetableBank& bank) noexcept
    {
        return engine.publishWavetable(oscillatorIndex, bank);
    }
    [[nodiscard]] juce::Result setModulationRoutes(std::span<const synth::ModulationRoute> routes);
    [[nodiscard]] synth::ModulationSnapshot getConfiguredModulationRoutes() const;
    [[nodiscard]] juce::Result requestWavetableImport(const juce::File& file,
                                                      int oscillatorIndex,
                                                      int requestedCycleLength = 0)
    {
        return wavetableImportService.request(file, oscillatorIndex, requestedCycleLength);
    }
    [[nodiscard]] juce::Result confirmWavetableImport() { return wavetableImportService.confirm(); }
    void cancelWavetableImport() { wavetableImportService.cancel(); }
    [[nodiscard]] synth::WavetableImportService::Snapshot getWavetableImportSnapshot() const
    {
        return wavetableImportService.getSnapshot();
    }
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return engine.getActiveVoiceCount(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    [[nodiscard]] synth::ParameterSnapshot readSynthParameters() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    synth::SynthEngine engine;
    synth::WavetableImportService wavetableImportService;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;
    std::atomic<bool> panicRequested{false};

    struct OscillatorParameterPointers
    {
        std::atomic<float>* position = nullptr;
        std::atomic<float>* coarse = nullptr;
        std::atomic<float>* fine = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* randomPhase = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* unison = nullptr;
        std::atomic<float>* detune = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* blend = nullptr;
        std::atomic<float>* phaseReset = nullptr;
    };

    struct EnvelopeParameterPointers
    {
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
    };

    struct LfoParameterPointers
    {
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* syncDivision = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* tempoSync = nullptr;
        std::atomic<float>* retrigger = nullptr;
    };

    std::atomic<float>* masterGainParameter = nullptr;
    std::atomic<float>* waveformParameter = nullptr;
    OscillatorParameterPointers oscillatorAParameters;
    OscillatorParameterPointers oscillatorBParameters;
    std::atomic<float>* subWaveformParameter = nullptr;
    std::atomic<float>* subOctaveParameter = nullptr;
    std::atomic<float>* subLevelParameter = nullptr;
    std::atomic<float>* noiseTypeParameter = nullptr;
    std::atomic<float>* noiseLevelParameter = nullptr;
    std::atomic<float>* filterModeParameter = nullptr;
    std::atomic<float>* cutoffParameter = nullptr;
    std::atomic<float>* filterResonanceParameter = nullptr;
    std::atomic<float>* filterDriveParameter = nullptr;
    std::atomic<float>* filterKeyTrackingParameter = nullptr;
    std::atomic<float>* filterEnvelopeAmountParameter = nullptr;
    EnvelopeParameterPointers ampEnvelopeParameters;
    EnvelopeParameterPointers filterEnvelopeParameters;
    EnvelopeParameterPointers auxiliaryEnvelopeParameters;
    std::array<LfoParameterPointers, 4> lfoParameters{};
    mutable std::mutex modulationStateMutex;
    synth::ModulationSnapshot configuredModulationRoutes;

    double activeSampleRate = 0.0;
    int activeBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}
