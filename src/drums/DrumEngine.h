#pragma once

#include "Rhythm.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace folkpark::drums
{
class DrumEngine final
{
public:
    static constexpr std::size_t voicesPerLane = 4;
    static constexpr std::size_t maximumVoices
        = voicesPerLane * static_cast<std::size_t>(DrumLane::count);

    void prepare(double newSampleRate, int maximumBlockSize,
                 const SynthDrumKit& newKit = {});
    [[nodiscard]] bool setKit(const SynthDrumKit& newKit) noexcept;
    void trigger(DrumLane lane, int velocity,
                 DrumArticulation articulation = DrumArticulation::normal) noexcept;
    void process(juce::AudioBuffer<float>& output, int startSample = 0,
                 int numberOfSamples = -1) noexcept;
    void reset() noexcept;

    [[nodiscard]] std::size_t activeVoiceCount() const noexcept;
    [[nodiscard]] const SynthDrumKit& currentKit() const noexcept { return kit; }

private:
    struct Voice
    {
        DrumLane lane = DrumLane::kick;
        bool active = false;
        double phaseA = 0.0;
        double phaseB = 0.0;
        float amplitude = 0.0f;
        float amplitudeMultiplier = 0.0f;
        float pitchEnvelope = 0.0f;
        float pitchMultiplier = 0.0f;
        float previousNoise = 0.0f;
        float velocity = 0.0f;
        std::uint32_t noiseState = 1;
        std::uint64_t age = 0;
    };

    [[nodiscard]] Voice& selectVoice(DrumLane lane) noexcept;
    [[nodiscard]] float renderVoice(Voice& voice) noexcept;
    [[nodiscard]] float nextNoise(Voice& voice) noexcept;
    [[nodiscard]] float decayMultiplier(float seconds) const noexcept;

    std::array<Voice, maximumVoices> voices{};
    SynthDrumKit kit;
    double sampleRate = 48000.0;
    std::uint64_t triggerCounter = 0;
};
}
