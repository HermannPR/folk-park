#include "Composition.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace folkpark::midi
{
namespace
{
constexpr std::array<const char*, 4> partIds{"chords", "melody", "bass", "arp"};
constexpr std::array<const char*, 12> keyIds{"C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B"};
constexpr std::array<const char*, 7> scaleIds{"major", "natural_minor", "harmonic_minor",
                                              "dorian", "mixolydian", "pentatonic_major",
                                              "pentatonic_minor"};
constexpr std::array<const char*, 6> genreIds{"generic", "melodic_techno", "house", "ambient",
                                              "synthwave", "cinematic"};
constexpr std::array<const char*, 6> emotionIds{"neutral", "dark", "bright", "tense",
                                                "hopeful", "dreamy"};
constexpr std::array<const char*, 5> arpModeIds{"up", "down", "up_down", "random_seeded",
                                                "chord_order"};
constexpr std::array<const char*, 4> arpRateIds{"1/4", "1/8", "1/16", "1/32"};
constexpr std::array<const char*, 4> articulationIds{"normal", "staccato", "legato", "accent"};

constexpr std::array majorIntervals{0, 2, 4, 5, 7, 9, 11};
constexpr std::array naturalMinorIntervals{0, 2, 3, 5, 7, 8, 10};
constexpr std::array harmonicMinorIntervals{0, 2, 3, 5, 7, 8, 11};
constexpr std::array dorianIntervals{0, 2, 3, 5, 7, 9, 10};
constexpr std::array mixolydianIntervals{0, 2, 4, 5, 7, 9, 10};
constexpr std::array pentatonicMajorIntervals{0, 2, 4, 7, 9};
constexpr std::array pentatonicMinorIntervals{0, 3, 5, 7, 10};

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

std::uint64_t splitMix(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t hashString(const juce::String& text) noexcept
{
    auto hash = 1469598103934665603ULL;
    const auto utf8 = text.toUTF8();
    for (const auto* cursor = utf8.getAddress(); *cursor != '\0'; ++cursor)
    {
        hash ^= static_cast<std::uint8_t>(*cursor);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t deriveSeed(std::uint64_t seed, const juce::String& domain) noexcept
{
    return splitMix(seed ^ hashString(domain));
}

class DeterministicRandom final
{
public:
    explicit DeterministicRandom(std::uint64_t seed) : state(splitMix(seed)) {}

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
        if (maximumInclusive <= minimum)
            return minimum;
        const auto range = static_cast<std::uint64_t>(maximumInclusive - minimum + 1);
        return minimum + static_cast<int>(next() % range);
    }

private:
    std::uint64_t state;
};

struct HarmonicSegment
{
    std::int64_t startTick = 0;
    std::int64_t durationTicks = compositionPpq;
    int degree = 0;
    int rootPitchClass = 0;
    std::vector<int> voicedPitches;
    juce::String label;
};

using HarmonicPlan = std::vector<HarmonicSegment>;

std::span<const int> chordScaleIntervals(ScaleType scale) noexcept
{
    if (scale == ScaleType::pentatonicMajor)
        return majorIntervals;
    if (scale == ScaleType::pentatonicMinor)
        return naturalMinorIntervals;
    return scaleIntervals(scale);
}

int wrapPitchClass(int value) noexcept
{
    value %= 12;
    return value < 0 ? value + 12 : value;
}

int closestPitchForClass(int pitchClass, int target, int lowest, int highest) noexcept
{
    auto best = juce::jlimit(lowest, highest, target);
    auto bestDistance = std::numeric_limits<int>::max();
    for (int pitch = lowest; pitch <= highest; ++pitch)
    {
        if (wrapPitchClass(pitch) != wrapPitchClass(pitchClass))
            continue;
        const auto distance = std::abs(pitch - target);
        if (distance < bestDistance)
        {
            best = pitch;
            bestDistance = distance;
        }
    }
    return best;
}

std::vector<int> pitchClassesForChord(const MusicIntent& intent, int degree, bool seventh)
{
    const auto intervals = chordScaleIntervals(intent.scale);
    const auto count = seventh ? 4 : 3;
    std::vector<int> classes;
    classes.reserve(static_cast<std::size_t>(count));
    for (int tone = 0; tone < count; ++tone)
    {
        const auto scaleIndex = degree + tone * 2;
        const auto octave = scaleIndex / static_cast<int>(intervals.size());
        const auto interval = intervals[static_cast<std::size_t>(scaleIndex
            % static_cast<int>(intervals.size()))] + octave * 12;
        classes.push_back(static_cast<int>(intent.key) + interval);
    }
    return classes;
}

juce::String chordSymbol(const MusicIntent& intent, int degree, const std::vector<int>& classes)
{
    const auto rootClass = wrapPitchClass(classes.front());
    const auto third = wrapPitchClass(classes[1] - classes[0]);
    const auto fifth = wrapPitchClass(classes[2] - classes[0]);
    auto suffix = juce::String{};
    if (third == 3 && fifth == 7)
        suffix = "m";
    else if (third == 3 && fifth == 6)
        suffix = "dim";
    else if (third == 4 && fifth == 8)
        suffix = "aug";
    if (classes.size() == 4)
    {
        const auto seventh = wrapPitchClass(classes[3] - classes[0]);
        suffix += seventh == 11 ? "maj7" : "7";
    }
    juce::ignoreUnused(intent, degree);
    return keyIds[static_cast<std::size_t>(rootClass)] + suffix;
}

std::vector<int> voiceChord(const MusicIntent& intent,
                            const std::vector<int>& absoluteClasses,
                            const std::vector<int>& previous,
                            DeterministicRandom& random)
{
    auto tones = absoluteClasses;
    if (tones.size() > static_cast<std::size_t>(intent.constraints.maxPolyphony))
        tones.resize(static_cast<std::size_t>(intent.constraints.maxPolyphony));

    std::vector<int> best;
    auto bestCost = std::numeric_limits<double>::max();
    for (int baseOctave = -1; baseOctave <= 10; ++baseOctave)
    {
        for (std::size_t inversion = 0; inversion < tones.size(); ++inversion)
        {
            std::vector<int> candidate;
            candidate.reserve(tones.size());
            for (std::size_t tone = 0; tone < tones.size(); ++tone)
            {
                auto pitch = tones[tone] + baseOctave * 12;
                if (tone < inversion)
                    pitch += 12;
                candidate.push_back(pitch);
            }
            std::sort(candidate.begin(), candidate.end());
            if (candidate.front() < intent.constraints.lowestMidiNote
                || candidate.back() > intent.constraints.highestMidiNote)
                continue;

            auto cost = 0.0;
            if (previous.empty())
            {
                const auto centre = 0.5 * static_cast<double>(intent.constraints.lowestMidiNote
                                                              + intent.constraints.highestMidiNote);
                for (const auto pitch : candidate)
                    cost += std::abs(static_cast<double>(pitch) - centre) * 0.2;
                cost += random.unit() * 2.0;
            }
            else
            {
                const auto compared = std::min(previous.size(), candidate.size());
                for (std::size_t index = 0; index < compared; ++index)
                    cost += std::abs(candidate[index] - previous[index]);
                cost += std::abs(static_cast<int>(candidate.size()) - static_cast<int>(previous.size())) * 6.0;
                cost += static_cast<double>(candidate.back() - candidate.front()) * 0.05;
            }
            if (cost < bestCost)
            {
                best = std::move(candidate);
                bestCost = cost;
            }
        }
    }

    if (!best.empty())
        return best;
    for (const auto absolute : tones)
    {
        const auto pitch = closestPitchForClass(absolute, 60,
                                                intent.constraints.lowestMidiNote,
                                                intent.constraints.highestMidiNote);
        if (std::find(best.begin(), best.end(), pitch) == best.end())
            best.push_back(pitch);
    }
    std::sort(best.begin(), best.end());
    return best;
}

HarmonicPlan createHarmonicPlan(const MusicIntent& intent)
{
    const auto barTicks = ticksPerBar(intent.timeSignature, compositionPpq);
    const auto divisionsPerBar = intent.rhythmComplexity > 0.72f && intent.density > 0.5f ? 2 : 1;
    const auto segmentTicks = barTicks / divisionsPerBar;
    const auto segmentCount = intent.lengthBars * divisionsPerBar;
    DeterministicRandom random(deriveSeed(intent.seed, "harmonic-plan"));
    constexpr std::array<std::array<int, 4>, 7> transitions{{
        {{3, 4, 5, 1}}, {{4, 3, 6, 4}}, {{5, 3, 1, 5}}, {{0, 4, 1, 0}},
        {{0, 5, 0, 3}}, {{1, 3, 4, 1}}, {{0, 0, 4, 0}}
    }};

    std::vector<int> degrees(static_cast<std::size_t>(segmentCount), 0);
    if (!degrees.empty())
        degrees[0] = random.unit() < 0.25f * intent.tension ? 5 : 0;
    for (int index = 1; index < segmentCount; ++index)
    {
        const auto previous = juce::jlimit(0, 6, degrees[static_cast<std::size_t>(index - 1)]);
        degrees[static_cast<std::size_t>(index)]
            = transitions[static_cast<std::size_t>(previous)][static_cast<std::size_t>(random.integer(0, 3))];
    }
    if (segmentCount >= 2)
    {
        degrees[degrees.size() - 2] = 4;
        degrees.back() = 0;
    }

    HarmonicPlan plan;
    plan.reserve(static_cast<std::size_t>(segmentCount));
    std::vector<int> previousVoicing;
    for (int index = 0; index < segmentCount; ++index)
    {
        const auto degree = degrees[static_cast<std::size_t>(index)];
        const auto seventhProbability = 0.1f + 0.75f * intent.tension;
        const auto seventh = intent.constraints.maxPolyphony >= 4
            && random.unit() < seventhProbability;
        auto classes = pitchClassesForChord(intent, degree, seventh);
        auto voiced = voiceChord(intent, classes, previousVoicing, random);
        HarmonicSegment segment;
        segment.startTick = static_cast<std::int64_t>(index) * segmentTicks;
        segment.durationTicks = segmentTicks;
        segment.degree = degree;
        segment.rootPitchClass = wrapPitchClass(classes.front());
        segment.label = chordSymbol(intent, degree, classes);
        segment.voicedPitches = std::move(voiced);
        previousVoicing = segment.voicedPitches;
        plan.push_back(std::move(segment));
    }
    return plan;
}

const HarmonicSegment& segmentAt(const HarmonicPlan& plan, std::int64_t tick)
{
    const auto found = std::upper_bound(plan.begin(), plan.end(), tick,
        [](std::int64_t value, const HarmonicSegment& segment)
        {
            return value < segment.startTick;
        });
    if (found == plan.begin())
        return plan.front();
    return *std::prev(found);
}

GeneratedClip baseClip(const MusicIntent& intent,
                       PartType part,
                       std::int64_t createdUnixMs,
                       const juce::String& parentId)
{
    GeneratedClip clip;
    clip.id = deterministicUuid(intent.seed,
        intent.requestId + ":" + stableId(part) + ":" + compositionGeneratorVersion + ":" + parentId);
    clip.part = part;
    clip.ppq = compositionPpq;
    clip.lengthTicks = ticksPerBar(intent.timeSignature, compositionPpq) * intent.lengthBars;
    clip.tempoBpm = intent.tempoBpm;
    clip.timeSignature = intent.timeSignature;
    clip.key = intent.key;
    clip.scale = intent.scale;
    clip.seed = intent.seed;
    clip.parentClipId = parentId;
    clip.createdUnixMs = createdUnixMs;
    return clip;
}

void copyChordLabels(const HarmonicPlan& plan, GeneratedClip& clip)
{
    clip.chordLabels.reserve(plan.size());
    for (const auto& segment : plan)
        clip.chordLabels.push_back({segment.startTick, segment.durationTicks,
                                    segment.label, segment.degree + 1});
}

bool addEvent(GeneratedClip& clip, const NoteEvent& event, std::size_t budget)
{
    if (clip.events.size() >= budget)
        return false;
    clip.events.push_back(event);
    return true;
}

int baseVelocity(const MusicIntent& intent, PartType part) noexcept
{
    auto velocity = 76 + juce::roundToInt(intent.tension * 18.0f + intent.density * 12.0f);
    if (intent.emotion == Emotion::bright || intent.emotion == Emotion::hopeful)
        velocity += 5;
    if (part == PartType::bass)
        velocity += 4;
    return juce::jlimit(1, 127, velocity);
}

GeneratedClip generateChords(const MusicIntent& intent,
                             const HarmonicPlan& plan,
                             std::size_t budget,
                             std::int64_t createdUnixMs,
                             const juce::String& parentId)
{
    auto clip = baseClip(intent, PartType::chords, createdUnixMs, parentId);
    copyChordLabels(plan, clip);
    const auto velocity = baseVelocity(intent, PartType::chords);
    for (const auto& segment : plan)
    {
        for (const auto pitch : segment.voicedPitches)
        {
            if (!addEvent(clip, {segment.startTick, segment.durationTicks, pitch, velocity, 1, 1.0f,
                                 Articulation::legato}, budget))
                return clip;
        }
    }
    return clip;
}

int scalePitchNear(const MusicIntent& intent, int scaleDegree, int target)
{
    const auto intervals = scaleIntervals(intent.scale);
    const auto safeDegree = ((scaleDegree % static_cast<int>(intervals.size()))
                             + static_cast<int>(intervals.size())) % static_cast<int>(intervals.size());
    const auto pitchClass = static_cast<int>(intent.key) + intervals[static_cast<std::size_t>(safeDegree)];
    return closestPitchForClass(pitchClass, target, intent.constraints.lowestMidiNote,
                                intent.constraints.highestMidiNote);
}

GeneratedClip generateMelody(const MusicIntent& intent,
                             const HarmonicPlan& plan,
                             std::size_t budget,
                             std::int64_t createdUnixMs,
                             const juce::String& parentId)
{
    auto clip = baseClip(intent, PartType::melody, createdUnixMs, parentId);
    copyChordLabels(plan, clip);
    DeterministicRandom random(deriveSeed(intent.seed, "melody"));
    const auto grid = compositionPpq / 4;
    const auto totalSteps = static_cast<int>(clip.lengthTicks / grid);
    const auto intervals = scaleIntervals(intent.scale);
    std::array<int, 8> motif{};
    motif.fill(-1);
    auto previousPitch = juce::jlimit(intent.constraints.lowestMidiNote,
                                      intent.constraints.highestMidiNote, 64);
    const auto playableLow = juce::jlimit(intent.constraints.lowestMidiNote,
        intent.constraints.highestMidiNote, std::max(intent.constraints.lowestMidiNote, 52));
    const auto playableHigh = std::max(playableLow,
        juce::jlimit(intent.constraints.lowestMidiNote, intent.constraints.highestMidiNote,
                    std::min(intent.constraints.highestMidiNote, 88)));

    for (int step = 0; step < totalSteps && clip.events.size() < budget; ++step)
    {
        const auto phraseStep = step % 32;
        const auto contour = phraseStep < 16 ? static_cast<float>(phraseStep) / 15.0f
                                             : static_cast<float>(31 - phraseStep) / 15.0f;
        const auto target = juce::roundToInt(static_cast<float>(playableLow)
            + (0.2f + 0.65f * contour) * static_cast<float>(playableHigh - playableLow));
        const auto onsetProbability = 0.12f + 0.78f * intent.density;
        if (random.unit() > onsetProbability)
            continue;

        const auto motifIndex = static_cast<std::size_t>(step % static_cast<int>(motif.size()));
        auto degree = random.integer(0, static_cast<int>(intervals.size()) - 1);
        if (motif[motifIndex] >= 0 && random.unit() < intent.repetition)
        {
            degree = motif[motifIndex];
            if (random.unit() < intent.variation * 0.35f)
                degree += random.integer(-1, 1);
        }
        else
        {
            motif[motifIndex] = degree;
        }

        const auto tick = static_cast<std::int64_t>(step) * grid;
        const auto& harmony = segmentAt(plan, tick);
        auto pitch = scalePitchNear(intent, degree, target);
        const auto strongBeat = tick % compositionPpq == 0;
        if (strongBeat && !harmony.voicedPitches.empty() && random.unit() < 0.75f)
        {
            pitch = *std::min_element(harmony.voicedPitches.begin(), harmony.voicedPitches.end(),
                [target](int left, int right)
                {
                    return std::abs(left - target) < std::abs(right - target);
                });
            while (pitch < playableLow && pitch + 12 <= playableHigh)
                pitch += 12;
            while (pitch > playableHigh && pitch - 12 >= playableLow)
                pitch -= 12;
        }
        while (pitch - previousPitch > 12 && pitch - 12 >= playableLow)
            pitch -= 12;
        while (previousPitch - pitch > 12 && pitch + 12 <= playableHigh)
            pitch += 12;
        pitch = juce::jlimit(playableLow, playableHigh, pitch);

        const auto maximumDurationSteps = intent.rhythmComplexity < 0.35f ? 4 : 2;
        const auto durationSteps = random.integer(1, maximumDurationSteps);
        const auto duration = std::min<std::int64_t>(durationSteps * grid,
                                                     clip.lengthTicks - tick);
        const auto velocity = juce::jlimit(1, 127, baseVelocity(intent, PartType::melody)
            + random.integer(-8, 8));
        addEvent(clip, {tick, std::max<std::int64_t>(1, duration), pitch, velocity, 1, 1.0f,
                        durationSteps == 1 ? Articulation::staccato : Articulation::normal}, budget);
        previousPitch = pitch;
        step += durationSteps - 1;
    }
    return clip;
}

GeneratedClip generateBass(const MusicIntent& intent,
                           const HarmonicPlan& plan,
                           std::size_t budget,
                           std::int64_t createdUnixMs,
                           const juce::String& parentId)
{
    auto clip = baseClip(intent, PartType::bass, createdUnixMs, parentId);
    copyChordLabels(plan, clip);
    DeterministicRandom random(deriveSeed(intent.seed, "bass"));
    const auto stepTicks = intent.rhythmComplexity > 0.58f ? compositionPpq / 2 : compositionPpq;
    const auto bassLow = intent.constraints.lowestMidiNote;
    const auto bassHigh = std::max(bassLow,
        std::min(intent.constraints.highestMidiNote, std::max(bassLow, 60)));
    for (std::int64_t tick = 0; tick < clip.lengthTicks && clip.events.size() < budget;
         tick += stepTicks)
    {
        const auto& harmony = segmentAt(plan, tick);
        const auto atHarmonyStart = tick == harmony.startTick;
        if (!atHarmonyStart && random.unit() > 0.35f + 0.6f * intent.density)
            continue;
        auto pitchClass = harmony.rootPitchClass;
        const auto segmentEnd = harmony.startTick + harmony.durationTicks;
        if (tick + stepTicks >= segmentEnd && segmentEnd < clip.lengthTicks
            && random.unit() < intent.tension * 0.7f)
        {
            const auto& next = segmentAt(plan, segmentEnd);
            pitchClass = wrapPitchClass(next.rootPitchClass + (random.unit() < 0.5f ? -1 : 1));
        }
        auto pitch = closestPitchForClass(pitchClass, 43, bassLow, bassHigh);
        if (random.unit() < 0.2f + 0.35f * intent.variation && pitch + 12 <= bassHigh)
            pitch += 12;
        auto start = tick;
        if (intent.genreProfile == GenreProfile::melodicTechno && tick % compositionPpq == 0
            && intent.rhythmComplexity > 0.45f)
            start = std::min<std::int64_t>(clip.lengthTicks - 1, tick + compositionPpq / 16);
        const auto duration = std::max<std::int64_t>(1,
            std::min<std::int64_t>(juce::roundToInt(stepTicks * 0.72), clip.lengthTicks - start));
        addEvent(clip, {start, duration, pitch,
                        juce::jlimit(1, 127, baseVelocity(intent, PartType::bass)
                            + random.integer(-5, 5)),
                        1, 1.0f, Articulation::staccato}, budget);
    }
    return clip;
}

int arpRateTicks(ArpRateDivision division) noexcept
{
    switch (division)
    {
        case ArpRateDivision::quarter: return compositionPpq;
        case ArpRateDivision::eighth: return compositionPpq / 2;
        case ArpRateDivision::sixteenth: return compositionPpq / 4;
        case ArpRateDivision::thirtySecond: return compositionPpq / 8;
        case ArpRateDivision::count: break;
    }
    return compositionPpq / 4;
}

GeneratedClip generateArp(const MusicIntent& intent,
                          const HarmonicPlan& plan,
                          std::size_t budget,
                          std::int64_t createdUnixMs,
                          const juce::String& parentId)
{
    auto clip = baseClip(intent, PartType::arp, createdUnixMs, parentId);
    copyChordLabels(plan, clip);
    DeterministicRandom random(deriveSeed(intent.seed, "arp"));
    const auto rate = arpRateTicks(intent.arp.rateDivision);
    const auto duration = std::max<std::int64_t>(1, juce::roundToInt(rate * intent.arp.gate));
    for (const auto& harmony : plan)
    {
        std::vector<int> pool;
        for (int octave = 0; octave < intent.arp.octaveSpan; ++octave)
        {
            for (const auto pitch : harmony.voicedPitches)
            {
                const auto expanded = pitch + octave * 12;
                if (expanded >= intent.constraints.lowestMidiNote
                    && expanded <= intent.constraints.highestMidiNote)
                    pool.push_back(expanded);
            }
        }
        if (pool.empty())
            continue;
        std::sort(pool.begin(), pool.end());
        const auto steps = static_cast<int>(harmony.durationTicks / rate);
        for (int step = 0; step < steps && clip.events.size() < budget; ++step)
        {
            auto index = step % static_cast<int>(pool.size());
            switch (intent.arp.mode)
            {
                case ArpMode::up:
                case ArpMode::chordOrder:
                    break;
                case ArpMode::down:
                    index = static_cast<int>(pool.size()) - 1 - index;
                    break;
                case ArpMode::upDown:
                {
                    const auto cycle = pool.size() <= 1 ? 1 : static_cast<int>(pool.size() * 2 - 2);
                    const auto position = step % cycle;
                    index = position < static_cast<int>(pool.size())
                        ? position : cycle - position;
                    break;
                }
                case ArpMode::randomSeeded:
                    index = random.integer(0, static_cast<int>(pool.size()) - 1);
                    break;
                case ArpMode::count:
                    break;
            }
            const auto start = harmony.startTick + static_cast<std::int64_t>(step) * rate;
            addEvent(clip, {start, std::min<std::int64_t>(duration, clip.lengthTicks - start),
                            pool[static_cast<std::size_t>(index)],
                            juce::jlimit(1, 127, baseVelocity(intent, PartType::arp)
                                + random.integer(-6, 6)),
                            1, 1.0f, intent.arp.gate < 0.6f
                                ? Articulation::staccato : Articulation::normal}, budget);
        }
    }
    return clip;
}

void sortEvents(std::vector<NoteEvent>& events)
{
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent& left, const NoteEvent& right)
    {
        return std::tie(left.startTick, left.pitch, left.channel, left.durationTicks)
            < std::tie(right.startTick, right.pitch, right.channel, right.durationTicks);
    });
}

void humanizeClip(const MusicIntent& intent, GeneratedClip& clip)
{
    if (clip.events.empty() || intent.humanization <= 0.0f)
    {
        sortEvents(clip.events);
        return;
    }
    DeterministicRandom random(deriveSeed(intent.seed, "humanize:" + stableId(clip.part)));
    const auto maximumTiming = juce::roundToInt(intent.humanization * compositionPpq * 0.05f);
    const auto maximumVelocity = juce::roundToInt(intent.humanization * 14.0f);
    std::size_t groupStart = 0;
    while (groupStart < clip.events.size())
    {
        const auto originalTick = clip.events[groupStart].startTick;
        auto groupEnd = groupStart + 1;
        while (groupEnd < clip.events.size() && clip.events[groupEnd].startTick == originalTick)
            ++groupEnd;
        const auto jitter = maximumTiming == 0 ? 0 : random.integer(-maximumTiming, maximumTiming);
        const auto shifted = juce::jlimit<std::int64_t>(0, clip.lengthTicks - 1, originalTick + jitter);
        for (auto index = groupStart; index < groupEnd; ++index)
        {
            auto& event = clip.events[index];
            event.startTick = shifted;
            event.durationTicks = std::min(event.durationTicks, clip.lengthTicks - shifted);
            event.durationTicks = std::max<std::int64_t>(1, event.durationTicks);
            event.velocity = juce::jlimit(1, 127, event.velocity
                + (maximumVelocity == 0 ? 0 : random.integer(-maximumVelocity, maximumVelocity)));
        }
        groupStart = groupEnd;
    }
    sortEvents(clip.events);

    if (clip.part == PartType::chords)
    {
        auto groupBegin = std::size_t{0};
        while (groupBegin < clip.events.size())
        {
            auto groupEnd = groupBegin + 1;
            while (groupEnd < clip.events.size()
                   && clip.events[groupEnd].startTick == clip.events[groupBegin].startTick)
                ++groupEnd;
            const auto nextStart = groupEnd < clip.events.size()
                ? clip.events[groupEnd].startTick : clip.lengthTicks;
            for (auto index = groupBegin; index < groupEnd; ++index)
                clip.events[index].durationTicks = std::max<std::int64_t>(1,
                    std::min(clip.events[index].durationTicks,
                             nextStart - clip.events[index].startTick));
            groupBegin = groupEnd;
        }
        return;
    }
    for (std::size_t index = 1; index < clip.events.size(); ++index)
    {
        auto& previous = clip.events[index - 1];
        auto& current = clip.events[index];
        if (current.startTick <= previous.startTick)
            current.startTick = std::min<std::int64_t>(clip.lengthTicks - 1, previous.startTick + 1);
        previous.durationTicks = std::max<std::int64_t>(1,
            std::min(previous.durationTicks, current.startTick - previous.startTick));
        current.durationTicks = std::max<std::int64_t>(1,
            std::min(current.durationTicks, clip.lengthTicks - current.startTick));
    }
}

bool eventsEqual(const GeneratedClip& first, const GeneratedClip& second)
{
    if (first.events.size() != second.events.size())
        return false;
    for (std::size_t index = 0; index < first.events.size(); ++index)
    {
        const auto& left = first.events[index];
        const auto& right = second.events[index];
        if (left.startTick != right.startTick || left.durationTicks != right.durationTicks
            || left.pitch != right.pitch || left.velocity != right.velocity
            || left.channel != right.channel
            || std::abs(left.probability - right.probability) > 1.0e-7f
            || left.articulation != right.articulation)
            return false;
    }
    return true;
}

const GeneratedClip* parentFor(PartType part, std::span<const GeneratedClip> parents)
{
    const auto found = std::find_if(parents.begin(), parents.end(), [part](const GeneratedClip& clip)
    {
        return clip.part == part;
    });
    return found == parents.end() ? nullptr : &*found;
}
}

juce::String stableId(PartType value) { return enumId(value, PartType::count, partIds); }
juce::String stableId(KeyRoot value) { return enumId(value, KeyRoot::count, keyIds); }
juce::String stableId(ScaleType value) { return enumId(value, ScaleType::count, scaleIds); }
juce::String stableId(GenreProfile value) { return enumId(value, GenreProfile::count, genreIds); }
juce::String stableId(Emotion value) { return enumId(value, Emotion::count, emotionIds); }
juce::String stableId(ArpMode value) { return enumId(value, ArpMode::count, arpModeIds); }
juce::String stableId(ArpRateDivision value) { return enumId(value, ArpRateDivision::count, arpRateIds); }
juce::String stableId(Articulation value) { return enumId(value, Articulation::count, articulationIds); }

std::optional<KeyRoot> parseKeyRoot(const juce::String& text)
{
    const auto candidate = text.trim().toUpperCase().replace("DB", "C#").replace("EB", "D#")
        .replace("GB", "F#").replace("AB", "G#").replace("BB", "A#");
    for (std::size_t index = 0; index < keyIds.size(); ++index)
        if (candidate == keyIds[index])
            return static_cast<KeyRoot>(index);
    return std::nullopt;
}

std::optional<ScaleType> parseScaleType(const juce::String& text)
{
    const auto candidate = text.trim().toLowerCase().replaceCharacter(' ', '_').replaceCharacter('-', '_');
    for (std::size_t index = 0; index < scaleIds.size(); ++index)
        if (candidate == scaleIds[index])
            return static_cast<ScaleType>(index);
    if (candidate == "minor")
        return ScaleType::naturalMinor;
    return std::nullopt;
}

std::span<const int> scaleIntervals(ScaleType scale) noexcept
{
    switch (scale)
    {
        case ScaleType::major: return majorIntervals;
        case ScaleType::naturalMinor: return naturalMinorIntervals;
        case ScaleType::harmonicMinor: return harmonicMinorIntervals;
        case ScaleType::dorian: return dorianIntervals;
        case ScaleType::mixolydian: return mixolydianIntervals;
        case ScaleType::pentatonicMajor: return pentatonicMajorIntervals;
        case ScaleType::pentatonicMinor: return pentatonicMinorIntervals;
        case ScaleType::count: break;
    }
    return {};
}

bool isUuid(const juce::String& text) noexcept
{
    if (text.length() != 36)
        return false;
    for (int index = 0; index < text.length(); ++index)
    {
        const auto character = text[index];
        const auto hyphen = index == 8 || index == 13 || index == 18 || index == 23;
        const auto lower = juce::CharacterFunctions::toLowerCase(character);
        const auto hexadecimal = juce::CharacterFunctions::isDigit(character)
            || (lower >= 'a' && lower <= 'f');
        if (hyphen ? character != '-' : !hexadecimal)
            return false;
    }
    return true;
}

juce::String deterministicUuid(std::uint64_t seed, const juce::String& domain)
{
    const auto first = splitMix(seed ^ hashString(domain));
    const auto second = splitMix(first ^ 0x6a09e667f3bcc909ULL);
    auto hex = juce::String::toHexString(static_cast<juce::int64>(first)).paddedLeft('0', 16)
        + juce::String::toHexString(static_cast<juce::int64>(second)).paddedLeft('0', 16);
    hex = hex.substring(0, 32).toLowerCase();
    return hex.substring(0, 8) + "-" + hex.substring(8, 12) + "-" + hex.substring(12, 16)
        + "-" + hex.substring(16, 20) + "-" + hex.substring(20, 32);
}

std::int64_t ticksPerBar(const TimeSignature& signature, int ppq) noexcept
{
    if (signature.denominator <= 0 || ppq <= 0)
        return 0;
    return static_cast<std::int64_t>(ppq) * 4 * signature.numerator / signature.denominator;
}

juce::Result normaliseAndValidate(MusicIntent& intent)
{
    if (!validEnum(intent.key, KeyRoot::count) || !validEnum(intent.scale, ScaleType::count)
        || !validEnum(intent.genreProfile, GenreProfile::count)
        || !validEnum(intent.emotion, Emotion::count)
        || !validEnum(intent.arp.mode, ArpMode::count)
        || !validEnum(intent.arp.rateDivision, ArpRateDivision::count))
        return juce::Result::fail("MusicIntent contains an unsupported enum value");

    intent.tempoBpm = juce::jlimit(20.0, 400.0, intent.tempoBpm);
    intent.lengthBars = juce::jlimit(1, 64, intent.lengthBars);
    intent.density = juce::jlimit(0.0f, 1.0f, intent.density);
    intent.rhythmComplexity = juce::jlimit(0.0f, 1.0f, intent.rhythmComplexity);
    intent.tension = juce::jlimit(0.0f, 1.0f, intent.tension);
    intent.humanization = juce::jlimit(0.0f, 1.0f, intent.humanization);
    intent.repetition = juce::jlimit(0.0f, 1.0f, intent.repetition);
    intent.variation = juce::jlimit(0.0f, 1.0f, intent.variation);
    intent.arp.gate = juce::jlimit(0.05f, 1.0f, intent.arp.gate);
    intent.arp.octaveSpan = juce::jlimit(1, 4, intent.arp.octaveSpan);
    intent.constraints.lowestMidiNote = juce::jlimit(0, 127, intent.constraints.lowestMidiNote);
    intent.constraints.highestMidiNote = juce::jlimit(0, 127, intent.constraints.highestMidiNote);
    intent.constraints.maxPolyphony = juce::jlimit(1, 16, intent.constraints.maxPolyphony);
    intent.constraints.maximumEvents = juce::jlimit(1, static_cast<int>(maximumGeneratedEvents),
                                                     intent.constraints.maximumEvents);
    return validateMusicIntent(intent);
}

juce::Result validateMusicIntent(const MusicIntent& intent)
{
    if (intent.schemaVersion != MusicIntent::currentSchemaVersion)
        return juce::Result::fail("MusicIntent schema version is unsupported");
    if (!isUuid(intent.requestId))
        return juce::Result::fail("MusicIntent requestId must be a bounded UUID");
    if (!validEnum(intent.key, KeyRoot::count) || !validEnum(intent.scale, ScaleType::count)
        || !validEnum(intent.genreProfile, GenreProfile::count)
        || !validEnum(intent.emotion, Emotion::count)
        || !validEnum(intent.arp.mode, ArpMode::count)
        || !validEnum(intent.arp.rateDivision, ArpRateDivision::count))
        return juce::Result::fail("MusicIntent contains an unsupported enum value");
    if (!std::isfinite(intent.tempoBpm) || intent.tempoBpm < 20.0 || intent.tempoBpm > 400.0)
        return juce::Result::fail("MusicIntent tempo is outside 20-400 BPM");
    if (intent.timeSignature.numerator < 2 || intent.timeSignature.numerator > 12
        || (intent.timeSignature.denominator != 2 && intent.timeSignature.denominator != 4
            && intent.timeSignature.denominator != 8 && intent.timeSignature.denominator != 16))
        return juce::Result::fail("MusicIntent time signature is unsupported");
    if (intent.lengthBars < 1 || intent.lengthBars > 64)
        return juce::Result::fail("MusicIntent length must be 1-64 bars");
    if (intent.partCount < 1 || intent.partCount > intent.parts.size())
        return juce::Result::fail("MusicIntent must request 1-4 parts");
    std::array<bool, static_cast<std::size_t>(PartType::count)> seen{};
    auto needsHarmony = false;
    for (std::size_t index = 0; index < intent.partCount; ++index)
    {
        const auto part = intent.parts[index];
        if (!validEnum(part, PartType::count))
            return juce::Result::fail("MusicIntent contains an unsupported part");
        const auto raw = static_cast<std::size_t>(part);
        if (seen[raw])
            return juce::Result::fail("MusicIntent parts must be unique");
        seen[raw] = true;
        needsHarmony = needsHarmony || part == PartType::chords || part == PartType::arp;
    }
    const std::array macros{intent.density, intent.rhythmComplexity, intent.tension,
                            intent.humanization, intent.repetition, intent.variation};
    for (const auto value : macros)
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            return juce::Result::fail("MusicIntent macro values must be finite and normalized");
    if (!std::isfinite(intent.arp.gate) || intent.arp.gate < 0.05f || intent.arp.gate > 1.0f
        || intent.arp.octaveSpan < 1 || intent.arp.octaveSpan > 4)
        return juce::Result::fail("MusicIntent arpeggiator settings are outside bounds");
    if (intent.constraints.lowestMidiNote < 0 || intent.constraints.highestMidiNote > 127
        || intent.constraints.lowestMidiNote > intent.constraints.highestMidiNote)
        return juce::Result::fail("MusicIntent note range is invalid");
    if (intent.constraints.maxPolyphony < 1 || intent.constraints.maxPolyphony > 16)
        return juce::Result::fail("MusicIntent polyphony is outside 1-16");
    if (needsHarmony && (intent.constraints.maxPolyphony < 3
        || intent.constraints.highestMidiNote - intent.constraints.lowestMidiNote < 7))
        return juce::Result::fail("Chord/arp generation requires polyphony 3 and at least seven semitones");
    if (intent.constraints.maximumEvents < static_cast<int>(intent.partCount)
        || intent.constraints.maximumEvents > static_cast<int>(maximumGeneratedEvents))
        return juce::Result::fail("MusicIntent event bound cannot cover every requested part");
    if (ticksPerBar(intent.timeSignature, compositionPpq) <= 0)
        return juce::Result::fail("MusicIntent produces an invalid bar length");
    return juce::Result::ok();
}

juce::Result validateGeneratedClip(const GeneratedClip& clip)
{
    if (clip.schemaVersion != GeneratedClip::currentSchemaVersion)
        return juce::Result::fail("GeneratedClip schema version is unsupported");
    if (!isUuid(clip.id) || (!clip.parentClipId.isEmpty() && !isUuid(clip.parentClipId)))
        return juce::Result::fail("GeneratedClip IDs must be UUIDs");
    if (!validEnum(clip.part, PartType::count) || !validEnum(clip.key, KeyRoot::count)
        || !validEnum(clip.scale, ScaleType::count))
        return juce::Result::fail("GeneratedClip contains an unsupported enum value");
    if (clip.ppq < 24 || clip.ppq > 9600 || clip.lengthTicks <= 0 || clip.lengthTicks > 983040)
        return juce::Result::fail("GeneratedClip timing metadata is outside bounds");
    if (!std::isfinite(clip.tempoBpm) || clip.tempoBpm < 20.0 || clip.tempoBpm > 400.0)
        return juce::Result::fail("GeneratedClip tempo is outside bounds");
    if (clip.generatorVersion.isEmpty() || clip.generatorVersion.length() > 32
        || clip.createdUnixMs < 0 || clip.events.size() > maximumGeneratedEvents
        || clip.chordLabels.size() > 512)
        return juce::Result::fail("GeneratedClip metadata or collection size is invalid");

    std::int64_t previousStart = -1;
    int previousPitch = -1;
    for (const auto& event : clip.events)
    {
        if (event.startTick < 0 || event.durationTicks <= 0
            || event.startTick + event.durationTicks > clip.lengthTicks
            || event.pitch < 0 || event.pitch > 127 || event.velocity < 1 || event.velocity > 127
            || event.channel < 1 || event.channel > 16 || !std::isfinite(event.probability)
            || event.probability < 0.0f || event.probability > 1.0f
            || !validEnum(event.articulation, Articulation::count))
            return juce::Result::fail("GeneratedClip contains an invalid note event");
        if (event.startTick < previousStart
            || (event.startTick == previousStart && event.pitch < previousPitch))
            return juce::Result::fail("GeneratedClip note events are not canonically sorted");
        previousStart = event.startTick;
        previousPitch = event.pitch;
    }
    if (clip.part != PartType::chords)
    {
        for (std::size_t index = 1; index < clip.events.size(); ++index)
            if (clip.events[index - 1].startTick + clip.events[index - 1].durationTicks
                > clip.events[index].startTick)
                return juce::Result::fail("Monophonic GeneratedClip events overlap");
    }
    previousStart = -1;
    for (const auto& label : clip.chordLabels)
    {
        if (label.startTick < 0 || label.durationTicks <= 0
            || label.startTick + label.durationTicks > clip.lengthTicks
            || label.startTick < previousStart || label.symbol.isEmpty() || label.symbol.length() > 24
            || label.scaleDegree < 1 || label.scaleDegree > 7)
            return juce::Result::fail("GeneratedClip contains an invalid chord label");
        previousStart = label.startTick;
    }
    return juce::Result::ok();
}

juce::Result validateBundle(const CompositionBundle& bundle)
{
    if (const auto intentResult = validateMusicIntent(bundle.intent); intentResult.failed())
        return intentResult;
    if (bundle.clips.size() != bundle.intent.partCount)
        return juce::Result::fail("Composition bundle must contain exactly one clip per requested part");
    std::array<bool, static_cast<std::size_t>(PartType::count)> seen{};
    std::size_t eventCount = 0;
    for (const auto& clip : bundle.clips)
    {
        if (const auto clipResult = validateGeneratedClip(clip); clipResult.failed())
            return clipResult;
        const auto part = static_cast<std::size_t>(clip.part);
        if (seen[part])
            return juce::Result::fail("Composition bundle contains a duplicate part");
        seen[part] = true;
        if (clip.ppq != compositionPpq || clip.seed != bundle.intent.seed
            || clip.key != bundle.intent.key || clip.scale != bundle.intent.scale
            || clip.timeSignature.numerator != bundle.intent.timeSignature.numerator
            || clip.timeSignature.denominator != bundle.intent.timeSignature.denominator)
            return juce::Result::fail("Composition bundle clip context differs from MusicIntent");
        eventCount += clip.events.size();
        for (const auto& event : clip.events)
            if (event.pitch < bundle.intent.constraints.lowestMidiNote
                || event.pitch > bundle.intent.constraints.highestMidiNote)
                return juce::Result::fail("Composition bundle violates its requested note range");

        std::vector<std::int64_t> activeEnds;
        for (const auto& event : clip.events)
        {
            activeEnds.erase(std::remove_if(activeEnds.begin(), activeEnds.end(),
                [&event](std::int64_t end) { return end <= event.startTick; }), activeEnds.end());
            activeEnds.push_back(event.startTick + event.durationTicks);
            if (activeEnds.size() > static_cast<std::size_t>(bundle.intent.constraints.maxPolyphony))
                return juce::Result::fail("Composition bundle violates its requested polyphony");
        }
    }
    if (eventCount > static_cast<std::size_t>(bundle.intent.constraints.maximumEvents))
        return juce::Result::fail("Composition bundle exceeds its requested event bound");
    for (std::size_t index = 0; index < bundle.intent.partCount; ++index)
        if (!seen[static_cast<std::size_t>(bundle.intent.parts[index])])
            return juce::Result::fail("Composition bundle is missing a requested part");
    return juce::Result::ok();
}

PianoRollPreview createPianoRollPreview(const CompositionBundle& bundle)
{
    PianoRollPreview preview;
    preview.lowestPitch = bundle.intent.constraints.lowestMidiNote;
    preview.highestPitch = bundle.intent.constraints.highestMidiNote;
    for (const auto& clip : bundle.clips)
    {
        preview.lengthTicks = std::max(preview.lengthTicks, clip.lengthTicks);
        preview.sourceEventCount += clip.events.size();
    }
    preview.truncated = preview.sourceEventCount > maximumPreviewNotes;
    const auto stride = preview.sourceEventCount <= maximumPreviewNotes ? std::size_t{1}
        : static_cast<std::size_t>(std::ceil(static_cast<double>(preview.sourceEventCount)
                                             / static_cast<double>(maximumPreviewNotes)));
    preview.notes.reserve(std::min(preview.sourceEventCount, maximumPreviewNotes));
    auto sourceIndex = std::size_t{0};
    const auto pitchRange = std::max(1, preview.highestPitch - preview.lowestPitch);
    for (const auto& clip : bundle.clips)
    {
        for (const auto& event : clip.events)
        {
            if (sourceIndex++ % stride != 0 || preview.notes.size() >= maximumPreviewNotes)
                continue;
            preview.notes.push_back({clip.part,
                static_cast<float>(event.startTick) / static_cast<float>(std::max<std::int64_t>(1, preview.lengthTicks)),
                static_cast<float>(event.durationTicks) / static_cast<float>(std::max<std::int64_t>(1, preview.lengthTicks)),
                static_cast<float>(event.pitch - preview.lowestPitch) / static_cast<float>(pitchRange),
                static_cast<float>(event.velocity) / 127.0f});
        }
    }
    return preview;
}

GenerationResult CompositionEngine::generate(MusicIntent intent,
                                             std::int64_t createdUnixMs,
                                             std::span<const GeneratedClip> parents) const
{
    GenerationResult result;
    if (const auto validation = normaliseAndValidate(intent); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (createdUnixMs < 0)
    {
        result.status = juce::Result::fail("Composition creation time cannot be negative");
        return result;
    }

    const auto plan = createHarmonicPlan(intent);
    result.bundle.intent = intent;
    result.bundle.clips.reserve(intent.partCount);
    const auto perPartBudget = std::max<std::size_t>(1,
        static_cast<std::size_t>(intent.constraints.maximumEvents) / intent.partCount);
    for (std::size_t index = 0; index < intent.partCount; ++index)
    {
        const auto part = intent.parts[index];
        const auto* parent = parentFor(part, parents);
        const auto parentId = parent == nullptr ? juce::String{} : parent->id;
        GeneratedClip clip;
        switch (part)
        {
            case PartType::chords:
                clip = generateChords(intent, plan, perPartBudget, createdUnixMs, parentId);
                break;
            case PartType::melody:
                clip = generateMelody(intent, plan, perPartBudget, createdUnixMs, parentId);
                break;
            case PartType::bass:
                clip = generateBass(intent, plan, perPartBudget, createdUnixMs, parentId);
                break;
            case PartType::arp:
                clip = generateArp(intent, plan, perPartBudget, createdUnixMs, parentId);
                break;
            case PartType::count:
                result.status = juce::Result::fail("Unsupported composition part");
                return result;
        }
        humanizeClip(intent, clip);
        result.bundle.clips.push_back(std::move(clip));
    }
    result.status = validateBundle(result.bundle);
    return result;
}

GenerationResult CompositionEngine::moreLikeThis(const CompositionBundle& source,
                                                 std::uint32_t variationIndex,
                                                 std::int64_t createdUnixMs) const
{
    GenerationResult result;
    if (const auto sourceValidation = validateBundle(source); sourceValidation.failed())
    {
        result.status = sourceValidation;
        return result;
    }
    auto intent = source.intent;
    intent.seed = static_cast<std::uint32_t>(deriveSeed(intent.seed,
        "more-like-this:" + juce::String(variationIndex)) & 0xffffffffULL);
    intent.requestId = deterministicUuid(intent.seed,
        source.intent.requestId + ":more-like-this:" + juce::String(variationIndex));
    result = generate(intent, createdUnixMs, source.clips);
    if (!result.succeeded())
        return result;

    for (auto& clip : result.bundle.clips)
    {
        if (const auto* parent = parentFor(clip.part, source.clips);
            parent != nullptr && eventsEqual(clip, *parent) && !clip.events.empty())
        {
            auto& event = clip.events.back();
            event.velocity = event.velocity == 127 ? 126 : event.velocity + 1;
        }
    }
    result.status = validateBundle(result.bundle);
    return result;
}

GenerationResult CompositionEngine::surpriseMe(const MusicIntent& source,
                                               std::uint32_t surpriseIndex,
                                               std::int64_t createdUnixMs) const
{
    auto intent = source;
    intent.seed = static_cast<std::uint32_t>(deriveSeed(intent.seed,
        "surprise:" + juce::String(surpriseIndex)) & 0xffffffffULL);
    intent.requestId = deterministicUuid(intent.seed,
        source.requestId + ":surprise:" + juce::String(surpriseIndex));
    DeterministicRandom random(deriveSeed(intent.seed, "surprise-macros"));
    intent.density = 0.2f + 0.7f * random.unit();
    intent.rhythmComplexity = 0.15f + 0.8f * random.unit();
    intent.tension = 0.1f + 0.85f * random.unit();
    intent.humanization = 0.02f + 0.28f * random.unit();
    intent.repetition = 0.2f + 0.7f * random.unit();
    intent.variation = 0.55f + 0.45f * random.unit();
    intent.genreProfile = static_cast<GenreProfile>(random.integer(0,
        static_cast<int>(GenreProfile::count) - 1));
    intent.emotion = static_cast<Emotion>(random.integer(0,
        static_cast<int>(Emotion::count) - 1));
    intent.arp.mode = static_cast<ArpMode>(random.integer(0, static_cast<int>(ArpMode::count) - 1));
    return generate(intent, createdUnixMs);
}
}
