#include "midi/CompositionSession.h"
#include "midi/MidiDelivery.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

folkpark::midi::MusicIntent standardIntent()
{
    folkpark::midi::MusicIntent intent;
    intent.requestId = folkpark::midi::deterministicUuid(intent.seed, "midi-delivery-request");
    return intent;
}

folkpark::midi::CompositionBundle exactTimingBundle()
{
    using namespace folkpark::midi;
    MusicIntent intent;
    intent.seed = 9001;
    intent.requestId = deterministicUuid(intent.seed, "direct-midi-exact");
    intent.key = KeyRoot::c;
    intent.scale = ScaleType::major;
    intent.tempoBpm = 120.0;
    intent.partCount = 1;
    intent.parts[0] = PartType::melody;
    intent.constraints.lowestMidiNote = 0;
    intent.constraints.highestMidiNote = 127;
    intent.constraints.maxPolyphony = 1;
    intent.constraints.maximumEvents = 8;

    GeneratedClip clip;
    clip.id = deterministicUuid(intent.seed, "direct-midi-clip");
    clip.part = PartType::melody;
    clip.lengthTicks = compositionPpq * 4;
    clip.tempoBpm = intent.tempoBpm;
    clip.timeSignature = intent.timeSignature;
    clip.key = intent.key;
    clip.scale = intent.scale;
    clip.seed = intent.seed;
    clip.createdUnixMs = 1;
    clip.events.push_back({compositionPpq / 2, compositionPpq / 2, 60, 100, 1,
                           1.0f, Articulation::normal});
    return {intent, {clip}};
}

void testExportParity()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    auto intent = standardIntent();
    intent.tempoBpm = 137.5;
    intent.timeSignature = {7, 8};
    intent.constraints.lowestMidiNote = 42;
    intent.constraints.highestMidiNote = 91;
    const auto generated = engine.generate(intent, 123456);
    expect(generated.succeeded(), "MIDI export fixture must generate");
    if (!generated.succeeded())
        return;

    for (const auto ppq : {96, 480, 960, 1920})
    {
        const auto exported = createMidiFileData(generated.bundle, ppq);
        expect(exported.succeeded(), "Every supported test PPQ must export and reopen");
        if (!exported.succeeded())
            std::cerr << "  PPQ " << ppq << ": " << exported.status.getErrorMessage() << '\n';
        if (exported.succeeded())
            expect(validateMidiFileData(exported.data, generated.bundle, ppq).wasOk(),
                   "Reopened SMF must match source notes, tempo, meter, and PPQ");
    }

    expect(createMidiFileData(generated.bundle, 23).status.failed(),
           "Out-of-range export PPQ must be rejected");
    auto malformed = generated.bundle;
    malformed.clips.front().events.front().pitch = 128;
    expect(createMidiFileData(malformed).status.failed(),
           "Malformed clip cannot cross the export boundary");
    const juce::MemoryBlock truncated("MThd", 4);
    expect(validateMidiFileData(truncated, generated.bundle).failed(),
           "Truncated MIDI data must fail reopen validation");
}

void testFileLifecycle()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    const auto generated = engine.generate(standardIntent(), 2);
    expect(generated.succeeded(), "Temporary-file fixture must generate");
    if (!generated.succeeded())
        return;
    const auto file = writeMidiToTemporaryFile(generated.bundle, 480);
    expect(file.existsAsFile() && file.getSize() > 0,
           "Accepted MIDI must be writable to a non-empty temporary drag file");
    expect(file.deleteFile(), "Test-owned temporary MIDI file must be removable after validation");
}

void testDirectOffsetsAndStop()
{
    using namespace folkpark::midi;
    const auto bundle = exactTimingBundle();
    expect(validateBundle(bundle).wasOk(), "Exact direct-MIDI fixture must validate");
    DirectMidiPlayer player;
    expect(player.publish(bundle).wasOk(), "Validated accepted bundle must publish atomically");
    expect(player.publish(bundle).failed(), "A second schedule cannot replace an unconsumed pending schedule");

    constexpr auto blockSize = 512;
    constexpr auto sampleRate = 48000.0;
    std::vector<int> noteOnSamples;
    std::vector<int> noteOffSamples;
    for (int block = 0; block < 60 && !(!player.isPlaying() && !player.hasPendingSchedule()); ++block)
    {
        juce::MidiBuffer output;
        output.ensureSize(2048);
        const auto render = player.renderBlock(output, blockSize, sampleRate);
        expect(!render.overflow, "Bounded direct schedule must never overflow a block");
        for (const auto metadata : output)
        {
            if (metadata.getMessage().isNoteOn())
                noteOnSamples.push_back(block * blockSize + metadata.samplePosition);
            if (metadata.getMessage().isNoteOff())
                noteOffSamples.push_back(block * blockSize + metadata.samplePosition);
        }
    }
    expect(noteOnSamples == std::vector<int>{12000},
           "Direct MIDI note-on must preserve its exact sample offset across blocks");
    expect(noteOffSamples == std::vector<int>{24000},
           "Direct MIDI note-off must preserve its exact sample offset across blocks");

    expect(player.publish(bundle).wasOk(), "Completed direct player must accept another schedule");
    juce::MidiBuffer first;
    first.ensureSize(2048);
    const auto firstRender = player.renderBlock(first, 13000, sampleRate);
    expect(!firstRender.overflow, "Large direct-MIDI test block must remain bounded");
    expect(player.isPlaying(), "Direct player must remain active after the test note begins");
    player.requestStop();
    juce::MidiBuffer stopped;
    stopped.ensureSize(2048);
    const auto result = player.renderBlock(stopped, blockSize, sampleRate);
    auto foundOffsetZeroNoteOff = false;
    for (const auto metadata : stopped)
        foundOffsetZeroNoteOff = foundOffsetZeroNoteOff
            || (metadata.getMessage().isNoteOff() && metadata.samplePosition == 0);
    expect(result.completed && foundOffsetZeroNoteOff && !player.isPlaying(),
           "Explicit Stop must emit tracked note-offs at offset zero and finish playback");
}

void testCandidateAcceptance()
{
    using namespace folkpark::midi;
    CompositionSession session;
    expect(!session.getAcceptedBundle().has_value(), "New session must contain no accepted MIDI");
    expect(session.acceptCandidate().failed(), "Acceptance must fail before candidate generation");
    expect(session.generateCandidate(standardIntent()).wasOk(), "Session must generate a candidate");
    auto snapshot = session.getSnapshot();
    expect(snapshot.hasCandidate && !snapshot.hasAccepted && snapshot.candidateNoteCount > 0,
           "Generated MIDI must remain an unaccepted candidate");
    expect(session.acceptCandidate().wasOk(), "Explicit acceptance must publish the reviewed candidate");
    snapshot = session.getSnapshot();
    expect(snapshot.hasAccepted && snapshot.acceptedNoteCount == snapshot.candidateNoteCount,
           "Accepted snapshot must reference the reviewed candidate notes");
    expect(session.moreLikeCandidate(1).wasOk(), "More Like This must create a new reviewable child");
    snapshot = session.getSnapshot();
    expect(snapshot.hasAccepted && snapshot.candidateRequestId != snapshot.acceptedRequestId,
           "A variation cannot silently replace the last accepted composition");
    expect(session.getAcceptedBundle().has_value(), "Previously accepted MIDI must remain deliverable");
}
}

int main()
{
    testExportParity();
    testFileLifecycle();
    testDirectOffsetsAndStop();
    testCandidateAcceptance();
    if (failures == 0)
        std::cout << "PASS: M3 MIDI export parity, lifecycle, direct offsets, stop, and explicit acceptance\n";
    return failures == 0 ? 0 : 1;
}
