#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace folkpark::midi
{
class PreviewMidiQueue final
{
public:
    static constexpr std::size_t maximumQueuedCommands = 64;

    [[nodiscard]] bool enqueueNoteOn(int note, int velocity) noexcept;
    [[nodiscard]] bool enqueueNoteOff(int note) noexcept;
    void requestReleaseAll() noexcept { releaseAllRequested.store(true, std::memory_order_release); }
    [[nodiscard]] int renderBlock(juce::MidiBuffer& output, int sampleOffset = 0) noexcept;
    void reset() noexcept;

private:
    enum class Kind : std::uint8_t { noteOn, noteOff };
    struct Command { Kind kind = Kind::noteOff; std::uint8_t note = 0; std::uint8_t velocity = 0; };
    static constexpr std::size_t storageSize = maximumQueuedCommands + 1;

    [[nodiscard]] bool enqueue(Command command) noexcept;
    int emitReleaseAll(juce::MidiBuffer& output, int sampleOffset) noexcept;

    std::array<Command, storageSize> commands{};
    std::atomic<std::size_t> writeIndex{0};
    std::atomic<std::size_t> readIndex{0};
    std::atomic<bool> releaseAllRequested{false};
    std::array<bool, 128> activeNotes{};
};
}
