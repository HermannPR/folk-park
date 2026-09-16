#include "drums/DrumMidi.h"
#include "drums/RhythmGenerator.h"
#include "drums/RhythmSession.h"

#include <iostream>
#include <set>
#include <tuple>

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

folkpark::drums::RhythmIntent intentFor(folkpark::drums::RhythmGenre genre)
{
    folkpark::drums::RhythmIntent intent;
    intent.genre = genre;
    intent.seed = 24680;
    intent.requestId = folkpark::midi::deterministicUuid(
        intent.seed, "rhythm-generator-" + folkpark::drums::stableId(genre));
    return intent;
}

std::string fingerprint(const folkpark::drums::DrumPattern& pattern)
{
    std::string result;
    for (const auto& event : pattern.events)
        result += std::to_string(event.startTick) + ":"
            + std::to_string(static_cast<int>(event.lane)) + ";";
    return result;
}
}

int main()
{
    using namespace folkpark::drums;
    RhythmGenerator generator;
    std::set<std::string> genreFingerprints;

    for (int genreIndex = 0; genreIndex < static_cast<int>(RhythmGenre::count); ++genreIndex)
    {
        const auto genre = static_cast<RhythmGenre>(genreIndex);
        const auto intent = intentFor(genre);
        const auto first = generator.generate(intent, 100);
        const auto second = generator.generate(intent, 100);
        expect(first.succeeded() && second.succeeded(),
               "Every producer-selected rhythm profile must generate");
        if (!first.succeeded() || !second.succeeded())
            continue;
        expect(validateDrumPattern(first.pattern).wasOk(),
               "Every generated genre pattern must preserve the strict contract");
        expect(first.pattern.events == second.pattern.events,
               "A seed and genre must generate the same drum events");
        expect(!first.pattern.events.empty(),
               "Every producer-selected genre must create audible drum triggers");
        genreFingerprints.insert(fingerprint(first.pattern));
    }
    expect(genreFingerprints.size() == static_cast<std::size_t>(RhythmGenre::count),
           "The five genre profiles must produce materially distinct timing fingerprints");

    auto cappedIntent = intentFor(RhythmGenre::funk);
    cappedIntent.maximumEvents = 12;
    cappedIntent.lengthBars = 64;
    cappedIntent.density = 1.0f;
    cappedIntent.complexity = 1.0f;
    const auto capped = generator.generate(cappedIntent, 100);
    expect(capped.succeeded() && capped.pattern.events.size() <= 12,
           "Long dense rhythms must obey the requested event cap");

    RhythmSession session;
    auto techno = intentFor(RhythmGenre::techno);
    expect(session.generateCandidate(techno).wasOk(),
           "Rhythm session must stage a candidate");
    expect(!session.getAccepted().has_value(),
           "A generated drum candidate must not become deliverable implicitly");
    expect(session.acceptCandidate().wasOk() && session.getAccepted().has_value(),
           "Explicit acceptance must enable the drum delivery boundary");
    const auto acceptedId = session.getAccepted()->id;
    expect(session.moreLikeCandidate(3).wasOk(),
           "More Like This must create a bounded rhythm variation");
    expect(session.getAccepted()->id == acceptedId
               && session.getCandidate()->parentPatternId == acceptedId,
           "A new candidate must retain lineage without replacing the accepted rhythm");

    for (const auto ppq : {96, 480, 960, 1920})
    {
        const auto exported = createDrumMidiFileData(*session.getAccepted(), ppq);
        expect(exported.succeeded(),
               "Accepted synthesized drums must export standards-compliant MIDI");
        if (exported.succeeded())
            expect(validateDrumMidiFileData(exported.data, *session.getAccepted(), ppq).wasOk(),
                   "Reopened drum MIDI must match lane, timing, velocity, and channel 10");
    }

    if (failures == 0)
    {
        std::cout << "Rhythm generator, acceptance, and MIDI tests passed\n";
        return 0;
    }
    std::cerr << failures << " rhythm generator test(s) failed\n";
    return 1;
}
