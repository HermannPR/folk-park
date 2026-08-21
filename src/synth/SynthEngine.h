#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace folkpark::synth
{
struct ParameterSnapshot
{
    int waveform = 0;
    float oscillatorLevelDb = -6.0f;
    float subLevelDb = -18.0f;
    float filterCutoffHz = 12000.0f;
    float attackSeconds = 0.01f;
    float decaySeconds = 0.15f;
    float sustainLevel = 0.8f;
    float releaseSeconds = 0.4f;
};

class SynthEngine final
{
public:
    static constexpr int maximumVoices = 16;
    static constexpr int wavetableSize = 2048;

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;
    void panic() noexcept;
    void process(juce::AudioBuffer<float>& output,
                 const juce::MidiBuffer& midi,
                 const ParameterSnapshot& parameters) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
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

    struct Voice
    {
        void start(int channel,
                   int note,
                   float noteVelocity,
                   std::uint64_t age,
                   double currentSampleRate,
                   const ParameterSnapshot& parameters) noexcept;
        void release(const ParameterSnapshot& parameters,
                     bool allowTail,
                     double currentSampleRate) noexcept;
        void reset() noexcept;
        float render(const std::array<float, wavetableSize>& table,
                     const std::array<float, wavetableSize>& sineTable,
                     float oscillatorGain,
                     float subGain,
                     float lowPassCoefficient,
                     float pitchBendSemitones,
                     double currentSampleRate,
                     const ParameterSnapshot& parameters) noexcept;
        void updateEnvelopeRates(double currentSampleRate,
                                 const ParameterSnapshot& parameters) noexcept;
        float nextEnvelopeSample(const ParameterSnapshot& parameters) noexcept;

        EnvelopeStage stage = EnvelopeStage::idle;
        int midiChannel = 0;
        int midiNote = -1;
        float velocity = 0.0f;
        float phase = 0.0f;
        float subPhase = 0.0f;
        float envelopeLevel = 0.0f;
        float attackIncrement = 1.0f;
        float decayDecrement = 1.0f;
        float releaseDecrement = 1.0f;
        float filterState = 0.0f;
        std::uint64_t startAge = 0;
        bool sustained = false;
    };

    void initialiseWavetables() noexcept;
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

    std::array<std::array<float, wavetableSize>, 2> wavetables{};
    std::array<Voice, maximumVoices> voices{};
    std::atomic<int> activeVoiceCount{0};
    double sampleRate = 44100.0;
    std::uint64_t voiceAgeCounter = 0;
    float pitchBendSemitones = 0.0f;
    bool sustainPedalDown = false;
};
}
