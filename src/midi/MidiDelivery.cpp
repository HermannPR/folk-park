#include "MidiDelivery.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

namespace folkpark::midi
{
namespace
{
std::int64_t rescaleTick(std::int64_t tick, int sourcePpq, int destinationPpq) noexcept
{
    return static_cast<std::int64_t>(std::llround(static_cast<double>(tick)
        * static_cast<double>(destinationPpq) / static_cast<double>(sourcePpq)));
}

void addAt(juce::MidiMessageSequence& sequence, juce::MidiMessage message, std::int64_t tick)
{
    message.setTimeStamp(static_cast<double>(tick));
    sequence.addEvent(std::move(message));
}

bool noteEventsMatch(const std::vector<NoteEvent>& expected,
                     const juce::MidiMessageSequence& source,
                     int sourcePpq,
                     int exportedPpq)
{
    auto track = source;
    track.updateMatchedPairs();
    std::vector<NoteEvent> actual;
    actual.reserve(expected.size());
    for (int index = 0; index < track.getNumEvents(); ++index)
    {
        const auto* holder = track.getEventPointer(index);
        if (!holder->message.isNoteOn() || holder->noteOffObject == nullptr)
            continue;
        const auto start = static_cast<std::int64_t>(std::llround(holder->message.getTimeStamp()));
        const auto end = static_cast<std::int64_t>(std::llround(
            holder->noteOffObject->message.getTimeStamp()));
        actual.push_back({start, end - start, holder->message.getNoteNumber(),
                          static_cast<int>(holder->message.getVelocity()),
                          holder->message.getChannel(), 1.0f, Articulation::normal});
    }
    if (actual.size() != expected.size())
        return false;
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const auto& left = expected[index];
        const auto& right = actual[index];
        if (rescaleTick(left.startTick, sourcePpq, exportedPpq) != right.startTick
            || rescaleTick(left.durationTicks, sourcePpq, exportedPpq) != right.durationTicks
            || left.pitch != right.pitch || left.velocity != right.velocity
            || left.channel != right.channel)
            return false;
    }
    return true;
}

std::vector<NoteEvent> quantiseEvents(const GeneratedClip& clip, int destinationPpq)
{
    std::vector<NoteEvent> result;
    result.reserve(clip.events.size());
    auto previousGroupEnd = std::int64_t{0};
    auto index = std::size_t{0};
    while (index < clip.events.size())
    {
        const auto originalStart = clip.events[index].startTick;
        auto groupEnd = index + 1;
        while (groupEnd < clip.events.size()
               && clip.events[groupEnd].startTick == originalStart)
            ++groupEnd;

        auto quantisedStart = rescaleTick(originalStart, clip.ppq, destinationPpq);
        if (clip.part != PartType::chords || index > 0)
            quantisedStart = std::max(quantisedStart, previousGroupEnd);
        auto currentGroupEnd = quantisedStart + 1;
        for (auto eventIndex = index; eventIndex < groupEnd; ++eventIndex)
        {
            const auto& source = clip.events[eventIndex];
            const auto quantisedEnd = std::max(quantisedStart + 1,
                rescaleTick(source.startTick + source.durationTicks, clip.ppq, destinationPpq));
            auto event = source;
            event.startTick = quantisedStart;
            event.durationTicks = quantisedEnd - quantisedStart;
            result.push_back(event);
            currentGroupEnd = std::max(currentGroupEnd, quantisedEnd);
        }
        previousGroupEnd = currentGroupEnd;
        index = groupEnd;
    }
    return result;
}
}

MidiExportResult createMidiFileData(const CompositionBundle& bundle, int targetPpq)
{
    MidiExportResult result;
    if (const auto validation = validateBundle(bundle); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (targetPpq < 24 || targetPpq > 9600)
    {
        result.status = juce::Result::fail("MIDI export PPQ is outside 24-9600");
        return result;
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote(targetPpq);
    juce::MidiMessageSequence metadata;
    addAt(metadata, juce::MidiMessage::tempoMetaEvent(
        juce::roundToInt(60000000.0 / bundle.intent.tempoBpm)), 0);
    addAt(metadata, juce::MidiMessage::timeSignatureMetaEvent(
        bundle.intent.timeSignature.numerator, bundle.intent.timeSignature.denominator), 0);
    addAt(metadata, juce::MidiMessage::textMetaEvent(3, "folk park composition"), 0);
    metadata.sort();
    file.addTrack(metadata);

    for (const auto& clip : bundle.clips)
    {
        juce::MidiMessageSequence track;
        addAt(track, juce::MidiMessage::textMetaEvent(3, stableId(clip.part)), 0);
        for (const auto& event : quantiseEvents(clip, targetPpq))
        {
            addAt(track, juce::MidiMessage::noteOn(event.channel, event.pitch,
                static_cast<juce::uint8>(event.velocity)), event.startTick);
            addAt(track, juce::MidiMessage::noteOff(event.channel, event.pitch),
                  event.startTick + event.durationTicks);
        }
        track.sort();
        track.updateMatchedPairs();
        file.addTrack(track);
    }

    juce::MemoryOutputStream stream(result.data, false);
    if (!file.writeTo(stream, 1))
    {
        result.data.reset();
        result.status = juce::Result::fail("Standards-compliant MIDI serialization failed");
        return result;
    }
    result.status = validateMidiFileData(result.data, bundle, targetPpq);
    if (result.status.failed())
        result.data.reset();
    return result;
}

juce::Result validateMidiFileData(const juce::MemoryBlock& data,
                                  const CompositionBundle& source,
                                  int expectedPpq)
{
    if (data.isEmpty())
        return juce::Result::fail("Exported MIDI is empty");
    if (const auto validation = validateBundle(source); validation.failed())
        return validation;
    juce::MemoryInputStream stream(data, false);
    juce::MidiFile file;
    if (!file.readFrom(stream))
        return juce::Result::fail("Exported MIDI cannot be reopened");
    if (file.getTimeFormat() != expectedPpq)
        return juce::Result::fail("Exported MIDI PPQ differs from the requested resolution");
    if (file.getNumTracks() != static_cast<int>(source.clips.size() + 1))
        return juce::Result::fail("Exported MIDI track count differs from the source bundle");

    auto foundTempo = false;
    auto foundSignature = false;
    const auto* metadata = file.getTrack(0);
    for (int index = 0; index < metadata->getNumEvents(); ++index)
    {
        const auto message = metadata->getEventPointer(index)->message;
        if (message.isTempoMetaEvent())
            foundTempo = std::abs(message.getTempoSecondsPerQuarterNote()
                - 60.0 / source.intent.tempoBpm) < 1.0e-5;
        if (message.isTimeSignatureMetaEvent())
        {
            auto numerator = 0;
            auto denominator = 0;
            message.getTimeSignatureInfo(numerator, denominator);
            foundSignature = numerator == source.intent.timeSignature.numerator
                && denominator == source.intent.timeSignature.denominator;
        }
    }
    if (!foundTempo || !foundSignature)
        return juce::Result::fail("Exported MIDI is missing matching tempo/time-signature metadata");

    for (std::size_t index = 0; index < source.clips.size(); ++index)
    {
        const auto expectedEvents = quantiseEvents(source.clips[index], expectedPpq);
        if (!noteEventsMatch(expectedEvents, *file.getTrack(static_cast<int>(index + 1)),
                             expectedPpq, expectedPpq))
            return juce::Result::fail("Reopened MIDI note events differ from the source clip");
    }
    return juce::Result::ok();
}

juce::Result writeMidiFile(const CompositionBundle& bundle,
                           const juce::File& destination,
                           int targetPpq)
{
    if (destination == juce::File{} || destination.isDirectory())
        return juce::Result::fail("MIDI export destination is invalid");
    const auto exported = createMidiFileData(bundle, targetPpq);
    if (!exported.succeeded())
        return exported.status;
    if (!destination.replaceWithData(exported.data.getData(), exported.data.getSize()))
        return juce::Result::fail("MIDI export destination could not be written");
    return juce::Result::ok();
}

juce::File writeMidiToTemporaryFile(const CompositionBundle& bundle, int targetPpq)
{
    if (validateBundle(bundle).failed())
        return {};
    const auto filename = "folk-park-" + bundle.intent.requestId + ".mid";
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(filename);
    return writeMidiFile(bundle, file, targetPpq).wasOk() ? file : juce::File{};
}

juce::Result DirectMidiPlayer::publish(const CompositionBundle& bundle)
{
    if (const auto validation = validateBundle(bundle); validation.failed())
        return validation;
    std::size_t noteCount = 0;
    for (const auto& clip : bundle.clips)
        noteCount += clip.events.size();
    if (noteCount * 2 > maximumScheduledEvents)
        return juce::Result::fail("Direct MIDI schedule exceeds its fixed event capacity");
    if (pendingIndex.load(std::memory_order_acquire) >= 0
        || writer.test_and_set(std::memory_order_acquire))
        return juce::Result::fail("A direct MIDI schedule is already pending");

    const auto releaseWriter = [this] { writer.clear(std::memory_order_release); };
    const auto inactive = 1 - activeIndex.load(std::memory_order_acquire);
    auto& schedule = schedules[static_cast<std::size_t>(inactive)];
    schedule.eventCount = 0;
    schedule.durationSeconds = 0.0;
    for (const auto& clip : bundle.clips)
    {
        const auto secondsPerTick = 60.0 / (clip.tempoBpm * static_cast<double>(clip.ppq));
        schedule.durationSeconds = std::max(schedule.durationSeconds,
            static_cast<double>(clip.lengthTicks) * secondsPerTick);
        for (const auto& event : clip.events)
        {
            auto& noteOn = schedule.events[schedule.eventCount++];
            noteOn.timeSeconds = static_cast<double>(event.startTick) * secondsPerTick;
            noteOn.bytes = {static_cast<std::uint8_t>(0x90 | (event.channel - 1)),
                            static_cast<std::uint8_t>(event.pitch),
                            static_cast<std::uint8_t>(event.velocity)};
            noteOn.noteOn = true;
            auto& noteOff = schedule.events[schedule.eventCount++];
            noteOff.timeSeconds = static_cast<double>(event.startTick + event.durationTicks)
                * secondsPerTick;
            noteOff.bytes = {static_cast<std::uint8_t>(0x80 | (event.channel - 1)),
                             static_cast<std::uint8_t>(event.pitch), 0};
            noteOff.noteOn = false;
        }
    }
    std::sort(schedule.events.begin(), schedule.events.begin()
                  + static_cast<std::ptrdiff_t>(schedule.eventCount),
        [](const ScheduledEvent& left, const ScheduledEvent& right)
        {
            return std::tie(left.timeSeconds, left.noteOn, left.bytes[0], left.bytes[1])
                < std::tie(right.timeSeconds, right.noteOn, right.bytes[0], right.bytes[1]);
        });

    auto simultaneous = 0;
    auto previousTime = -1.0;
    for (std::size_t index = 0; index < schedule.eventCount; ++index)
    {
        const auto time = schedule.events[index].timeSeconds;
        simultaneous = std::abs(time - previousTime) <= 1.0e-12 ? simultaneous + 1 : 1;
        previousTime = time;
        if (simultaneous > maximumEventsPerBlock)
        {
            schedule.eventCount = 0;
            releaseWriter();
            return juce::Result::fail("Direct MIDI schedule has too many simultaneous events");
        }
    }
    pendingIndex.store(inactive, std::memory_order_release);
    releaseWriter();
    return juce::Result::ok();
}

void DirectMidiPlayer::reset() noexcept
{
    pendingIndex.store(-1, std::memory_order_release);
    stopRequested.store(false, std::memory_order_release);
    playing.store(false, std::memory_order_release);
    activeNotes = {};
    nextEventIndex = 0;
    cursorSeconds = 0.0;
}

int DirectMidiPlayer::emitAllTrackedNoteOffs(juce::MidiBuffer& output, int sampleOffset) noexcept
{
    auto emitted = 0;
    for (std::size_t channel = 0; channel < activeNotes.size(); ++channel)
    {
        for (std::size_t pitch = 0; pitch < activeNotes[channel].size(); ++pitch)
        {
            if (!activeNotes[channel][pitch])
                continue;
            const std::array<std::uint8_t, 3> bytes{
                static_cast<std::uint8_t>(0x80 | channel), static_cast<std::uint8_t>(pitch), 0};
            output.addEvent(bytes.data(), static_cast<int>(bytes.size()), sampleOffset);
            activeNotes[channel][pitch] = false;
            ++emitted;
        }
    }
    return emitted;
}

DirectMidiPlayer::RenderResult DirectMidiPlayer::renderBlock(juce::MidiBuffer& output,
                                                              int numberOfSamples,
                                                              double sampleRate) noexcept
{
    RenderResult result;
    if (numberOfSamples <= 0 || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return result;
    if (stopRequested.exchange(false, std::memory_order_acq_rel))
    {
        pendingIndex.store(-1, std::memory_order_release);
        result.emittedEvents = emitAllTrackedNoteOffs(output, 0);
        playing.store(false, std::memory_order_release);
        nextEventIndex = 0;
        cursorSeconds = 0.0;
        result.completed = true;
        return result;
    }

    if (const auto pending = pendingIndex.exchange(-1, std::memory_order_acquire); pending >= 0)
    {
        result.emittedEvents += emitAllTrackedNoteOffs(output, 0);
        activeIndex.store(pending, std::memory_order_release);
        nextEventIndex = 0;
        cursorSeconds = 0.0;
        playing.store(true, std::memory_order_release);
    }
    if (!playing.load(std::memory_order_acquire))
        return result;

    const auto& schedule = schedules[static_cast<std::size_t>(
        activeIndex.load(std::memory_order_acquire))];
    const auto blockDuration = static_cast<double>(numberOfSamples) / sampleRate;
    const auto endSeconds = cursorSeconds + blockDuration;
    while (nextEventIndex < schedule.eventCount)
    {
        const auto& event = schedule.events[nextEventIndex];
        if (event.timeSeconds >= endSeconds)
            break;
        if (result.emittedEvents >= maximumEventsPerBlock)
        {
            result.overflow = true;
            result.emittedEvents += emitAllTrackedNoteOffs(output, numberOfSamples - 1);
            playing.store(false, std::memory_order_release);
            result.completed = true;
            return result;
        }
        const auto offset = juce::jlimit(0, numberOfSamples - 1,
            static_cast<int>(std::floor((event.timeSeconds - cursorSeconds) * sampleRate + 0.5)));
        output.addEvent(event.bytes.data(), static_cast<int>(event.bytes.size()), offset);
        const auto channel = static_cast<std::size_t>(event.bytes[0] & 0x0f);
        const auto pitch = static_cast<std::size_t>(event.bytes[1]);
        activeNotes[channel][pitch] = event.noteOn;
        ++nextEventIndex;
        ++result.emittedEvents;
    }
    cursorSeconds = endSeconds;
    if (nextEventIndex >= schedule.eventCount)
    {
        result.emittedEvents += emitAllTrackedNoteOffs(output, numberOfSamples - 1);
        playing.store(false, std::memory_order_release);
        result.completed = true;
    }
    return result;
}
}
