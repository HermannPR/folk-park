#pragma once

#include "WavetableConverter.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace folkpark::synth
{
class WavetableImportService final
{
public:
    enum class Status : std::uint8_t
    {
        idle,
        processing,
        awaitingConfirmation,
        loaded,
        failed,
        cancelled
    };

    struct Snapshot
    {
        Status status = Status::idle;
        int oscillatorIndex = 0;
        juce::String message{"No wavetable import requested"};
        WavetableConverter::Metadata metadata;
        std::array<float, WavetableConverter::previewSize> preview{};
    };

    using Publisher = std::function<bool(int, const WavetableBank&)>;

    explicit WavetableImportService(Publisher publisher);
    ~WavetableImportService();

    [[nodiscard]] juce::Result request(const juce::File& file,
                                       int oscillatorIndex,
                                       int requestedCycleLength = 0);
    [[nodiscard]] juce::Result confirm();
    void cancel();
    [[nodiscard]] Snapshot getSnapshot() const;

private:
    Publisher publisher;
    WavetableConverter converter;
    juce::ThreadPool worker{juce::ThreadPool::Options{}.withNumberOfThreads(1)
                                                        .withThreadName("folk park wavetable converter")};
    mutable std::mutex stateMutex;
    Snapshot state;
    std::unique_ptr<WavetableBank> pendingBank;
    std::atomic<std::uint64_t> generation{0};
    std::atomic<bool> shuttingDown{false};
};
}
