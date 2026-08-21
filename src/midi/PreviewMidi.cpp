#include "PreviewMidi.h"

namespace folkpark::midi
{
bool PreviewMidiQueue::enqueue(Command command) noexcept
{
    const auto write = writeIndex.load(std::memory_order_relaxed);
    const auto next = (write + 1) % storageSize;
    if (next == readIndex.load(std::memory_order_acquire))
        return false;
    commands[write] = command;
    writeIndex.store(next, std::memory_order_release);
    return true;
}

bool PreviewMidiQueue::enqueueNoteOn(int note, int velocity) noexcept
{
    if (note < 0 || note > 127 || velocity < 1 || velocity > 127)
        return false;
    return enqueue({Kind::noteOn, static_cast<std::uint8_t>(note),
                    static_cast<std::uint8_t>(velocity)});
}

bool PreviewMidiQueue::enqueueNoteOff(int note) noexcept
{
    if (note < 0 || note > 127)
        return false;
    if (enqueue({Kind::noteOff, static_cast<std::uint8_t>(note), 0}))
        return true;
    requestReleaseAll();
    return false;
}

int PreviewMidiQueue::emitReleaseAll(juce::MidiBuffer& output, int sampleOffset) noexcept
{
    auto emitted = 0;
    for (std::size_t note = 0; note < activeNotes.size(); ++note)
    {
        if (!activeNotes[note])
            continue;
        const std::array<std::uint8_t, 3> bytes{0x80, static_cast<std::uint8_t>(note), 0};
        output.addEvent(bytes.data(), static_cast<int>(bytes.size()), sampleOffset);
        activeNotes[note] = false;
        ++emitted;
    }
    return emitted;
}

int PreviewMidiQueue::renderBlock(juce::MidiBuffer& output, int sampleOffset) noexcept
{
    auto emitted = 0;
    if (releaseAllRequested.exchange(false, std::memory_order_acq_rel))
    {
        emitted += emitReleaseAll(output, sampleOffset);
        readIndex.store(writeIndex.load(std::memory_order_acquire), std::memory_order_release);
        return emitted;
    }

    auto read = readIndex.load(std::memory_order_relaxed);
    const auto write = writeIndex.load(std::memory_order_acquire);
    while (read != write)
    {
        const auto command = commands[read];
        const auto note = static_cast<std::size_t>(command.note);
        const auto duplicateOn = command.kind == Kind::noteOn && activeNotes[note];
        const auto duplicateOff = command.kind == Kind::noteOff && !activeNotes[note];
        read = (read + 1) % storageSize;
        if (duplicateOn || duplicateOff)
        {
            // Key repeat and redundant releases are idempotent at the native
            // boundary. A held computer key must not repeatedly retrigger.
            continue;
        }
        const std::array<std::uint8_t, 3> bytes{
            static_cast<std::uint8_t>(command.kind == Kind::noteOn ? 0x90 : 0x80),
            command.note, command.velocity};
        output.addEvent(bytes.data(), static_cast<int>(bytes.size()), sampleOffset);
        activeNotes[note] = command.kind == Kind::noteOn;
        ++emitted;
    }
    readIndex.store(read, std::memory_order_release);
    return emitted;
}

void PreviewMidiQueue::reset() noexcept
{
    readIndex.store(writeIndex.load(std::memory_order_acquire), std::memory_order_release);
    releaseAllRequested.store(false, std::memory_order_release);
    activeNotes = {};
}
}
