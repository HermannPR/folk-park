#include "RhythmGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

namespace folkpark::drums
{
namespace
{
std::uint64_t splitMix(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

class Random final
{
public:
    explicit Random(std::uint64_t seed) : state(splitMix(seed)) {}

    std::uint64_t next() noexcept
    {
        state = splitMix(state);
        return state;
    }

    float unit() noexcept
    {
        return static_cast<float>((next() >> 40U) & 0x00ffffffULL)
            / static_cast<float>(0x01000000ULL);
    }

    int integer(int minimum, int maximumInclusive) noexcept
    {
        const auto range = static_cast<std::uint64_t>(maximumInclusive - minimum + 1);
        return minimum + static_cast<int>(next() % range);
    }

private:
    std::uint64_t state;
};

float baseProbability(RhythmGenre genre, DrumLane lane, int step) noexcept
{
    const auto position = step % 16;
    switch (genre)
    {
        case RhythmGenre::indieRock:
            switch (lane)
            {
                case DrumLane::kick: return position == 0 || position == 8 ? 0.98f
                    : (position == 6 || position == 10 || position == 14 ? 0.34f : 0.04f);
                case DrumLane::snare: return position == 4 || position == 12 ? 0.99f
                    : (position == 11 || position == 15 ? 0.18f : 0.02f);
                case DrumLane::closedHat: return position % 2 == 0 ? 0.94f : 0.12f;
                case DrumLane::openHat: return position == 14 ? 0.34f : 0.015f;
                case DrumLane::percussion: return position == 7 || position == 15 ? 0.18f : 0.015f;
                case DrumLane::count: break;
            }
            break;
        case RhythmGenre::eurodance:
            switch (lane)
            {
                case DrumLane::kick: return position % 4 == 0 ? 1.0f : 0.015f;
                case DrumLane::snare: return position == 4 || position == 12 ? 1.0f : 0.015f;
                case DrumLane::closedHat: return position % 2 == 0 ? 0.72f : 0.22f;
                case DrumLane::openHat: return position % 4 == 2 ? 0.9f : 0.015f;
                case DrumLane::percussion: return position == 3 || position == 11 ? 0.28f : 0.02f;
                case DrumLane::count: break;
            }
            break;
        case RhythmGenre::techno:
            switch (lane)
            {
                case DrumLane::kick: return position % 4 == 0 ? 1.0f : 0.025f;
                case DrumLane::snare: return position == 4 || position == 12 ? 0.78f : 0.035f;
                case DrumLane::closedHat: return position % 2 == 0 ? 0.68f : 0.46f;
                case DrumLane::openHat: return position % 4 == 2 ? 0.72f : 0.025f;
                case DrumLane::percussion: return position == 3 || position == 7
                    || position == 11 || position == 15 ? 0.32f : 0.06f;
                case DrumLane::count: break;
            }
            break;
        case RhythmGenre::funk:
            switch (lane)
            {
                case DrumLane::kick: return position == 0 ? 1.0f
                    : (position == 3 || position == 7 || position == 10 || position == 14 ? 0.58f : 0.08f);
                case DrumLane::snare: return position == 4 || position == 12 ? 0.96f
                    : (position == 6 || position == 11 || position == 15 ? 0.31f : 0.04f);
                case DrumLane::closedHat: return position % 2 == 0 ? 0.9f : 0.63f;
                case DrumLane::openHat: return position == 7 || position == 15 ? 0.38f : 0.025f;
                case DrumLane::percussion: return position == 2 || position == 9 || position == 13 ? 0.43f : 0.06f;
                case DrumLane::count: break;
            }
            break;
        case RhythmGenre::jazz:
            switch (lane)
            {
                case DrumLane::kick: return position == 0 ? 0.68f
                    : (position == 6 || position == 11 ? 0.22f : 0.04f);
                case DrumLane::snare: return position == 4 || position == 12 ? 0.54f
                    : (position % 2 == 1 ? 0.16f : 0.05f);
                case DrumLane::closedHat: return position % 4 == 0 || position % 4 == 3 ? 0.82f : 0.24f;
                case DrumLane::openHat: return position == 4 || position == 12 ? 0.46f : 0.02f;
                case DrumLane::percussion: return position == 5 || position == 10 || position == 15 ? 0.3f : 0.05f;
                case DrumLane::count: break;
            }
            break;
        case RhythmGenre::count: break;
    }
    return 0.0f;
}

int baseVelocity(RhythmGenre genre, DrumLane lane, int step) noexcept
{
    auto velocity = lane == DrumLane::kick ? 108 : lane == DrumLane::snare ? 101 : 86;
    if (step % 4 == 0)
        velocity += 8;
    if (genre == RhythmGenre::jazz)
        velocity -= 12;
    if (genre == RhythmGenre::eurodance && (lane == DrumLane::kick || lane == DrumLane::snare))
        velocity += 6;
    return velocity;
}
}

RhythmGenerationResult RhythmGenerator::generate(RhythmIntent intent,
                                                  std::int64_t createdUnixMs,
                                                  const juce::String& parentPatternId) const
{
    RhythmGenerationResult result;
    if (const auto validation = normaliseAndValidate(intent); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (parentPatternId.isNotEmpty() && !midi::isUuid(parentPatternId))
    {
        result.status = juce::Result::fail("Rhythm parent ID must be a UUID");
        return result;
    }

    auto& pattern = result.pattern;
    pattern.requestId = intent.requestId;
    pattern.id = midi::deterministicUuid(intent.seed,
        intent.requestId + ":rhythm:" + stableId(intent.genre) + ":" + parentPatternId);
    pattern.lengthTicks = midi::ticksPerBar(intent.timeSignature, rhythmPpq) * intent.lengthBars;
    pattern.tempoBpm = intent.tempoBpm;
    pattern.timeSignature = intent.timeSignature;
    pattern.seed = intent.seed;
    pattern.genre = intent.genre;
    pattern.parentPatternId = parentPatternId;
    pattern.createdUnixMs = createdUnixMs;

    const auto stepTicks = static_cast<std::int64_t>(rhythmPpq / 4);
    const auto stepCount = static_cast<int>((pattern.lengthTicks + stepTicks - 1) / stepTicks);
    Random random(static_cast<std::uint64_t>(intent.seed)
                  ^ (static_cast<std::uint64_t>(intent.genre) << 48U));
    pattern.events.reserve(std::min<std::size_t>(intent.maximumEvents,
                                                 static_cast<std::size_t>(stepCount) * intent.laneCount));

    for (int step = 0; step < stepCount && pattern.events.size() < intent.maximumEvents; ++step)
    {
        const auto nominalTick = static_cast<std::int64_t>(step) * stepTicks;
        const auto finalQuarter = nominalTick >= pattern.lengthTicks - rhythmPpq;
        for (std::size_t laneIndex = 0;
             laneIndex < intent.laneCount && pattern.events.size() < intent.maximumEvents;
             ++laneIndex)
        {
            const auto lane = intent.lanes[laneIndex];
            auto probability = baseProbability(intent.genre, lane, step);
            probability *= 0.5f + intent.density * 0.72f;
            if (finalQuarter && (lane == DrumLane::snare || lane == DrumLane::percussion))
                probability += intent.fillAmount * intent.complexity * 0.48f;
            if (lane == DrumLane::closedHat && step % 2 == 1)
                probability += intent.complexity * 0.18f;
            probability = juce::jlimit(0.0f, 1.0f, probability);
            if (random.unit() >= probability)
                continue;

            auto tick = nominalTick;
            if (step % 2 == 1)
                tick += static_cast<std::int64_t>(std::llround(
                    static_cast<double>(stepTicks) * intent.swing * 0.5));
            const auto humanRange = static_cast<int>(std::llround(
                static_cast<double>(stepTicks) * intent.humanization * 0.08));
            if (humanRange > 0)
                tick += random.integer(-humanRange, humanRange);
            tick = juce::jlimit<std::int64_t>(0, pattern.lengthTicks - 1, tick);

            auto velocity = baseVelocity(intent.genre, lane, step)
                + random.integer(-8, 8)
                + juce::roundToInt(intent.humanization * random.integer(-5, 5));
            velocity = juce::jlimit(1, 127, velocity);
            auto articulation = step % 4 == 0 ? DrumArticulation::accent
                                               : DrumArticulation::normal;
            if ((lane == DrumLane::snare || lane == DrumLane::percussion)
                && velocity < 82)
                articulation = DrumArticulation::ghost;
            if (finalQuarter && intent.fillAmount > 0.7f && random.unit() < 0.12f)
                articulation = DrumArticulation::flam;

            pattern.events.push_back({tick,
                std::min<std::int64_t>(stepTicks / 2, pattern.lengthTicks - tick),
                lane, velocity, 1.0f, articulation});
        }
    }

    std::stable_sort(pattern.events.begin(), pattern.events.end(),
        [](const DrumEvent& left, const DrumEvent& right)
        {
            return std::tie(left.startTick, left.lane, left.velocity)
                < std::tie(right.startTick, right.lane, right.velocity);
        });
    result.status = validateDrumPattern(pattern);
    return result;
}

RhythmGenerationResult RhythmGenerator::moreLikeThis(const DrumPattern& source,
                                                      const RhythmIntent& sourceIntent,
                                                      std::uint32_t variationIndex,
                                                      std::int64_t createdUnixMs) const
{
    RhythmGenerationResult result;
    if (const auto validation = validateDrumPattern(source); validation.failed())
    {
        result.status = validation;
        return result;
    }
    auto intent = sourceIntent;
    if (const auto validation = validateRhythmIntent(intent); validation.failed())
    {
        result.status = validation;
        return result;
    }
    intent.seed = static_cast<std::uint32_t>(splitMix(
        static_cast<std::uint64_t>(source.seed) ^ variationIndex ^ 0x52485954484dULL));
    intent.requestId = midi::deterministicUuid(intent.seed, "rhythm-variation");
    intent.complexity = juce::jlimit(0.0f, 1.0f,
        intent.complexity + 0.08f + static_cast<float>(variationIndex % 5U) * 0.025f);
    intent.fillAmount = juce::jlimit(0.0f, 1.0f,
        intent.fillAmount + 0.05f + static_cast<float>(variationIndex % 3U) * 0.035f);
    return generate(intent, createdUnixMs, source.id);
}
}
