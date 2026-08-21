#include "EffectChain.h"

#include <algorithm>
#include <cmath>

namespace folkpark::effects
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr std::array<float, 5> delayBeats{4.0f, 2.0f, 1.0f, 0.5f, 0.25f};

template <typename Value>
Value finiteOr(Value value, Value fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

template <typename Value>
Value clamped(Value minimum, Value maximum, Value value, Value fallback) noexcept
{
    return juce::jlimit(minimum, maximum, finiteOr(value, fallback));
}

float gainFromDb(float value) noexcept
{
    return std::pow(10.0f, finiteOr(value, 0.0f) / 20.0f);
}

float bounded(float value) noexcept
{
    return std::isfinite(value) ? juce::jlimit(-32.0f, 32.0f, value) : 0.0f;
}

float readDelay(const std::vector<float>& line, std::size_t write, float samples) noexcept
{
    const auto size = line.size();
    auto position = static_cast<float>(write) - samples;
    while (position < 0.0f)
        position += static_cast<float>(size);
    const auto first = static_cast<std::size_t>(position) % size;
    const auto second = (first + 1) % size;
    const auto fraction = position - std::floor(position);
    return line[first] + fraction * (line[second] - line[first]);
}
}

void EffectChain::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = clamped(8000.0, 192000.0, sampleRate, 44100.0);
    preparedBlockSize = std::max(1, maximumBlockSize);
    scratchLeft.resize(static_cast<std::size_t>(preparedBlockSize));
    scratchRight.resize(static_cast<std::size_t>(preparedBlockSize));
    const auto chorusSamples = static_cast<std::size_t>(std::ceil(0.05 * currentSampleRate)) + 2;
    const auto delaySamples = static_cast<std::size_t>(std::ceil(12.0 * currentSampleRate)) + 2;
    chorusLeft.resize(chorusSamples);
    chorusRight.resize(chorusSamples);
    delayLeft.resize(delaySamples);
    delayRight.resize(delaySamples);
    reverbProcessor.setSampleRate(currentSampleRate);
    reset();
}

void EffectChain::reset() noexcept
{
    std::fill(chorusLeft.begin(), chorusLeft.end(), 0.0f);
    std::fill(chorusRight.begin(), chorusRight.end(), 0.0f);
    std::fill(delayLeft.begin(), delayLeft.end(), 0.0f);
    std::fill(delayRight.begin(), delayRight.end(), 0.0f);
    chorusWrite = delayWrite = 0;
    chorusPhase = compressorEnvelope = 0.0f;
    bypassAmounts.fill(0.0f);
    lowShelf.reset();
    midPeak.reset();
    highShelf.reset();
    reverbProcessor.reset();
}

void EffectChain::copyDry(const juce::AudioBuffer<float>& audio) noexcept
{
    const auto count = audio.getNumSamples();
    std::copy_n(audio.getReadPointer(0), count, scratchLeft.data());
    std::copy_n(audio.getReadPointer(std::min(1, audio.getNumChannels() - 1)), count, scratchRight.data());
}

void EffectChain::blendBypass(juce::AudioBuffer<float>& audio, int stage, bool bypass) noexcept
{
    auto& amount = bypassAmounts[static_cast<std::size_t>(stage)];
    const auto target = bypass ? 0.0f : 1.0f;
    const auto step = 1.0f / static_cast<float>(std::max(1.0, 0.01 * currentSampleRate));
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        amount += juce::jlimit(-step, step, target - amount);
        audio.setSample(0, sample, scratchLeft[static_cast<std::size_t>(sample)]
            + amount * (audio.getSample(0, sample) - scratchLeft[static_cast<std::size_t>(sample)]));
        audio.setSample(1, sample, scratchRight[static_cast<std::size_t>(sample)]
            + amount * (audio.getSample(1, sample) - scratchRight[static_cast<std::size_t>(sample)]));
    }
}

void EffectChain::process(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    if (audio.getNumChannels() < 2 || audio.getNumSamples() > preparedBlockSize || preparedBlockSize <= 0)
        return;
    distortion(audio, p); chorus(audio, p); delay(audio, p); reverb(audio, p); compressor(audio, p); equalizer(audio, p);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            audio.setSample(channel, sample, bounded(audio.getSample(channel, sample)));
}

void EffectChain::distortion(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    const auto drive = gainFromDb(clamped(0.0f, 36.0f, p.distortionDriveDb, 6.0f));
    const auto trim = gainFromDb(clamped(-24.0f, 0.0f, p.distortionOutputDb, -6.0f));
    const auto mix = clamped(0.0f, 1.0f, p.distortionMix, 0.5f);
    const auto norm = std::max(0.001f, std::tanh(drive));
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            const auto dry = audio.getSample(channel, sample);
            const auto wet = trim * std::tanh(dry * drive) / norm;
            audio.setSample(channel, sample, dry + mix * (wet - dry));
        }
    blendBypass(audio, 0, p.distortionBypass);
}

void EffectChain::chorus(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    const auto rate = clamped(0.05f, 5.0f, p.chorusRateHz, 0.35f);
    const auto depth = clamped(0.0f, 20.0f, p.chorusDepthMs, 6.0f) * 0.001f
        * static_cast<float>(currentSampleRate);
    const auto mix = clamped(0.0f, 1.0f, p.chorusMix, 0.25f);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto base = 0.012f * static_cast<float>(currentSampleRate);
        const auto wetL = readDelay(chorusLeft, chorusWrite, base + depth * (0.5f + 0.5f * std::sin(twoPi * chorusPhase)));
        const auto wetR = readDelay(chorusRight, chorusWrite, base + depth * (0.5f + 0.5f * std::cos(twoPi * chorusPhase)));
        chorusLeft[chorusWrite] = audio.getSample(0, sample);
        chorusRight[chorusWrite] = audio.getSample(1, sample);
        audio.setSample(0, sample, audio.getSample(0, sample) + mix * (wetL - audio.getSample(0, sample)));
        audio.setSample(1, sample, audio.getSample(1, sample) + mix * (wetR - audio.getSample(1, sample)));
        chorusWrite = (chorusWrite + 1) % chorusLeft.size();
        chorusPhase += rate / static_cast<float>(currentSampleRate);
        chorusPhase -= std::floor(chorusPhase);
    }
    blendBypass(audio, 1, p.chorusBypass);
}

void EffectChain::delay(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    const auto index = juce::jlimit(0, static_cast<int>(delayBeats.size()) - 1, p.delayDivision);
    const auto seconds = delayBeats[static_cast<std::size_t>(index)] * 60.0f
        / static_cast<float>(clamped(20.0, 400.0, p.tempoBpm, 120.0));
    const auto samples = juce::jlimit(1.0f, static_cast<float>(delayLeft.size() - 2), seconds * static_cast<float>(currentSampleRate));
    const auto feedback = clamped(0.0f, 0.85f, p.delayFeedback, 0.3f);
    const auto mix = clamped(0.0f, 1.0f, p.delayMix, 0.25f);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto wetL = readDelay(delayLeft, delayWrite, samples);
        const auto wetR = readDelay(delayRight, delayWrite, samples);
        const auto dryL = audio.getSample(0, sample), dryR = audio.getSample(1, sample);
        delayLeft[delayWrite] = bounded(dryL + wetR * feedback);
        delayRight[delayWrite] = bounded(dryR + wetL * feedback);
        audio.setSample(0, sample, dryL + mix * (wetL - dryL));
        audio.setSample(1, sample, dryR + mix * (wetR - dryR));
        delayWrite = (delayWrite + 1) % delayLeft.size();
    }
    blendBypass(audio, 2, p.delayBypass);
}

void EffectChain::reverb(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    juce::Reverb::Parameters values;
    values.roomSize = clamped(0.0f, 1.0f, p.reverbRoomSize, 0.45f);
    values.damping = clamped(0.0f, 1.0f, p.reverbDamping, 0.4f);
    values.width = 1.0f; values.wetLevel = 1.0f; values.dryLevel = 0.0f; values.freezeMode = 0.0f;
    reverbProcessor.setParameters(values);
    reverbProcessor.processStereo(audio.getWritePointer(0), audio.getWritePointer(1), audio.getNumSamples());
    const auto mix = clamped(0.0f, 1.0f, p.reverbMix, 0.2f);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        audio.setSample(0, sample, scratchLeft[static_cast<std::size_t>(sample)] + mix * (audio.getSample(0, sample) - scratchLeft[static_cast<std::size_t>(sample)]));
        audio.setSample(1, sample, scratchRight[static_cast<std::size_t>(sample)] + mix * (audio.getSample(1, sample) - scratchRight[static_cast<std::size_t>(sample)]));
    }
    blendBypass(audio, 3, p.reverbBypass);
}

void EffectChain::compressor(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    const auto attackMs = clamped(0.1f, 100.0f, p.compressorAttackMs, 10.0f);
    const auto releaseMs = clamped(10.0f, 1000.0f, p.compressorReleaseMs, 100.0f);
    const auto attack = std::exp(-1.0f / std::max(1.0f, attackMs * 0.001f * static_cast<float>(currentSampleRate)));
    const auto release = std::exp(-1.0f / std::max(1.0f, releaseMs * 0.001f * static_cast<float>(currentSampleRate)));
    const auto ratio = clamped(1.0f, 20.0f, p.compressorRatio, 4.0f);
    const auto threshold = clamped(-60.0f, 0.0f, p.compressorThresholdDb, -18.0f);
    const auto makeup = clamped(0.0f, 18.0f, p.compressorMakeupDb, 0.0f);
    const auto mix = clamped(0.0f, 1.0f, p.compressorMix, 1.0f);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        const auto level = std::max(std::abs(audio.getSample(0, sample)), std::abs(audio.getSample(1, sample)));
        compressorEnvelope = level > compressorEnvelope ? attack * compressorEnvelope + (1.0f - attack) * level
                                                         : release * compressorEnvelope + (1.0f - release) * level;
        const auto levelDb = juce::Decibels::gainToDecibels(std::max(1.0e-9f, compressorEnvelope));
        const auto over = std::max(0.0f, levelDb - threshold);
        const auto gain = gainFromDb(-over * (1.0f - 1.0f / ratio) + makeup);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto dry = audio.getSample(channel, sample);
            audio.setSample(channel, sample, dry + mix * (dry * gain - dry));
        }
    }
    blendBypass(audio, 4, p.compressorBypass);
}

void EffectChain::equalizer(juce::AudioBuffer<float>& audio, const Parameters& p) noexcept
{
    copyDry(audio);
    lowShelf.setShelf(120.0f, clamped(-18.0f, 18.0f, p.eqLowGainDb, 0.0f), false, currentSampleRate);
    midPeak.setPeak(clamped(20.0f, 20000.0f, p.eqMidFrequencyHz, 1000.0f),
                    clamped(0.1f, 10.0f, p.eqMidQ, 1.0f),
                    clamped(-18.0f, 18.0f, p.eqMidGainDb, 0.0f), currentSampleRate);
    highShelf.setShelf(8000.0f, clamped(-18.0f, 18.0f, p.eqHighGainDb, 0.0f), true, currentSampleRate);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            audio.setSample(channel, sample, highShelf.process(midPeak.process(lowShelf.process(audio.getSample(channel, sample), channel), channel), channel));
    blendBypass(audio, 5, p.eqBypass);
}

void EffectChain::Biquad::reset() noexcept { z1 = {}; z2 = {}; }

void EffectChain::Biquad::setPeak(float frequency, float q, float gainDb, double rate) noexcept
{
    const auto a = std::pow(10.0f, clamped(-18.0f, 18.0f, gainDb, 0.0f) / 40.0f);
    const auto w = twoPi * clamped(20.0f, static_cast<float>(0.45 * rate), frequency, 1000.0f)
        / static_cast<float>(rate);
    const auto alpha = std::sin(w) / (2.0f * clamped(0.1f, 10.0f, q, 1.0f));
    const auto a0 = 1.0f + alpha / a;
    b0 = (1.0f + alpha * a) / a0; b1 = (-2.0f * std::cos(w)) / a0; b2 = (1.0f - alpha * a) / a0;
    a1 = (-2.0f * std::cos(w)) / a0; a2 = (1.0f - alpha / a) / a0;
}

void EffectChain::Biquad::setShelf(float frequency, float gainDb, bool high, double rate) noexcept
{
    const auto a = std::pow(10.0f, clamped(-18.0f, 18.0f, gainDb, 0.0f) / 40.0f);
    const auto w = twoPi * frequency / static_cast<float>(rate), c = std::cos(w), s = std::sin(w);
    const auto alpha = s / std::sqrt(2.0f), beta = 2.0f * std::sqrt(a) * alpha;
    const auto sign = high ? -1.0f : 1.0f;
    const auto a0 = (a + 1.0f) + sign * (a - 1.0f) * c + beta;
    b0 = a * ((a + 1.0f) - sign * (a - 1.0f) * c + beta) / a0;
    b1 = 2.0f * a * (sign * (a - 1.0f) - (a + 1.0f) * c) / a0;
    b2 = a * ((a + 1.0f) - sign * (a - 1.0f) * c - beta) / a0;
    a1 = -2.0f * (sign * (a - 1.0f) + (a + 1.0f) * c) / a0;
    a2 = ((a + 1.0f) + sign * (a - 1.0f) * c - beta) / a0;
}

float EffectChain::Biquad::process(float input, int channel) noexcept
{
    const auto index = static_cast<std::size_t>(channel);
    const auto output = b0 * input + z1[index];
    z1[index] = b1 * input - a1 * output + z2[index];
    z2[index] = b2 * input - a2 * output;
    return bounded(output);
}
}
