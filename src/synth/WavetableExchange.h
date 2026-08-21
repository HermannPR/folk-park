#pragma once

#include "WavetableBank.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace folkpark::synth
{
class WavetableExchange final
{
public:
    static constexpr int crossfadeLengthSamples = 128;

    struct RenderView
    {
        const WavetableBank* current = nullptr;
        const WavetableBank* previous = nullptr;
        float currentGain = 1.0f;
        float previousGain = 0.0f;

        [[nodiscard]] float read(float framePosition, float phase, int mipLevel) const noexcept;
    };

    explicit WavetableExchange(const WavetableBank& initialBank);

    [[nodiscard]] bool publish(const WavetableBank& bank) noexcept;
    void beginAudioBlock() noexcept;
    [[nodiscard]] RenderView renderView() const noexcept;
    void advanceSample() noexcept;
    [[nodiscard]] bool hasPendingBank() const noexcept;
    [[nodiscard]] int getCurrentFrameCount() const noexcept;

private:
    enum class SlotState : std::uint8_t
    {
        free,
        writing,
        pending,
        current,
        previous
    };

    struct Storage
    {
        std::array<WavetableBank, 3> banks{};
        std::array<std::atomic<SlotState>, 3> states{};
    };

    std::unique_ptr<Storage> storage;
    std::atomic<int> pendingIndex{-1};
    int currentIndex = 0;
    int previousIndex = -1;
    int crossfadeRemaining = 0;
};
}
