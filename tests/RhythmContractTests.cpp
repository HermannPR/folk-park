#include "drums/Rhythm.h"

#include <cmath>
#include <iostream>
#include <limits>

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
}

int main()
{
    using namespace folkpark;

    drums::RhythmIntent intent;
    intent.requestId = midi::deterministicUuid(1, "rhythm-contract");
    expect(drums::validateRhythmIntent(intent).wasOk(),
           "Default synthesized-first rhythm intent must validate");

    for (int genre = 0; genre < static_cast<int>(drums::RhythmGenre::count); ++genre)
    {
        intent.genre = static_cast<drums::RhythmGenre>(genre);
        expect(drums::stableId(intent.genre).isNotEmpty(),
               "Every producer-selected genre requires a stable ID");
        expect(drums::validateRhythmIntent(intent).wasOk(),
               "Every producer-selected genre must validate through one contract");
    }

    auto invalidIntent = intent;
    invalidIntent.lanes[1] = invalidIntent.lanes[0];
    expect(drums::validateRhythmIntent(invalidIntent).failed(),
           "Duplicate drum lanes must be rejected");
    invalidIntent = intent;
    invalidIntent.swing = std::numeric_limits<float>::infinity();
    expect(drums::validateRhythmIntent(invalidIntent).failed(),
           "Non-finite rhythm macros must be rejected");

    drums::SynthDrumKit kit;
    expect(drums::validateSynthDrumKit(kit).wasOk(),
           "The authored synthesized core kit must validate without samples");
    kit.openHatDecaySeconds = kit.closedHatDecaySeconds;
    expect(drums::validateSynthDrumKit(kit).failed(),
           "Invalid synthesized hat relationships must be rejected");

    drums::DrumPattern pattern;
    pattern.id = midi::deterministicUuid(2, "drum-pattern");
    pattern.requestId = intent.requestId;
    pattern.seed = intent.seed;
    pattern.events = {
        {0, drums::rhythmPpq / 8, drums::DrumLane::kick, 112, 1.0f,
         drums::DrumArticulation::accent},
        {drums::rhythmPpq, drums::rhythmPpq / 8, drums::DrumLane::snare, 104, 1.0f,
         drums::DrumArticulation::normal}
    };
    expect(drums::validateDrumPattern(pattern).wasOk(),
           "A bounded synthesized drum pattern must validate");

    auto invalidPattern = pattern;
    invalidPattern.kitId = "downloaded_unknown_pack";
    expect(drums::validateDrumPattern(invalidPattern).failed(),
           "R1 must reject unreviewed sample-kit identities");
    invalidPattern = pattern;
    invalidPattern.events[1].startTick = -1;
    expect(drums::validateDrumPattern(invalidPattern).failed(),
           "Out-of-bounds drum timing must be rejected");
    invalidPattern = pattern;
    invalidPattern.events[0].probability = std::nanf("");
    expect(drums::validateDrumPattern(invalidPattern).failed(),
           "Non-finite drum performance values must be rejected");

    if (failures == 0)
    {
        std::cout << "Rhythm Lab contract tests passed\n";
        return 0;
    }
    std::cerr << failures << " Rhythm Lab contract test(s) failed\n";
    return 1;
}
