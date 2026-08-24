#pragma once

#include "assistant/AssistantAudition.h"
#include "assistant/OfflineAssistant.h"
#include "diagnostics/Diagnostics.h"
#include "effects/EffectChain.h"
#include "midi/CompositionSession.h"
#include "midi/MidiDelivery.h"
#include "midi/PreviewMidi.h"
#include "persistence/PersistenceCoordinator.h"
#include "render/OfflinePreviewRenderer.h"
#include "synth/SynthEngine.h"
#include "synth/WavetableImportService.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace folkpark
{
struct WavetableUiSnapshot
{
    static constexpr int samplesPerFrame = 96;
    static constexpr int maximumFrames = synth::WavetableBank::maximumFrames;

    int frameCount = 0;
    std::array<float, static_cast<std::size_t>(samplesPerFrame * maximumFrames)> samples{};
};

struct PresetSaveRequest
{
    juce::String name;
    juce::String author;
    std::vector<juce::String> tags;
    juce::String genre;
    juce::String emotion;
    juce::String description;
    bool favorite = false;
    bool allowOverwrite = false;
};

struct HistoryEntryDetail
{
    persistence::HistorySummary summary;
    midi::MusicIntent intent;
    int clipCount = 0;
    int noteCount = 0;
};

class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    using PersistenceConfiguration = persistence::PersistenceConfiguration;

    PluginProcessor();
    explicit PluginProcessor(PersistenceConfiguration configuration);
    ~PluginProcessor() override;

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
    [[nodiscard]] juce::Result acceptCompositionCandidate();
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
    [[nodiscard]] juce::Result requestAcceptedWavRender(const juce::File& destination,
                                                        bool allowOverwrite);
    void cancelAcceptedWavRender() { offlinePreviewService.cancel(); }
    [[nodiscard]] render::OfflinePreviewService::Snapshot getAcceptedWavRenderSnapshot() const
    {
        return offlinePreviewService.getSnapshot();
    }
    [[nodiscard]] juce::Result routeAcceptedMidi();
    void stopDirectMidi() noexcept { directMidiPlayer.requestStop(); }
    [[nodiscard]] bool isDirectMidiPlaying() const noexcept
    {
        return directMidiPlayer.isPlaying() || directMidiPlayer.hasPendingSchedule();
    }
    [[nodiscard]] bool previewNoteOn(int note, int velocity) noexcept;
    [[nodiscard]] bool previewNoteOff(int note) noexcept;
    void releasePreviewNotes() noexcept { previewMidiQueue.requestReleaseAll(); }
    [[nodiscard]] bool publishWavetable(int oscillatorIndex,
                                        const synth::WavetableBank& bank);
    [[nodiscard]] WavetableUiSnapshot getWavetableUiSnapshot(int oscillatorIndex) const;
    [[nodiscard]] juce::Result setModulationRoutes(std::span<const synth::ModulationRoute> routes);
    [[nodiscard]] synth::ModulationSnapshot getConfiguredModulationRoutes() const;
    [[nodiscard]] std::vector<assistant::CurrentParameterValue>
        getAssistantParameterSnapshot() const;
    [[nodiscard]] juce::Result beginAssistantProposal(
        const assistant::ParameterProposal& proposal);
    [[nodiscard]] juce::Result auditionAssistantSide(assistant::AuditionSide side);
    [[nodiscard]] juce::Result acceptAssistantProposal();
    [[nodiscard]] juce::Result rejectAssistantProposal();
    [[nodiscard]] assistant::AssistantAuditionSnapshot getAssistantAuditionSnapshot() const;
    [[nodiscard]] assistant::GuidedProgress getAssistantQuestions(
        const assistant::SoundIntent& intent) const
    {
        return offlineAssistant.questionsFor(intent);
    }
    [[nodiscard]] assistant::AssistantProviderResult runOfflineAssistant(
        const assistant::AssistantRequest& request) const
    {
        return request.target == assistant::AssistantTarget::sound
            ? offlineAssistant.respond(request, getAssistantParameterSnapshot())
            : offlineAssistant.respond(request);
    }
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
    [[nodiscard]] diagnostics::Snapshot getDiagnosticsSnapshot() const;
    [[nodiscard]] juce::Result initialisePersistence();
    [[nodiscard]] persistence::PersistenceStatusSnapshot getPersistenceStatus() const;
    [[nodiscard]] persistence::PresetLibraryResult listPresets();
    [[nodiscard]] juce::Result saveCurrentPreset(const PresetSaveRequest& request);
    [[nodiscard]] juce::Result loadLibraryPreset(const juce::String& presetId);
    [[nodiscard]] juce::Result importExternalPreset(const juce::File& file);
    [[nodiscard]] juce::Result relinkPendingPresetAsset(persistence::AssetSlot slot,
                                                       const juce::File& selectedFile);
    [[nodiscard]] juce::Result setPresetFavorite(const juce::String& presetId, bool favorite);
    [[nodiscard]] persistence::HistorySearchResult searchHistory(
        const persistence::HistorySearchQuery& query);
    [[nodiscard]] juce::Result recallHistory(const juce::String& historyId);
    [[nodiscard]] std::optional<HistoryEntryDetail> inspectHistory(
        const juce::String& historyId);
    [[nodiscard]] juce::Result setHistoryFavorite(const juce::String& historyId,
                                                  bool favorite);
    [[nodiscard]] juce::Result setHistorySoftDeleted(const juce::String& historyId,
                                                     bool deleted);
    [[nodiscard]] juce::Result setHistoryRetentionDays(int days);
    [[nodiscard]] persistence::HistoryCleanupResult cleanupHistory(bool keepFavorites);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void parameterChanged(const juce::String&, float) override;
    [[nodiscard]] synth::ParameterSnapshot readSynthParameters() const noexcept;
    [[nodiscard]] effects::Parameters readEffectsParameters(double tempoBpm) const noexcept;
    [[nodiscard]] render::OfflinePreviewSnapshot makeOfflinePreviewSnapshot(double tempoBpm) const;
    [[nodiscard]] persistence::PresetDocument captureCurrentPreset(
        const persistence::PresetMetadata& metadata) const;
    [[nodiscard]] juce::Result applyPresetCandidate(
        const persistence::PresetCandidateResult& candidate);
    [[nodiscard]] juce::Result publishImportedWavetable(
        int oscillatorIndex,
        const synth::WavetableBank& bank,
        const synth::WavetableConverter::Metadata& metadata,
        const juce::File& source);
    void recordAcceptedComposition(const midi::CompositionBundle& bundle);
    void applyAssistantParameterValues(
        std::span<const assistant::CurrentParameterValue> values);
    void resetAssistantAudition();
    void clearPendingProjectRestore();
    [[nodiscard]] juce::Result completeProjectRestore(
        const persistence::PresetDocument& document,
        std::optional<midi::CompositionBundle> acceptedBundle,
        std::optional<assistant::AssistantAuditionSnapshot> assistantSnapshot,
        bool dirty,
        const juce::String& historyEntryId);

    struct PendingProjectRestore
    {
        std::optional<midi::CompositionBundle> acceptedBundle;
        std::optional<assistant::AssistantAuditionSnapshot> assistantSnapshot;
        bool dirty = false;
        juce::String historyEntryId;
    };

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState parameters;
    synth::SynthEngine engine;
    effects::EffectChain effectChain;
    synth::WavetableImportService wavetableImportService;
    midi::CompositionSession compositionSession;
    midi::DirectMidiPlayer directMidiPlayer;
    midi::PreviewMidiQueue previewMidiQueue;
    render::OfflinePreviewService offlinePreviewService;
    std::unique_ptr<persistence::PersistenceCoordinator> persistenceCoordinator;
    mutable std::mutex projectStateMutex;
    std::optional<PendingProjectRestore> pendingProjectRestore;
    std::atomic<std::uint64_t> parameterRevision{0};
    std::atomic<std::uint64_t> cleanParameterRevision{0};
    struct AssistantRevisionBoundary
    {
        std::uint64_t base = 0;
        std::uint64_t expected = 0;
    };
    mutable std::mutex assistantAuditionMutex;
    assistant::AssistantAuditionSession assistantAudition;
    std::optional<AssistantRevisionBoundary> assistantRevisionBoundary;
    assistant::OfflineAssistantEngine offlineAssistant;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;
    std::atomic<bool> panicRequested{false};
    std::atomic<std::uint64_t> nonFiniteOutputSamples{0};
    std::atomic<std::uint64_t> directMidiOverflows{0};
    std::atomic<std::uint64_t> previewMidiOverflows{0};
    std::atomic<std::uint64_t> rejectedProjectStates{0};

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
    std::array<std::shared_ptr<const synth::WavetableBank>, 2> currentWavetables{};
    std::array<std::optional<persistence::AssetReference>, 2> currentWavetableAssets{};
    mutable std::mutex historyLineageMutex;
    juce::String lastHistoryEntryId;
    std::vector<juce::String> lastHistoryClipIds;

    std::atomic<double> activeSampleRate{0.0};
    std::atomic<int> activeBlockSize{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}
