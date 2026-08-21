#include "WavetableConverter.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace folkpark::synth
{
namespace
{
float interpolate(const std::vector<float>& source, double position) noexcept
{
    const auto first = juce::jlimit(0, static_cast<int>(source.size()) - 1, static_cast<int>(position));
    const auto second = juce::jmin(static_cast<int>(source.size()) - 1, first + 1);
    const auto fraction = static_cast<float>(position - static_cast<double>(first));
    return source[static_cast<std::size_t>(first)]
        + fraction * (source[static_cast<std::size_t>(second)] - source[static_cast<std::size_t>(first)]);
}
}

juce::Result WavetableConverter::validateDecodedAudio(const juce::AudioBuffer<float>& decoded) noexcept
{
    if (decoded.getNumChannels() <= 0 || decoded.getNumChannels() > 8)
        return juce::Result::fail("WAV channel count is unsupported");
    if (decoded.getNumSamples() < minimumCycleLength
        || static_cast<std::int64_t>(decoded.getNumSamples()) > maximumSourceSamples)
        return juce::Result::fail("WAV sample count is outside the supported bounds");

    for (int channel = 0; channel < decoded.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < decoded.getNumSamples(); ++sample)
        {
            if (!std::isfinite(decoded.getSample(channel, sample)))
                return juce::Result::fail("WAV contains a non-finite sample");
        }
    }
    return juce::Result::ok();
}

WavetableConverter::Result WavetableConverter::convertWavFile(const juce::File& file,
                                                              int requestedCycleLength) const
{
    Result result;
    if (!file.existsAsFile())
    {
        result.status = juce::Result::fail("WAV file does not exist");
        return result;
    }
    if (!file.hasFileExtension("wav;wave"))
    {
        result.status = juce::Result::fail("Only WAV files are supported");
        return result;
    }

    juce::WavAudioFormat wavFormat;
    auto stream = file.createInputStream();
    if (stream == nullptr)
    {
        result.status = juce::Result::fail("WAV file could not be opened");
        return result;
    }
    auto reader = std::unique_ptr<juce::AudioFormatReader>(wavFormat.createReaderFor(stream.release(), true));
    if (reader == nullptr)
    {
        result.status = juce::Result::fail("WAV header or encoding is unsupported");
        return result;
    }
    if (reader->numChannels == 0 || reader->numChannels > 8)
    {
        result.status = juce::Result::fail("WAV channel count is unsupported");
        return result;
    }
    if (reader->lengthInSamples < minimumCycleLength || reader->lengthInSamples > maximumSourceSamples)
    {
        result.status = juce::Result::fail("WAV sample count is outside the supported bounds");
        return result;
    }

    const auto cycleLength = requestedCycleLength == 0
        ? juce::jmin(WavetableBank::tableSize, static_cast<int>(reader->lengthInSamples))
        : requestedCycleLength;
    if (cycleLength < minimumCycleLength || cycleLength > maximumCycleLength
        || cycleLength > reader->lengthInSamples)
    {
        result.status = juce::Result::fail("Requested cycle length is invalid for this WAV");
        return result;
    }

    juce::AudioBuffer<float> decoded(static_cast<int>(reader->numChannels),
                                     static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true))
    {
        result.status = juce::Result::fail("WAV samples could not be decoded");
        return result;
    }

    if (const auto validation = validateDecodedAudio(decoded); validation.failed())
    {
        result.status = validation;
        return result;
    }

    std::vector<float> mono(static_cast<std::size_t>(decoded.getNumSamples()), 0.0f);
    for (int sample = 0; sample < decoded.getNumSamples(); ++sample)
    {
        auto value = 0.0f;
        for (int channel = 0; channel < decoded.getNumChannels(); ++channel)
        {
            const auto channelValue = decoded.getSample(channel, sample);
            value += channelValue;
        }
        mono[static_cast<std::size_t>(sample)] = value / static_cast<float>(decoded.getNumChannels());
    }

    const auto availableCycles = static_cast<int>(reader->lengthInSamples / cycleLength);
    const auto frameCount = juce::jlimit(1, WavetableBank::maximumFrames, availableCycles);
    auto bank = std::make_unique<WavetableBank>();
    bank->clear();
    bank->setFrameCount(frameCount);
    auto globalPeak = 0.0f;

    for (int frame = 0; frame < frameCount; ++frame)
    {
        const auto sourceCycle = frameCount == 1
            ? 0
            : juce::roundToInt(static_cast<double>(frame) * static_cast<double>(availableCycles - 1)
                               / static_cast<double>(frameCount - 1));
        const auto sourceStart = sourceCycle * cycleLength;
        std::array<float, WavetableBank::tableSize> converted{};
        auto mean = 0.0f;

        for (int sample = 0; sample < WavetableBank::tableSize; ++sample)
        {
            const auto sourcePosition = static_cast<double>(sourceStart)
                + static_cast<double>(sample) * static_cast<double>(cycleLength)
                    / static_cast<double>(WavetableBank::tableSize);
            converted[static_cast<std::size_t>(sample)] = interpolate(mono, sourcePosition);
            mean += converted[static_cast<std::size_t>(sample)];
        }
        mean /= static_cast<float>(WavetableBank::tableSize);

        for (auto& value : converted)
            value -= mean;

        const auto endpointDifference = converted.back() - converted.front();
        for (int sample = 0; sample < WavetableBank::tableSize; ++sample)
        {
            converted[static_cast<std::size_t>(sample)] -= endpointDifference
                * static_cast<float>(sample) / static_cast<float>(WavetableBank::tableSize - 1);
            globalPeak = std::max(globalPeak, std::abs(converted[static_cast<std::size_t>(sample)]));
            bank->setBaseSample(frame, sample, converted[static_cast<std::size_t>(sample)]);
        }
    }

    if (!std::isfinite(globalPeak) || globalPeak <= 1.0e-8f)
    {
        result.status = juce::Result::fail("WAV cycle has no usable signal");
        return result;
    }

    const auto gain = 0.98f / globalPeak;
    for (int frame = 0; frame < frameCount; ++frame)
    {
        for (int sample = 0; sample < WavetableBank::tableSize; ++sample)
            bank->setBaseSample(frame, sample, bank->getSample(frame, 0, sample) * gain);
    }
    bank->buildMipLevels();
    if (!bank->isFiniteAndNormalised())
    {
        result.status = juce::Result::fail("Converted wavetable failed finite/normalization validation");
        return result;
    }

    for (int sample = 0; sample < previewSize; ++sample)
    {
        const auto sourceIndex = sample * WavetableBank::tableSize / previewSize;
        result.preview[static_cast<std::size_t>(sample)] = bank->getSample(0, 0, sourceIndex);
    }

    result.metadata.sourceFileName = file.getFileName();
    result.metadata.sourceSha256 = juce::SHA256(file).toHexString();
    result.metadata.sourceSampleRate = reader->sampleRate;
    result.metadata.sourceSampleCount = reader->lengthInSamples;
    result.metadata.sourceChannels = static_cast<int>(reader->numChannels);
    result.metadata.acceptedCycleLength = cycleLength;
    result.metadata.outputFrameCount = frameCount;
    result.bank = std::move(bank);
    result.status = juce::Result::ok();
    return result;
}
}
