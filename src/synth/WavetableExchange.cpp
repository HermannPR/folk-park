#include "WavetableExchange.h"

#include <algorithm>

namespace folkpark::synth
{
float WavetableExchange::RenderView::read(float framePosition, float phase, int mipLevel) const noexcept
{
    if (current == nullptr)
        return 0.0f;
    const auto currentValue = current->read(framePosition, phase, mipLevel) * currentGain;
    if (previous == nullptr || previousGain <= 0.0f)
        return currentValue;
    return currentValue + previous->read(framePosition, phase, mipLevel) * previousGain;
}

WavetableExchange::WavetableExchange(const WavetableBank& initialBank)
    : storage(std::make_unique<Storage>())
{
    storage->banks[0] = initialBank;
    storage->states[0].store(SlotState::current, std::memory_order_relaxed);
    storage->states[1].store(SlotState::free, std::memory_order_relaxed);
    storage->states[2].store(SlotState::free, std::memory_order_relaxed);
}

bool WavetableExchange::publish(const WavetableBank& bank) noexcept
{
    if (!bank.isFiniteAndNormalised() || pendingIndex.load(std::memory_order_acquire) >= 0)
        return false;

    for (int index = 0; index < static_cast<int>(storage->banks.size()); ++index)
    {
        auto expected = SlotState::free;
        if (!storage->states[static_cast<std::size_t>(index)].compare_exchange_strong(
                expected, SlotState::writing, std::memory_order_acq_rel))
            continue;

        storage->banks[static_cast<std::size_t>(index)] = bank;
        storage->states[static_cast<std::size_t>(index)].store(SlotState::pending, std::memory_order_release);
        auto noPending = -1;
        if (pendingIndex.compare_exchange_strong(noPending, index, std::memory_order_release))
            return true;

        storage->states[static_cast<std::size_t>(index)].store(SlotState::free, std::memory_order_release);
        return false;
    }
    return false;
}

void WavetableExchange::beginAudioBlock() noexcept
{
    if (crossfadeRemaining > 0)
        return;

    const auto nextIndex = pendingIndex.exchange(-1, std::memory_order_acquire);
    if (nextIndex < 0)
        return;

    previousIndex = currentIndex;
    currentIndex = nextIndex;
    storage->states[static_cast<std::size_t>(previousIndex)].store(SlotState::previous,
                                                                  std::memory_order_release);
    storage->states[static_cast<std::size_t>(currentIndex)].store(SlotState::current,
                                                                 std::memory_order_release);
    crossfadeRemaining = crossfadeLengthSamples;
}

WavetableExchange::RenderView WavetableExchange::renderView() const noexcept
{
    RenderView view;
    view.current = &storage->banks[static_cast<std::size_t>(currentIndex)];
    if (previousIndex >= 0 && crossfadeRemaining > 0)
    {
        const auto progressed = crossfadeLengthSamples - crossfadeRemaining + 1;
        view.currentGain = static_cast<float>(progressed) / static_cast<float>(crossfadeLengthSamples);
        view.previousGain = 1.0f - view.currentGain;
        view.previous = &storage->banks[static_cast<std::size_t>(previousIndex)];
    }
    return view;
}

void WavetableExchange::advanceSample() noexcept
{
    if (crossfadeRemaining <= 0)
        return;

    --crossfadeRemaining;
    if (crossfadeRemaining == 0 && previousIndex >= 0)
    {
        storage->states[static_cast<std::size_t>(previousIndex)].store(SlotState::free,
                                                                      std::memory_order_release);
        previousIndex = -1;
    }
}

bool WavetableExchange::hasPendingBank() const noexcept
{
    return pendingIndex.load(std::memory_order_acquire) >= 0;
}

int WavetableExchange::getCurrentFrameCount() const noexcept
{
    return storage->banks[static_cast<std::size_t>(currentIndex)].getFrameCount();
}
}
