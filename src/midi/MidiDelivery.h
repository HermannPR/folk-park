#pragma once

#include "Composition.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace folkpark::midi
{
struct MidiExportResult
{
    juce::Result status = juce::Result::fail("MIDI export did not run");
    juce::MemoryBlock data;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk() && !data.isEmpty(); }
};

[[nodiscard]] MidiExportResult createMidiFileData(const CompositionBundle& bundle,
                                                  int targetPpq = compositionPpq);
[[nodiscard]] juce::Result validateMidiFileData(const juce::MemoryBlock& data,
                                                const CompositionBundle& source,
                                                int expectedPpq = compositionPpq);
[[nodiscard]] juce::Result writeMidiFile(const CompositionBundle& bundle,
                                         const juce::File& destination,
                                         int targetPpq = compositionPpq);
[[nodiscard]] juce::File writeMidiToTemporaryFile(const CompositionBundle& bundle,
                                                  int targetPpq = compositionPpq);

class DirectMidiPlayer final
{
public:
    static constexpr std::size_t maximumScheduledEvents = maximumGeneratedEvents * 2;
    static constexpr int maximumEventsPerBlock = 128;

    struct RenderResult
    {
        int emittedEvents = 0;
        bool completed = false;
        bool overflow = false;
    };

    [[nodiscard]] juce::Result publish(const CompositionBundle& bundle);
    void requestStop() noexcept { stopRequested.store(true, std::memory_order_release); }
    void reset() noexcept;
    [[nodiscard]] RenderResult renderBlock(juce::MidiBuffer& output,
                                           int numberOfSamples,
                                           double sampleRate) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept
    {
        return playing.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool hasPendingSchedule() const noexcept
    {
        return pendingIndex.load(std::memory_order_acquire) >= 0;
    }

private:
    struct ScheduledEvent
    {
        double timeSeconds = 0.0;
        std::array<std::uint8_t, 3> bytes{};
        bool noteOn = false;
    };

    struct Schedule
    {
        std::array<ScheduledEvent, maximumScheduledEvents> events{};
        std::size_t eventCount = 0;
        double durationSeconds = 0.0;
    };

    int emitAllTrackedNoteOffs(juce::MidiBuffer& output, int sampleOffset) noexcept;

    std::array<Schedule, 2> schedules{};
    std::atomic<int> activeIndex{0};
    std::atomic<int> pendingIndex{-1};
    std::atomic_flag writer = ATOMIC_FLAG_INIT;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> playing{false};
    std::array<std::array<bool, 128>, 16> activeNotes{};
    std::size_t nextEventIndex = 0;
    double cursorSeconds = 0.0;
};
}
