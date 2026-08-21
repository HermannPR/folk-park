#pragma once

#include "effects/EffectChain.h"
#include "midi/CompositionSession.h"
#include "midi/MidiDelivery.h"
#include "midi/PreviewMidi.h"
#include "synth/SynthEngine.h"
#include "synth/WavetableImportService.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <mutex>
#include <span>

namespace folkpark
{
struct WavetableUiSnapshot
{
    static constexpr int samplesPerFrame = 96;
    static constexpr int maximumFrames = synth::WavetableBank::maximumFrames;

    int frameCount = 0;
    std::array<float, static_cast<std::size_t>(samplesPerFrame * maximumFrames)> samples{};
};

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
    [[nodiscard]] bool undoLastParameterChange()
    {
        // APVTS normally flushes parameter changes into its ValueTree on a timer.
        // Synchronise first so an immediate UI Undo sees the gesture that just ended.
        (void) parameters.copyState();
        return undoManager.canUndo() && undoManager.undo();
    }
    [[nodiscard]] bool redoLastParameterChange()
    {
        return undoManager.canRedo() && undoManager.redo();
    }
    void requestPanic() noexcept
    {
        panicRequested.store(true, std::memory_order_release);
        directMidiPlayer.requestStop();
        previewMidiQueue.requestReleaseAll();
    }
    [[nodiscard]] juce::Result generateCompositionCandidate(midi::MusicIntent intent)
    {
        return compositionSession.generateCandidate(std::move(intent));
    }
    [[nodiscard]] juce::Result generateMoreLikeComposition(std::uint32_t variationIndex)
    {
        return compositionSession.moreLikeCandidate(variationIndex);
    }
    [[nodiscard]] juce::Result generateSurpriseComposition(std::uint32_t surpriseIndex)
    {
        return compositionSession.surpriseCandidate(surpriseIndex);
    }
    [[nodiscard]] juce::Result adjustCompositionCandidateNote(std::size_t sourceIndex,
                                                              int pitchDelta,
                                                              std::int64_t startDeltaTicks,
                                                              std::int64_t durationDeltaTicks,
                                                              int velocityDelta)
    {
        return compositionSession.adjustCandidateNote(sourceIndex, pitchDelta, startDeltaTicks,
                                                      durationDeltaTicks, velocityDelta);
    }
    [[nodiscard]] juce::Result acceptCompositionCandidate()
    {
        return compositionSession.acceptCandidate();
    }
    [[nodiscard]] midi::CompositionSessionSnapshot getCompositionSnapshot() const
    {
        return compositionSession.getSnapshot();
    }
    [[nodiscard]] midi::PianoRollPreview getCompositionPreview() const
    {
        return compositionSession.getCandidatePreview();
    }
    [[nodiscard]] juce::File writeAcceptedMidiToTemporaryFile() const;
    [[nodiscard]] juce::Result writeAcceptedMidiFile(const juce::File& destination) const;
    [[nodiscard]] juce::Result routeAcceptedMidi();
    void stopDirectMidi() noexcept { directMidiPlayer.requestStop(); }
    [[nodiscard]] bool isDirectMidiPlaying() const noexcept
    {
        return directMidiPlayer.isPlaying() || directMidiPlayer.hasPendingSchedule();
    }
    [[nodiscard]] bool previewNoteOn(int note, int velocity) noexcept
    {
        return previewMidiQueue.enqueueNoteOn(note, velocity);
    }
    [[nodiscard]] bool previewNoteOff(int note) noexcept
    {
        return previewMidiQueue.enqueueNoteOff(note);
    }
    void releasePreviewNotes() noexcept { previewMidiQueue.requestReleaseAll(); }
    [[nodiscard]] bool publishWavetable(int oscillatorIndex,
                                        const synth::WavetableBank& bank);
    [[nodiscard]] WavetableUiSnapshot getWavetableUiSnapshot(int oscillatorIndex) const;
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
    [[nodiscard]] effects::Parameters readEffectsParameters(double tempoBpm) const noexcept;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState parameters;
    synth::SynthEngine engine;
    effects::EffectChain effectChain;
    synth::WavetableImportService wavetableImportService;
    midi::CompositionSession compositionSession;
    midi::DirectMidiPlayer directMidiPlayer;
    midi::PreviewMidiQueue previewMidiQueue;
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
    struct EffectParameterPointers
    {
        std::atomic<float>* distortionBypass = nullptr;
        std::atomic<float>* distortionDrive = nullptr;
        std::atomic<float>* distortionMix = nullptr;
        std::atomic<float>* distortionOutput = nullptr;
        std::atomic<float>* chorusBypass = nullptr;
        std::atomic<float>* chorusRate = nullptr;
        std::atomic<float>* chorusDepth = nullptr;
        std::atomic<float>* chorusMix = nullptr;
        std::atomic<float>* delayBypass = nullptr;
        std::atomic<float>* delayDivision = nullptr;
        std::atomic<float>* delayFeedback = nullptr;
        std::atomic<float>* delayMix = nullptr;
        std::atomic<float>* reverbBypass = nullptr;
        std::atomic<float>* reverbRoomSize = nullptr;
        std::atomic<float>* reverbDamping = nullptr;
        std::atomic<float>* reverbMix = nullptr;
        std::atomic<float>* compressorBypass = nullptr;
        std::atomic<float>* compressorThreshold = nullptr;
        std::atomic<float>* compressorRatio = nullptr;
        std::atomic<float>* compressorAttack = nullptr;
        std::atomic<float>* compressorRelease = nullptr;
        std::atomic<float>* compressorMakeup = nullptr;
        std::atomic<float>* compressorMix = nullptr;
        std::atomic<float>* eqBypass = nullptr;
        std::atomic<float>* eqLowGain = nullptr;
        std::atomic<float>* eqMidFrequency = nullptr;
        std::atomic<float>* eqMidGain = nullptr;
        std::atomic<float>* eqMidQ = nullptr;
        std::atomic<float>* eqHighGain = nullptr;
    } effectParameters;
    mutable std::mutex modulationStateMutex;
    synth::ModulationSnapshot configuredModulationRoutes;
    mutable std::mutex wavetableUiMutex;
    std::array<WavetableUiSnapshot, 2> wavetableUiSnapshots{};

    double activeSampleRate = 0.0;
    int activeBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}
