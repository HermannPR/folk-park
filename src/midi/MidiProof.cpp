#include "MidiProof.h"

namespace folkpark::midi
{
namespace
{
constexpr int ticksPerQuarterNote = 960;

void addEvent(juce::MidiMessageSequence& sequence, juce::MidiMessage message, double timestamp)
{
    message.setTimeStamp(timestamp);
    sequence.addEvent(message);
}
}

juce::MemoryBlock createM0ProofMidi()
{
    juce::MidiMessageSequence track;
    addEvent(track, juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
    addEvent(track, juce::MidiMessage::tempoMetaEvent(500000), 0.0);
    addEvent(track, juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0.0);
    addEvent(track, juce::MidiMessage::noteOff(1, 60), ticksPerQuarterNote);
    track.sort();
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote(ticksPerQuarterNote);
    file.addTrack(track);

    juce::MemoryBlock data;
    juce::MemoryOutputStream stream(data, false);
    if (!file.writeTo(stream, 1))
        data.reset();

    return data;
}

juce::Result validateM0ProofMidi(const juce::MemoryBlock& data)
{
    if (data.isEmpty())
        return juce::Result::fail("MIDI data is empty");

    juce::MemoryInputStream stream(data, false);
    juce::MidiFile file;
    if (!file.readFrom(stream))
        return juce::Result::fail("MIDI file cannot be reopened");
    if (file.getTimeFormat() != ticksPerQuarterNote)
        return juce::Result::fail("Unexpected PPQ resolution");
    if (file.getNumTracks() != 1)
        return juce::Result::fail("Expected exactly one track");

    const auto* track = file.getTrack(0);
    bool foundNoteOn = false;
    bool foundNoteOff = false;
    for (int index = 0; index < track->getNumEvents(); ++index)
    {
        const auto message = track->getEventPointer(index)->message;
        foundNoteOn = foundNoteOn || (message.isNoteOn() && message.getNoteNumber() == 60);
        foundNoteOff = foundNoteOff || (message.isNoteOff() && message.getNoteNumber() == 60);
    }

    if (!foundNoteOn || !foundNoteOff)
        return juce::Result::fail("Proof note lifecycle is incomplete");
    return juce::Result::ok();
}

juce::File writeM0ProofMidiToTemporaryFile()
{
    const auto data = createM0ProofMidi();
    if (validateM0ProofMidi(data).failed())
        return {};

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("folk-park-m0-proof.mid");
    if (!file.replaceWithData(data.getData(), data.getSize()))
        return {};
    return file;
}
}
