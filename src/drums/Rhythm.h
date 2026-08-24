#pragma once

#include "midi/Composition.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace folkpark::drums
{
inline constexpr int rhythmPpq = midi::compositionPpq;
inline constexpr std::size_t maximumRhythmEvents = 8192;
inline constexpr auto rhythmGeneratorVersion = "1.0.0-r1";
inline constexpr auto synthesizedCoreKitId = "synth_core_v1";

enum class DrumLane : std::uint8_t
{
    kick,
    snare,
    closedHat,
    openHat,
    percussion,
    count
};

enum class RhythmGenre : std::uint8_t
{
    indieRock,
    eurodance,
    techno,
    funk,
    jazz,
    count
};

enum class DrumArticulation : std::uint8_t
{
    normal,
    accent,
    ghost,
    flam,
    count
};

struct RhythmIntent
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String requestId;
    std::uint32_t seed = 12345;
    double tempoBpm = 124.0;
    midi::TimeSignature timeSignature;
    int lengthBars = 4;
    RhythmGenre genre = RhythmGenre::techno;
    float density = 0.55f;
    float complexity = 0.45f;
    float swing = 0.0f;
    float humanization = 0.08f;
    float fillAmount = 0.25f;
    std::array<DrumLane, 5> lanes{DrumLane::kick, DrumLane::snare,
                                  DrumLane::closedHat, DrumLane::openHat,
                                  DrumLane::percussion};
    std::size_t laneCount = 5;
    std::size_t maximumEvents = 2048;
};

struct DrumEvent
{
    std::int64_t startTick = 0;
    std::int64_t durationTicks = rhythmPpq / 8;
    DrumLane lane = DrumLane::kick;
    int velocity = 100;
    float probability = 1.0f;
    DrumArticulation articulation = DrumArticulation::normal;

    friend bool operator==(const DrumEvent& left, const DrumEvent& right) noexcept
    {
        return left.startTick == right.startTick
            && left.durationTicks == right.durationTicks
            && left.lane == right.lane
            && left.velocity == right.velocity
            && std::abs(left.probability - right.probability) < 0.0000001f
            && left.articulation == right.articulation;
    }
};

struct DrumPattern
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String id;
    juce::String requestId;
    int ppq = rhythmPpq;
    std::int64_t lengthTicks = rhythmPpq * 16;
    double tempoBpm = 124.0;
    midi::TimeSignature timeSignature;
    std::uint32_t seed = 0;
    RhythmGenre genre = RhythmGenre::techno;
    juce::String kitId{synthesizedCoreKitId};
    juce::String generatorVersion{rhythmGeneratorVersion};
    juce::String parentPatternId;
    std::int64_t createdUnixMs = 0;
    std::vector<DrumEvent> events;
};

struct SynthDrumKit
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String id{synthesizedCoreKitId};
    float kickTuneHz = 48.0f;
    float kickDecaySeconds = 0.34f;
    float kickClick = 0.35f;
    float snareTuneHz = 185.0f;
    float snareDecaySeconds = 0.22f;
    float snareNoise = 0.78f;
    float closedHatDecaySeconds = 0.065f;
    float openHatDecaySeconds = 0.32f;
    float hatMetal = 0.68f;
    float percussionTuneHz = 230.0f;
    float percussionDecaySeconds = 0.16f;
    float drive = 0.12f;
    float outputGain = 0.72f;
};

[[nodiscard]] juce::String stableId(DrumLane lane);
[[nodiscard]] juce::String stableId(RhythmGenre genre);
[[nodiscard]] juce::String stableId(DrumArticulation articulation);
[[nodiscard]] juce::Result normaliseAndValidate(RhythmIntent& intent);
[[nodiscard]] juce::Result validateRhythmIntent(const RhythmIntent& intent);
[[nodiscard]] juce::Result validateDrumPattern(const DrumPattern& pattern);
[[nodiscard]] juce::Result validateSynthDrumKit(const SynthDrumKit& kit);
}
