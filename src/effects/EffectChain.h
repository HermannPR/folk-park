#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace folkpark::effects
{
struct Parameters
{
    bool distortionBypass = true;
    float distortionDriveDb = 6.0f;
    float distortionMix = 0.5f;
    float distortionOutputDb = -6.0f;

    bool chorusBypass = true;
    float chorusRateHz = 0.35f;
    float chorusDepthMs = 6.0f;
    float chorusMix = 0.25f;

    bool delayBypass = true;
    int delayDivision = 2;
    float delayFeedback = 0.3f;
    float delayMix = 0.25f;

    bool reverbBypass = true;
    float reverbRoomSize = 0.45f;
    float reverbDamping = 0.4f;
    float reverbMix = 0.2f;

    bool compressorBypass = true;
    float compressorThresholdDb = -18.0f;
    float compressorRatio = 4.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 100.0f;
    float compressorMakeupDb = 0.0f;
    float compressorMix = 1.0f;

    bool eqBypass = true;
    float eqLowGainDb = 0.0f;
    float eqMidFrequencyHz = 1000.0f;
    float eqMidGainDb = 0.0f;
    float eqMidQ = 1.0f;
    float eqHighGainDb = 0.0f;

    double tempoBpm = 120.0;
};

class EffectChain final
{
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;

private:
    struct Biquad
    {
        void reset() noexcept;
        void setPeak(float frequency, float q, float gainDb, double sampleRate) noexcept;
        void setShelf(float frequency, float gainDb, bool high, double sampleRate) noexcept;
        float process(float input, int channel) noexcept;

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        std::array<float, 2> z1{}, z2{};
    };

    void copyDry(const juce::AudioBuffer<float>& audio) noexcept;
    void blendBypass(juce::AudioBuffer<float>& audio, int stage, bool bypass) noexcept;
    void distortion(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;
    void chorus(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;
    void delay(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;
    void reverb(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;
    void compressor(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;
    void equalizer(juce::AudioBuffer<float>& audio, const Parameters& parameters) noexcept;

    std::vector<float> scratchLeft, scratchRight;
    std::vector<float> chorusLeft, chorusRight;
    std::vector<float> delayLeft, delayRight;
    std::size_t chorusWrite = 0, delayWrite = 0;
    float chorusPhase = 0.0f;
    float compressorEnvelope = 0.0f;
    std::array<float, 6> bypassAmounts{};
    Biquad lowShelf, midPeak, highShelf;
    juce::Reverb reverbProcessor;
    double currentSampleRate = 44100.0;
    int preparedBlockSize = 0;
};
}
