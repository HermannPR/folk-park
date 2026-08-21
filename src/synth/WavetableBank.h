#pragma once

#include <array>
#include <cstddef>
#include <memory>

namespace folkpark::synth
{
class WavetableConverter;

class WavetableBank final
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int maximumFrames = 16;
    static constexpr int mipLevelCount = 11;

    [[nodiscard]] static std::unique_ptr<WavetableBank> createBuiltIn();

    [[nodiscard]] int getFrameCount() const noexcept { return frameCount; }
    [[nodiscard]] int mipLevelForFrequency(float frequency, double sampleRate) const noexcept;
    [[nodiscard]] float read(float framePosition, float phase, int mipLevel) const noexcept;
    [[nodiscard]] float getSample(int frame, int mipLevel, int sample) const noexcept;
    [[nodiscard]] bool isFiniteAndNormalised() const noexcept;

private:
    friend class WavetableConverter;

    static constexpr std::size_t sampleCount = static_cast<std::size_t>(maximumFrames)
        * static_cast<std::size_t>(mipLevelCount)
        * static_cast<std::size_t>(tableSize);

    [[nodiscard]] std::size_t indexFor(int frame, int mipLevel, int sample) const noexcept;
    void clear() noexcept;
    void setFrameCount(int newFrameCount) noexcept;
    void setBaseSample(int frame, int sample, float value) noexcept;
    void buildMipLevels();

    std::array<float, sampleCount> samples{};
    int frameCount = 1;
};
}
