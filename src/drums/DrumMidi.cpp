#include "DrumMidi.h"

#include <cmath>

namespace folkpark::drums
{
namespace
{
std::int64_t rescale(std::int64_t tick, int sourcePpq, int targetPpq) noexcept
{
    return static_cast<std::int64_t>(std::llround(static_cast<double>(tick)
        * static_cast<double>(targetPpq) / static_cast<double>(sourcePpq)));
}

void addAt(juce::MidiMessageSequence& sequence, juce::MidiMessage message,
           std::int64_t tick)
{
    message.setTimeStamp(static_cast<double>(tick));
    sequence.addEvent(std::move(message));
}
}

int generalMidiPitch(DrumLane lane) noexcept
{
    switch (lane)
    {
        case DrumLane::kick: return 36;
        case DrumLane::snare: return 38;
        case DrumLane::closedHat: return 42;
        case DrumLane::openHat: return 46;
        case DrumLane::percussion: return 39;
        case DrumLane::count: break;
    }
    return -1;
}

DrumMidiExportResult createDrumMidiFileData(const DrumPattern& pattern, int targetPpq)
{
    DrumMidiExportResult result;
    if (const auto validation = validateDrumPattern(pattern); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (targetPpq < 24 || targetPpq > 9600)
    {
        result.status = juce::Result::fail("Drum MIDI PPQ is outside 24-9600");
        return result;
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote(targetPpq);
    juce::MidiMessageSequence metadata;
    addAt(metadata, juce::MidiMessage::tempoMetaEvent(
        juce::roundToInt(60000000.0 / pattern.tempoBpm)), 0);
    addAt(metadata, juce::MidiMessage::timeSignatureMetaEvent(
        pattern.timeSignature.numerator, pattern.timeSignature.denominator), 0);
    addAt(metadata, juce::MidiMessage::textMetaEvent(3, "folk park drums"), 0);
    metadata.sort();
    file.addTrack(metadata);

    juce::MidiMessageSequence drums;
    addAt(drums, juce::MidiMessage::textMetaEvent(3, "folk park synthesized drums"), 0);
    for (const auto& event : pattern.events)
    {
        const auto pitch = generalMidiPitch(event.lane);
        const auto start = rescale(event.startTick, pattern.ppq, targetPpq);
        const auto end = std::max(start + 1,
            rescale(event.startTick + event.durationTicks, pattern.ppq, targetPpq));
        addAt(drums, juce::MidiMessage::noteOn(10, pitch,
            static_cast<juce::uint8>(event.velocity)), start);
        addAt(drums, juce::MidiMessage::noteOff(10, pitch), end);
    }
    drums.sort();
    drums.updateMatchedPairs();
    file.addTrack(drums);

    juce::MemoryOutputStream stream(result.data, false);
    if (!file.writeTo(stream, 1))
    {
        result.data.reset();
        result.status = juce::Result::fail("Drum MIDI serialization failed");
        return result;
    }
    result.status = validateDrumMidiFileData(result.data, pattern, targetPpq);
    if (result.status.failed())
        result.data.reset();
    return result;
}

juce::Result validateDrumMidiFileData(const juce::MemoryBlock& data,
                                      const DrumPattern& source,
                                      int expectedPpq)
{
    if (data.isEmpty())
        return juce::Result::fail("Exported drum MIDI is empty");
    if (const auto validation = validateDrumPattern(source); validation.failed())
        return validation;
    juce::MemoryInputStream stream(data, false);
    juce::MidiFile file;
    if (!file.readFrom(stream) || file.getTimeFormat() != expectedPpq
        || file.getNumTracks() != 2)
        return juce::Result::fail("Exported drum MIDI structure is invalid");

    auto track = *file.getTrack(1);
    track.updateMatchedPairs();
    auto eventIndex = std::size_t{0};
    for (int index = 0; index < track.getNumEvents(); ++index)
    {
        const auto* holder = track.getEventPointer(index);
        if (!holder->message.isNoteOn() || holder->noteOffObject == nullptr)
            continue;
        if (eventIndex >= source.events.size())
            return juce::Result::fail("Exported drum MIDI contains extra notes");
        const auto& expected = source.events[eventIndex++];
        const auto start = static_cast<std::int64_t>(std::llround(
            holder->message.getTimeStamp()));
        if (holder->message.getChannel() != 10
            || holder->message.getNoteNumber() != generalMidiPitch(expected.lane)
            || static_cast<int>(holder->message.getVelocity()) != expected.velocity
            || start != rescale(expected.startTick, source.ppq, expectedPpq))
            return juce::Result::fail("Reopened drum MIDI differs from its source pattern");
    }
    if (eventIndex != source.events.size())
        return juce::Result::fail("Exported drum MIDI is missing notes");
    return juce::Result::ok();
}
}
