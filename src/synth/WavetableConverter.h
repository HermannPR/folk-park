#pragma once

#include "WavetableBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <memory>

namespace folkpark::synth
{
class WavetableConverter final
{
public:
    static constexpr std::int64_t maximumSourceSamples = 2'000'000;
    static constexpr int minimumCycleLength = 32;
    static constexpr int maximumCycleLength = 65'536;
    static constexpr int previewSize = 256;

    struct Metadata
    {
        juce::String sourceFileName;
        juce::String sourceSha256;
        double sourceSampleRate = 0.0;
        std::int64_t sourceSampleCount = 0;
        int sourceChannels = 0;
        int acceptedCycleLength = 0;
        int outputFrameCount = 0;
    };

    struct Result
    {
        juce::Result status = juce::Result::fail("Conversion did not run");
        std::unique_ptr<WavetableBank> bank;
        Metadata metadata;
        std::array<float, previewSize> preview{};

        [[nodiscard]] bool succeeded() const noexcept { return status.wasOk() && bank != nullptr; }
    };

    [[nodiscard]] static juce::Result validateDecodedAudio(const juce::AudioBuffer<float>& decoded) noexcept;
    [[nodiscard]] Result convertWavFile(const juce::File& file, int requestedCycleLength = 0) const;
};
}
