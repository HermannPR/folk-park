#include "effects/EffectChain.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
constexpr int blockSize = 512;
constexpr double sampleRate = 48000.0;
int failures = 0;

void expect(bool value, const char* message)
{
    if (!value)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

juce::AudioBuffer<float> fixture(int samples, double rate = sampleRate)
{
    juce::AudioBuffer<float> audio(2, samples);
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto phase = juce::MathConstants<float>::twoPi * 440.0f
            * static_cast<float>(sample) / static_cast<float>(rate);
        const auto value = 0.2f * std::sin(phase);
        audio.setSample(0, sample, value);
        audio.setSample(1, sample, value * 0.8f);
    }
    return audio;
}

bool finiteAndBounded(const juce::AudioBuffer<float>& audio)
{
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            if (!std::isfinite(audio.getSample(channel, sample))
                || std::abs(audio.getSample(channel, sample)) > 32.0f)
                return false;
    return true;
}

float maximumDifference(const juce::AudioBuffer<float>& first,
                        const juce::AudioBuffer<float>& second)
{
    auto difference = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            difference = std::max(difference, std::abs(first.getSample(channel, sample)
                                                        - second.getSample(channel, sample)));
    return difference;
}

bool buffersMatch(const juce::AudioBuffer<float>& first,
                  const juce::AudioBuffer<float>& second,
                  float tolerance = 0.0f)
{
    return maximumDifference(first, second) <= tolerance;
}

folkpark::effects::Parameters onlyEffectEnabled(int stage)
{
    folkpark::effects::Parameters parameters;
    switch (stage)
    {
        case 0: parameters.distortionBypass = false; parameters.distortionDriveDb = 24.0f; break;
        case 1: parameters.chorusBypass = false; parameters.chorusMix = 0.8f; break;
        case 2:
            parameters.delayBypass = false;
            parameters.delayDivision = 4;
            parameters.delayMix = 0.7f;
            break;
        case 3: parameters.reverbBypass = false; parameters.reverbMix = 0.8f; break;
        case 4:
            parameters.compressorBypass = false;
            parameters.compressorThresholdDb = -36.0f;
            parameters.compressorAttackMs = 0.1f;
            break;
        case 5:
            parameters.eqBypass = false;
            parameters.eqLowGainDb = 12.0f;
            parameters.eqMidGainDb = -12.0f;
            parameters.eqHighGainDb = 9.0f;
            break;
        default: break;
    }
    return parameters;
}

void testSafeDefaultsAndIndividualStages()
{
    folkpark::effects::EffectChain chain;
    chain.prepare(sampleRate, blockSize);
    folkpark::effects::Parameters parameters;
    const auto dry = fixture(blockSize);
    auto bypassed = dry;
    chain.process(bypassed, parameters);
    expect(buffersMatch(dry, bypassed),
           "All six default bypasses must preserve audio bit-for-bit");

    for (int stage = 0; stage < 6; ++stage)
    {
        chain.prepare(sampleRate, blockSize);
        const auto isolated = onlyEffectEnabled(stage);
        auto changed = false;
        for (int block = 0; block < 24; ++block)
        {
            auto input = fixture(blockSize);
            const auto original = input;
            chain.process(input, isolated);
            changed = changed || maximumDifference(input, original) > 1.0e-4f;
            expect(finiteAndBounded(input), "Every isolated effect must produce finite bounded audio");
        }
        expect(changed, "Every advertised effect stage must audibly affect its isolated test signal");
    }
}

int detectDelayEcho(double tempoBpm)
{
    folkpark::effects::EffectChain chain;
    chain.prepare(sampleRate, blockSize);
    auto parameters = onlyEffectEnabled(2);
    parameters.delayMix = 1.0f;
    parameters.delayFeedback = 0.0f;
    parameters.delayDivision = 4;
    parameters.tempoBpm = tempoBpm;
    auto absoluteSample = 0;
    for (int block = 0; block < 32; ++block)
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        audio.clear();
        if (block == 0)
            audio.setSample(0, 0, 1.0f);
        chain.process(audio, parameters);
        for (int sample = 1; sample < blockSize; ++sample)
            if (std::abs(audio.getSample(0, sample)) > 0.5f)
                return absoluteSample + sample;
        absoluteSample += blockSize;
    }
    return -1;
}

void testTempoSyncAndBypassContinuity()
{
    expect(std::abs(detectDelayEcho(240.0) - 3000) <= 1,
           "A 1/16 delay must land at 3,000 samples at 240 BPM and 48 kHz");
    expect(std::abs(detectDelayEcho(120.0) - 6000) <= 1,
           "A 1/16 delay must land at 6,000 samples at 120 BPM and 48 kHz");

    folkpark::effects::EffectChain chain;
    chain.prepare(sampleRate, blockSize);
    folkpark::effects::Parameters parameters;
    juce::AudioBuffer<float> audio(2, blockSize);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < blockSize; ++sample)
            audio.setSample(channel, sample, 0.2f);
    chain.process(audio, parameters);
    const auto precedingDry = audio.getSample(0, blockSize - 1);
    parameters.distortionBypass = false;
    chain.process(audio, parameters);
    expect(std::abs(audio.getSample(0, 0) - precedingDry) < 0.01f,
           "Enabling an effect must begin with a bounded 10 ms crossfade rather than a click");
    const auto precedingWet = audio.getSample(0, blockSize - 1);
    parameters.distortionBypass = true;
    chain.process(audio, parameters);
    expect(std::abs(audio.getSample(0, 0) - precedingWet) < 0.01f,
           "Bypassing an effect must begin with a bounded 10 ms crossfade rather than a click");
}

void testRatesBlocksResetAndMalformedValues()
{
    auto parameters = onlyEffectEnabled(0);
    parameters.chorusBypass = parameters.delayBypass = parameters.reverbBypass = false;
    parameters.compressorBypass = parameters.eqBypass = false;
    parameters.eqMidGainDb = 6.0f;
    folkpark::effects::EffectChain chain;
    for (const auto rate : {44100.0, 48000.0, 96000.0})
    {
        chain.prepare(rate, blockSize);
        for (const auto samples : {1, 7, 64, 511, 512})
        {
            auto processed = fixture(samples, rate);
            chain.process(processed, parameters);
            expect(finiteAndBounded(processed),
                   "The ordered chain must remain finite across supported rates and block sizes");
        }
    }

    chain.prepare(sampleRate, blockSize);
    chain.reset();
    auto first = fixture(blockSize);
    chain.process(first, parameters);
    chain.reset();
    auto second = fixture(blockSize);
    chain.process(second, parameters);
    expect(buffersMatch(first, second), "Reset must make the complete effect chain deterministic");

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    parameters.distortionDriveDb = parameters.distortionMix = parameters.distortionOutputDb = nan;
    parameters.chorusRateHz = parameters.chorusDepthMs = parameters.chorusMix = nan;
    parameters.delayFeedback = parameters.delayMix = nan;
    parameters.reverbRoomSize = parameters.reverbDamping = parameters.reverbMix = nan;
    parameters.compressorThresholdDb = parameters.compressorRatio = nan;
    parameters.compressorAttackMs = parameters.compressorReleaseMs = nan;
    parameters.compressorMakeupDb = parameters.compressorMix = nan;
    parameters.eqLowGainDb = parameters.eqMidFrequencyHz = parameters.eqMidGainDb = nan;
    parameters.eqMidQ = parameters.eqHighGainDb = nan;
    parameters.tempoBpm = std::numeric_limits<double>::quiet_NaN();
    auto malformed = fixture(blockSize);
    malformed.setSample(0, 0, std::numeric_limits<float>::infinity());
    malformed.setSample(1, 1, nan);
    chain.process(malformed, parameters);
    expect(finiteAndBounded(malformed),
           "Malformed state and non-finite input must be contained at the DSP boundary");
}
}

int main()
{
    testSafeDefaultsAndIndividualStages();
    testTempoSyncAndBypassContinuity();
    testRatesBlocksResetAndMalformedValues();
    if (failures == 0)
        std::cout << "PASS: M5 ordered effects are effective, tempo-synced, finite, reset-safe, and click-safe\n";
    return failures == 0 ? 0 : 1;
}
