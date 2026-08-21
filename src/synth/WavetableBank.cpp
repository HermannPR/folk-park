#include "WavetableBank.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace folkpark::synth
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

float triangleAt(float phase) noexcept
{
    return 1.0f - 4.0f * std::abs(phase - 0.5f);
}

float sawAt(float phase) noexcept
{
    return 2.0f * phase - 1.0f;
}

float squareAt(float phase) noexcept
{
    return phase < 0.5f ? 1.0f : -1.0f;
}
}

std::unique_ptr<WavetableBank> WavetableBank::createBuiltIn()
{
    auto bank = std::make_unique<WavetableBank>();
    bank->clear();
    bank->setFrameCount(4);

    for (int sample = 0; sample < tableSize; ++sample)
    {
        const auto phase = static_cast<float>(sample) / static_cast<float>(tableSize);
        bank->setBaseSample(0, sample, std::sin(twoPi * phase));
        bank->setBaseSample(1, sample, triangleAt(phase));
        bank->setBaseSample(2, sample, sawAt(phase));
        bank->setBaseSample(3, sample, squareAt(phase));
    }

    bank->buildMipLevels();
    return bank;
}

std::size_t WavetableBank::indexFor(int frame, int mipLevel, int sample) const noexcept
{
    return (static_cast<std::size_t>(frame) * static_cast<std::size_t>(mipLevelCount)
            + static_cast<std::size_t>(mipLevel))
        * static_cast<std::size_t>(tableSize)
        + static_cast<std::size_t>(sample);
}

void WavetableBank::clear() noexcept
{
    samples.fill(0.0f);
    frameCount = 1;
}

void WavetableBank::setFrameCount(int newFrameCount) noexcept
{
    frameCount = juce::jlimit(1, maximumFrames, newFrameCount);
}

void WavetableBank::setBaseSample(int frame, int sample, float value) noexcept
{
    if (frame >= 0 && frame < frameCount && sample >= 0 && sample < tableSize)
        samples[indexFor(frame, 0, sample)] = value;
}

float WavetableBank::getSample(int frame, int mipLevel, int sample) const noexcept
{
    const auto safeFrame = juce::jlimit(0, frameCount - 1, frame);
    const auto safeMip = juce::jlimit(0, mipLevelCount - 1, mipLevel);
    const auto safeSample = sample & (tableSize - 1);
    return samples[indexFor(safeFrame, safeMip, safeSample)];
}

int WavetableBank::mipLevelForFrequency(float frequency, double sampleRate) const noexcept
{
    const auto safeFrequency = std::max(1.0f, frequency);
    const auto safeRate = static_cast<float>(std::max(1.0, sampleRate));
    const auto safeHarmonics = std::max(1.0f, (0.5f * safeRate) / safeFrequency);
    const auto fullBandHarmonics = static_cast<float>(tableSize / 2);
    const auto octaveReduction = std::ceil(std::log2(fullBandHarmonics / safeHarmonics));
    return juce::jlimit(0, mipLevelCount - 1, static_cast<int>(std::max(0.0f, octaveReduction)));
}

float WavetableBank::read(float framePosition, float phase, int mipLevel) const noexcept
{
    const auto wrappedPhase = phase - std::floor(phase);
    const auto tablePosition = wrappedPhase * static_cast<float>(tableSize);
    const auto firstSample = static_cast<int>(tablePosition) & (tableSize - 1);
    const auto secondSample = (firstSample + 1) & (tableSize - 1);
    const auto sampleFraction = tablePosition - static_cast<float>(firstSample);

    const auto safePosition = juce::jlimit(0.0f, 1.0f, framePosition);
    const auto framePositionScaled = safePosition * static_cast<float>(frameCount - 1);
    const auto firstFrame = static_cast<int>(framePositionScaled);
    const auto secondFrame = juce::jmin(frameCount - 1, firstFrame + 1);
    const auto frameFraction = framePositionScaled - static_cast<float>(firstFrame);

    const auto interpolatePhase = [this, mipLevel, firstSample, secondSample, sampleFraction](int frame)
    {
        const auto first = getSample(frame, mipLevel, firstSample);
        const auto second = getSample(frame, mipLevel, secondSample);
        return first + sampleFraction * (second - first);
    };

    const auto first = interpolatePhase(firstFrame);
    const auto second = interpolatePhase(secondFrame);
    return first + frameFraction * (second - first);
}

void WavetableBank::buildMipLevels()
{
    juce::dsp::FFT fft(11);
    std::vector<juce::dsp::Complex<float>> timeDomain(static_cast<std::size_t>(tableSize));
    std::vector<juce::dsp::Complex<float>> spectrum(static_cast<std::size_t>(tableSize));
    std::vector<juce::dsp::Complex<float>> filtered(static_cast<std::size_t>(tableSize));
    std::vector<juce::dsp::Complex<float>> reconstructed(static_cast<std::size_t>(tableSize));

    for (int frame = 0; frame < frameCount; ++frame)
    {
        for (int sample = 0; sample < tableSize; ++sample)
            timeDomain[static_cast<std::size_t>(sample)] = {getSample(frame, 0, sample), 0.0f};

        fft.perform(timeDomain.data(), spectrum.data(), false);

        for (int mip = 1; mip < mipLevelCount; ++mip)
        {
            filtered = spectrum;
            const auto maximumHarmonic = std::max(1, (tableSize / 2) >> mip);
            for (int bin = maximumHarmonic + 1; bin < tableSize - maximumHarmonic; ++bin)
                filtered[static_cast<std::size_t>(bin)] = {};

            fft.perform(filtered.data(), reconstructed.data(), true);
            auto peak = 0.0f;
            for (const auto& value : reconstructed)
                peak = std::max(peak, std::abs(value.real()));
            const auto normalisingGain = peak > 1.0f ? 1.0f / peak : 1.0f;
            for (int sample = 0; sample < tableSize; ++sample)
                samples[indexFor(frame, mip, sample)] = reconstructed[static_cast<std::size_t>(sample)].real()
                    * normalisingGain;
        }
    }
}

bool WavetableBank::isFiniteAndNormalised() const noexcept
{
    auto peak = 0.0f;
    for (int frame = 0; frame < frameCount; ++frame)
    {
        for (int mip = 0; mip < mipLevelCount; ++mip)
        {
            for (int sample = 0; sample < tableSize; ++sample)
            {
                const auto value = samples[indexFor(frame, mip, sample)];
                if (!std::isfinite(value))
                    return false;
                peak = std::max(peak, std::abs(value));
            }
        }
    }
    return peak <= 1.0001f;
}
}
