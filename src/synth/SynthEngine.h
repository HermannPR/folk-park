#pragma once

#include "Modulation.h"
#include "WavetableExchange.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>

namespace folkpark::synth
{
enum class FilterMode : std::uint8_t
{
    lowPass,
    highPass,
    bandPass
};

enum class LfoShape : std::uint8_t
{
    sine,
    triangle,
    saw,
    square
};

struct EnvelopeParameters
{
    float attackSeconds = 0.01f;
    float decaySeconds = 0.15f;
    float sustainLevel = 0.8f;
    float releaseSeconds = 0.4f;
};

struct OscillatorParameters
{
    float position = 0.0f;
    float coarseSemitones = 0.0f;
    float fineCents = 0.0f;
    float phase = 0.0f;
    float randomPhase = 0.0f;
    float levelDb = -6.0f;
    float pan = 0.0f;
    int unisonVoices = 1;
    float unisonDetuneCents = 12.0f;
    float unisonSpread = 0.5f;
    float unisonBlend = 0.5f;
    bool phaseReset = true;
};

struct LfoParameters
{
    LfoShape shape = LfoShape::sine;
    float rateHz = 1.0f;
    int syncDivision = 4;
    float phase = 0.0f;
    bool tempoSync = false;
    bool retrigger = true;
};

struct ParameterSnapshot
{
    OscillatorParameters oscillatorA{};
    OscillatorParameters oscillatorB{.levelDb = -60.0f};
    int legacyOscillatorAWaveform = 0;

    int subWaveform = 0;
    int subOctave = -1;
    float subLevelDb = -18.0f;
    int noiseType = 0;
    float noiseLevelDb = -60.0f;

    FilterMode filterMode = FilterMode::lowPass;
    float filterCutoffHz = 12000.0f;
    float filterResonance = 0.1f;
    float filterDriveDb = 0.0f;
    float filterKeyTracking = 0.0f;
    float filterEnvelopeOctaves = 0.0f;

    EnvelopeParameters ampEnvelope{};
    EnvelopeParameters filterEnvelope{.attackSeconds = 0.01f,
                                      .decaySeconds = 0.2f,
                                      .sustainLevel = 0.0f,
                                      .releaseSeconds = 0.3f};
    EnvelopeParameters auxiliaryEnvelope{.attackSeconds = 0.05f,
                                         .decaySeconds = 0.3f,
                                         .sustainLevel = 0.5f,
                                         .releaseSeconds = 0.5f};
    std::array<LfoParameters, 4> lfos{};
    double tempoBpm = 120.0;
};

class SynthEngine final
{
public:
    static constexpr int maximumVoices = 16;
    static constexpr int maximumUnisonVoices = 8;

    SynthEngine();

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;
    void panic() noexcept;
    void process(juce::AudioBuffer<float>& output,
                 const juce::MidiBuffer& midi,
                 const ParameterSnapshot& parameters) noexcept;

    [[nodiscard]] bool publishWavetable(int oscillatorIndex, const WavetableBank& bank) noexcept;
    [[nodiscard]] bool publishModulationRoutes(std::span<const ModulationRoute> routes) noexcept;
    [[nodiscard]] bool publishPresetSnapshot(const WavetableBank& oscillatorA,
                                             const WavetableBank& oscillatorB,
                                             std::span<const ModulationRoute> routes) noexcept;
    [[nodiscard]] const ModulationSnapshot& getActiveModulationSnapshot() const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] std::uint64_t getVoiceStealCount() const noexcept
    {
        return voiceStealCount.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool isNoteActive(int midiChannel, int midiNote) const noexcept;

private:
    enum class EnvelopeStage : std::uint8_t
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    struct EnvelopeState
    {
        void start(double currentSampleRate, const EnvelopeParameters& parameters) noexcept;
        void release(double currentSampleRate, const EnvelopeParameters& parameters) noexcept;
        void reset() noexcept;
        [[nodiscard]] float next(const EnvelopeParameters& parameters) noexcept;
        [[nodiscard]] bool isIdle() const noexcept { return stage == EnvelopeStage::idle; }
        [[nodiscard]] bool isReleased() const noexcept { return stage == EnvelopeStage::release; }

        EnvelopeStage stage = EnvelopeStage::idle;
        float level = 0.0f;
        float attackIncrement = 1.0f;
        float decayDecrement = 1.0f;
        float releaseDecrement = 1.0f;
    };

    struct FilterState
    {
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;
    };

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    struct Voice
    {
        void start(int channel,
                   int note,
                   float noteVelocity,
                   std::uint64_t age,
                   double currentSampleRate,
                   const ParameterSnapshot& parameters) noexcept;
        void release(double currentSampleRate, const ParameterSnapshot& parameters, bool allowTail) noexcept;
        void reset() noexcept;
        [[nodiscard]] StereoSample render(const WavetableExchange::RenderView& oscillatorA,
                                          const WavetableExchange::RenderView& oscillatorB,
                                          const ModulationSnapshot& routes,
                                          const std::array<float, 4>& globalLfoValues,
                                          float bendSemitones,
                                          float currentModWheel,
                                          float currentChannelPressure,
                                          double currentSampleRate,
                                          float smoothingAmount,
                                          const ParameterSnapshot& parameters) noexcept;

        [[nodiscard]] StereoSample renderOscillator(const WavetableExchange::RenderView& bank,
                                                    int oscillatorIndex,
                                                    const OscillatorParameters& parameters,
                                                    float positionModulation,
                                                    float pitchModulation,
                                                    float levelModulationDb,
                                                    float panModulation,
                                                    float bendSemitones,
                                                    double currentSampleRate) noexcept;
        [[nodiscard]] float nextLfo(int index,
                                    const LfoParameters& parameters,
                                    float globalValue,
                                    double tempoBpm,
                                    double currentSampleRate) noexcept;
        [[nodiscard]] float processFilter(float input,
                                          FilterState& state,
                                          FilterMode mode,
                                          float cutoff,
                                          float resonance,
                                          float driveDb,
                                          double currentSampleRate) noexcept;

        int midiChannel = 0;
        int midiNote = -1;
        float velocity = 0.0f;
        float currentPitch = 60.0f;
        float targetPitch = 60.0f;
        std::uint64_t startAge = 0;
        bool sustained = false;

        std::array<std::array<float, maximumUnisonVoices>, 2> oscillatorPhases{};
        std::array<std::array<float, maximumUnisonVoices>, 2> cachedOscillatorNotes{};
        std::array<std::array<float, maximumUnisonVoices>, 2> cachedOscillatorFrequencies{};
        std::array<std::array<int, maximumUnisonVoices>, 2> cachedOscillatorMipLevels{};
        float subPhase = 0.0f;
        std::array<float, 4> lfoPhases{};
        std::uint32_t noiseState = 1;
        float pinkNoiseState = 0.0f;

        EnvelopeState ampEnvelope;
        EnvelopeState filterEnvelope;
        EnvelopeState auxiliaryEnvelope;
        std::array<FilterState, 2> filterStates{};
        ModulationRegistry::DestinationValues modulationCache{};

        float smoothedOscillatorAPosition = 0.0f;
        float smoothedOscillatorBPosition = 0.0f;
        float smoothedOscillatorALevelDb = -6.0f;
        float smoothedOscillatorBLevelDb = -60.0f;
        float smoothedSubLevelDb = -18.0f;
        float smoothedNoiseLevelDb = -60.0f;
        float smoothedFilterCutoffHz = 12000.0f;
        float smoothedFilterResonance = 0.1f;
        float smoothedFilterDriveDb = 0.0f;
        float smoothedPan = 0.0f;
        std::array<float, 2> smoothedOscillatorPitchOffset{};
        std::array<float, 2> smoothedOscillatorPan{};
        std::array<float, 2> smoothedOscillatorDetune{};
        std::array<float, 2> smoothedOscillatorSpread{};
        std::array<float, 2> smoothedOscillatorBlend{};
        std::array<std::array<float, maximumUnisonVoices>, 2> smoothedUnisonWeights{};
        std::array<std::array<float, maximumUnisonVoices>, 2> smoothedUnisonLanePositions{};
    };

    void handleMidiMessage(const juce::MidiMessage& message,
                           const ParameterSnapshot& parameters) noexcept;
    void startVoice(int channel,
                    int note,
                    float velocity,
                    const ParameterSnapshot& parameters) noexcept;
    void releaseVoices(int channel,
                       int note,
                       const ParameterSnapshot& parameters) noexcept;
    Voice& chooseVoiceToStart() noexcept;
    void renderRange(juce::AudioBuffer<float>& output,
                     int startSample,
                     int endSample,
                     const ParameterSnapshot& parameters) noexcept;
    void publishActiveVoiceCount() noexcept;
    [[nodiscard]] std::array<float, 4> currentGlobalLfoValues(const ParameterSnapshot& parameters) const noexcept;
    void advanceGlobalLfos(const ParameterSnapshot& parameters) noexcept;

    std::array<Voice, maximumVoices> voices{};
    WavetableExchange oscillatorABank;
    WavetableExchange oscillatorBBank;
    ModulationExchange modulationExchange;
    std::atomic_flag publicationWriter = ATOMIC_FLAG_INIT;
    std::atomic<bool> publicationProducerActive{false};
    std::atomic<bool> publicationConsumerActive{false};
    std::atomic<int> activeVoiceCount{0};
    std::atomic<std::uint64_t> voiceStealCount{0};
    std::array<float, 4> globalLfoPhases{};
    double sampleRate = 44100.0;
    std::uint64_t voiceAgeCounter = 0;
    float pitchBendSemitones = 0.0f;
    float modWheel = 0.0f;
    float channelPressure = 0.0f;
    float smoothingCoefficient = 1.0f;
    bool sustainPedalDown = false;
};
}
