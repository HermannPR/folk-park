#include "Rhythm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>

namespace folkpark::drums
{
namespace
{
constexpr std::array<const char*, 5> laneIds{
    "kick", "snare", "closed_hat", "open_hat", "percussion"
};
constexpr std::array<const char*, 5> genreIds{
    "indie_rock", "eurodance", "techno", "funk", "jazz"
};
constexpr std::array<const char*, 4> articulationIds{
    "normal", "accent", "ghost", "flam"
};

template <typename Enum>
bool validEnum(Enum value, Enum count) noexcept
{
    const auto raw = static_cast<std::underlying_type_t<Enum>>(value);
    return raw >= 0 && raw < static_cast<std::underlying_type_t<Enum>>(count);
}

template <typename Enum, std::size_t Size>
juce::String enumId(Enum value, Enum count, const std::array<const char*, Size>& ids)
{
    if (!validEnum(value, count))
        return {};
    return ids[static_cast<std::size_t>(value)];
}

bool validTimeSignature(const midi::TimeSignature& signature) noexcept
{
    if (signature.numerator < 2 || signature.numerator > 12)
        return false;
    return signature.denominator == 2 || signature.denominator == 4
        || signature.denominator == 8 || signature.denominator == 16;
}

bool finiteUnit(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

bool finiteInRange(float value, float minimum, float maximum) noexcept
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

juce::String stableId(DrumLane lane)
{
    return enumId(lane, DrumLane::count, laneIds);
}

juce::String stableId(RhythmGenre genre)
{
    return enumId(genre, RhythmGenre::count, genreIds);
}

juce::String stableId(DrumArticulation articulation)
{
    return enumId(articulation, DrumArticulation::count, articulationIds);
}

juce::Result normaliseAndValidate(RhythmIntent& intent)
{
    intent.density = juce::jlimit(0.0f, 1.0f, intent.density);
    intent.complexity = juce::jlimit(0.0f, 1.0f, intent.complexity);
    intent.swing = juce::jlimit(0.0f, 0.75f, intent.swing);
    intent.humanization = juce::jlimit(0.0f, 1.0f, intent.humanization);
    intent.fillAmount = juce::jlimit(0.0f, 1.0f, intent.fillAmount);
    intent.maximumEvents = juce::jlimit<std::size_t>(1, maximumRhythmEvents,
                                                      intent.maximumEvents);
    return validateRhythmIntent(intent);
}

juce::Result validateRhythmIntent(const RhythmIntent& intent)
{
    if (intent.schemaVersion != RhythmIntent::currentSchemaVersion)
        return juce::Result::fail("Unsupported rhythm intent schema");
    if (!midi::isUuid(intent.requestId))
        return juce::Result::fail("Rhythm intent request ID must be a UUID");
    if (!std::isfinite(intent.tempoBpm) || intent.tempoBpm < 20.0 || intent.tempoBpm > 400.0)
        return juce::Result::fail("Rhythm tempo is outside the supported range");
    if (!validTimeSignature(intent.timeSignature))
        return juce::Result::fail("Unsupported rhythm time signature");
    if (intent.lengthBars < 1 || intent.lengthBars > 64)
        return juce::Result::fail("Rhythm length is outside the supported range");
    if (!validEnum(intent.genre, RhythmGenre::count))
        return juce::Result::fail("Unsupported rhythm genre profile");
    if (!finiteUnit(intent.density) || !finiteUnit(intent.complexity)
        || !finiteInRange(intent.swing, 0.0f, 0.75f)
        || !finiteUnit(intent.humanization) || !finiteUnit(intent.fillAmount))
        return juce::Result::fail("Rhythm macro is invalid");
    if (intent.laneCount == 0 || intent.laneCount > intent.lanes.size())
        return juce::Result::fail("Rhythm lane count is invalid");
    if (intent.maximumEvents == 0 || intent.maximumEvents > maximumRhythmEvents)
        return juce::Result::fail("Rhythm event limit is invalid");

    std::array<bool, static_cast<std::size_t>(DrumLane::count)> seen{};
    for (std::size_t index = 0; index < intent.laneCount; ++index)
    {
        const auto lane = intent.lanes[index];
        if (!validEnum(lane, DrumLane::count))
            return juce::Result::fail("Unsupported rhythm lane");
        const auto raw = static_cast<std::size_t>(lane);
        if (seen[raw])
            return juce::Result::fail("Rhythm lanes must be unique");
        seen[raw] = true;
    }
    return juce::Result::ok();
}

juce::Result validateDrumPattern(const DrumPattern& pattern)
{
    if (pattern.schemaVersion != DrumPattern::currentSchemaVersion)
        return juce::Result::fail("Unsupported drum pattern schema");
    if (!midi::isUuid(pattern.id) || !midi::isUuid(pattern.requestId))
        return juce::Result::fail("Drum pattern IDs must be UUIDs");
    if (pattern.ppq < 24 || pattern.ppq > 9600 || pattern.lengthTicks <= 0)
        return juce::Result::fail("Drum pattern timing is invalid");
    if (!std::isfinite(pattern.tempoBpm) || pattern.tempoBpm < 20.0 || pattern.tempoBpm > 400.0)
        return juce::Result::fail("Drum pattern tempo is invalid");
    if (!validTimeSignature(pattern.timeSignature)
        || !validEnum(pattern.genre, RhythmGenre::count))
        return juce::Result::fail("Drum pattern musical context is invalid");
    if (pattern.kitId != synthesizedCoreKitId)
        return juce::Result::fail("R1 drum patterns require the synthesized core kit");
    if (pattern.generatorVersion.isEmpty() || pattern.generatorVersion.length() > 32)
        return juce::Result::fail("Drum generator version is invalid");
    if (pattern.parentPatternId.isNotEmpty() && !midi::isUuid(pattern.parentPatternId))
        return juce::Result::fail("Drum parent pattern ID is invalid");
    if (pattern.createdUnixMs < 0 || pattern.events.size() > maximumRhythmEvents)
        return juce::Result::fail("Drum pattern metadata is invalid");

    std::int64_t previousTick = -1;
    auto previousLane = DrumLane::kick;
    for (const auto& event : pattern.events)
    {
        if (event.startTick < 0 || event.startTick >= pattern.lengthTicks
            || event.durationTicks <= 0
            || event.startTick + event.durationTicks > pattern.lengthTicks)
            return juce::Result::fail("Drum event timing is invalid");
        if (!validEnum(event.lane, DrumLane::count)
            || !validEnum(event.articulation, DrumArticulation::count))
            return juce::Result::fail("Drum event enum is invalid");
        if (event.velocity < 1 || event.velocity > 127 || !finiteUnit(event.probability))
            return juce::Result::fail("Drum event performance value is invalid");
        if (event.startTick < previousTick
            || (event.startTick == previousTick
                && static_cast<int>(event.lane) < static_cast<int>(previousLane)))
            return juce::Result::fail("Drum events must be in stable time/lane order");
        previousTick = event.startTick;
        previousLane = event.lane;
    }
    return juce::Result::ok();
}

juce::Result validateSynthDrumKit(const SynthDrumKit& kit)
{
    if (kit.schemaVersion != SynthDrumKit::currentSchemaVersion
        || kit.id != synthesizedCoreKitId)
        return juce::Result::fail("Unsupported synthesized drum kit");
    if (!finiteInRange(kit.kickTuneHz, 30.0f, 100.0f)
        || !finiteInRange(kit.kickDecaySeconds, 0.04f, 2.0f)
        || !finiteUnit(kit.kickClick)
        || !finiteInRange(kit.snareTuneHz, 90.0f, 400.0f)
        || !finiteInRange(kit.snareDecaySeconds, 0.03f, 2.0f)
        || !finiteUnit(kit.snareNoise)
        || !finiteInRange(kit.closedHatDecaySeconds, 0.01f, 0.5f)
        || !finiteInRange(kit.openHatDecaySeconds, 0.05f, 3.0f)
        || !finiteUnit(kit.hatMetal)
        || !finiteInRange(kit.percussionTuneHz, 80.0f, 1200.0f)
        || !finiteInRange(kit.percussionDecaySeconds, 0.02f, 2.0f)
        || !finiteUnit(kit.drive) || !finiteUnit(kit.outputGain))
        return juce::Result::fail("Synthesized drum kit parameter is invalid");
    if (kit.openHatDecaySeconds <= kit.closedHatDecaySeconds)
        return juce::Result::fail("Open hat must decay longer than closed hat");
    return juce::Result::ok();
}
}
