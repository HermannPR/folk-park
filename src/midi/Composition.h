#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace folkpark::midi
{
inline constexpr int compositionPpq = 960;
inline constexpr std::size_t maximumGeneratedEvents = 16384;
inline constexpr std::size_t maximumPreviewNotes = 4096;
inline constexpr auto compositionGeneratorVersion = "1.0.0-m3";

enum class PartType : std::uint8_t
{
    chords,
    melody,
    bass,
    arp,
    count
};

enum class KeyRoot : std::uint8_t
{
    c,
    cSharp,
    d,
    dSharp,
    e,
    f,
    fSharp,
    g,
    gSharp,
    a,
    aSharp,
    b,
    count
};

enum class ScaleType : std::uint8_t
{
    major,
    naturalMinor,
    harmonicMinor,
    dorian,
    mixolydian,
    pentatonicMajor,
    pentatonicMinor,
    count
};

enum class GenreProfile : std::uint8_t
{
    generic,
    melodicTechno,
    house,
    ambient,
    synthwave,
    cinematic,
    count
};

enum class Emotion : std::uint8_t
{
    neutral,
    dark,
    bright,
    tense,
    hopeful,
    dreamy,
    count
};

enum class ArpMode : std::uint8_t
{
    up,
    down,
    upDown,
    randomSeeded,
    chordOrder,
    count
};

enum class ArpRateDivision : std::uint8_t
{
    quarter,
    eighth,
    sixteenth,
    thirtySecond,
    count
};

enum class Articulation : std::uint8_t
{
    normal,
    staccato,
    legato,
    accent,
    count
};

struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
};

struct ArpSettings
{
    ArpMode mode = ArpMode::up;
    ArpRateDivision rateDivision = ArpRateDivision::sixteenth;
    float gate = 0.8f;
    int octaveSpan = 1;
    bool latch = false;
    bool sync = true;
};

struct CompositionConstraints
{
    int lowestMidiNote = 36;
    int highestMidiNote = 84;
    int maxPolyphony = 6;
    int maximumEvents = 4096;
};

struct MusicIntent
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String requestId;
    std::uint32_t seed = 12345;
    KeyRoot key = KeyRoot::d;
    ScaleType scale = ScaleType::naturalMinor;
    double tempoBpm = 124.0;
    TimeSignature timeSignature;
    int lengthBars = 4;
    std::array<PartType, 4> parts{PartType::chords, PartType::melody,
                                  PartType::bass, PartType::arp};
    std::size_t partCount = 4;
    GenreProfile genreProfile = GenreProfile::melodicTechno;
    Emotion emotion = Emotion::dark;
    float density = 0.55f;
    float rhythmComplexity = 0.45f;
    float tension = 0.7f;
    float humanization = 0.12f;
    float repetition = 0.6f;
    float variation = 0.4f;
    ArpSettings arp;
    CompositionConstraints constraints;
};

struct NoteEvent
{
    std::int64_t startTick = 0;
    std::int64_t durationTicks = compositionPpq / 4;
    int pitch = 60;
    int velocity = 100;
    int channel = 1;
    float probability = 1.0f;
    Articulation articulation = Articulation::normal;

};

struct ChordLabel
{
    std::int64_t startTick = 0;
    std::int64_t durationTicks = compositionPpq;
    juce::String symbol;
    int scaleDegree = 1;

    friend bool operator==(const ChordLabel&, const ChordLabel&) = default;
};

struct GeneratedClip
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String id;
    PartType part = PartType::chords;
    int ppq = compositionPpq;
    std::int64_t lengthTicks = compositionPpq * 16;
    double tempoBpm = 124.0;
    TimeSignature timeSignature;
    KeyRoot key = KeyRoot::d;
    ScaleType scale = ScaleType::naturalMinor;
    std::uint32_t seed = 0;
    juce::String generatorVersion{compositionGeneratorVersion};
    juce::String parentClipId;
    std::int64_t createdUnixMs = 0;
    std::vector<NoteEvent> events;
    std::vector<ChordLabel> chordLabels;
};

struct CompositionBundle
{
    MusicIntent intent;
    std::vector<GeneratedClip> clips;
};

struct GenerationResult
{
    juce::Result status = juce::Result::fail("Composition generation did not run");
    CompositionBundle bundle;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

struct PreviewNote
{
    PartType part = PartType::chords;
    float normalisedStart = 0.0f;
    float normalisedDuration = 0.0f;
    float normalisedPitch = 0.0f;
    float normalisedVelocity = 0.0f;
};

struct PianoRollPreview
{
    std::int64_t lengthTicks = 0;
    int lowestPitch = 0;
    int highestPitch = 127;
    std::size_t sourceEventCount = 0;
    bool truncated = false;
    std::vector<PreviewNote> notes;
};

[[nodiscard]] juce::String stableId(PartType value);
[[nodiscard]] juce::String stableId(KeyRoot value);
[[nodiscard]] juce::String stableId(ScaleType value);
[[nodiscard]] juce::String stableId(GenreProfile value);
[[nodiscard]] juce::String stableId(Emotion value);
[[nodiscard]] juce::String stableId(ArpMode value);
[[nodiscard]] juce::String stableId(ArpRateDivision value);
[[nodiscard]] juce::String stableId(Articulation value);

[[nodiscard]] std::optional<KeyRoot> parseKeyRoot(const juce::String& text);
[[nodiscard]] std::optional<ScaleType> parseScaleType(const juce::String& text);
[[nodiscard]] std::span<const int> scaleIntervals(ScaleType scale) noexcept;
[[nodiscard]] bool isUuid(const juce::String& text) noexcept;
[[nodiscard]] juce::String deterministicUuid(std::uint64_t seed, const juce::String& domain);
[[nodiscard]] std::int64_t ticksPerBar(const TimeSignature& signature, int ppq) noexcept;

[[nodiscard]] juce::Result normaliseAndValidate(MusicIntent& intent);
[[nodiscard]] juce::Result validateMusicIntent(const MusicIntent& intent);
[[nodiscard]] juce::Result validateGeneratedClip(const GeneratedClip& clip);
[[nodiscard]] juce::Result validateBundle(const CompositionBundle& bundle);
[[nodiscard]] PianoRollPreview createPianoRollPreview(const CompositionBundle& bundle);

class CompositionEngine final
{
public:
    [[nodiscard]] GenerationResult generate(MusicIntent intent,
                                            std::int64_t createdUnixMs = 0,
                                            std::span<const GeneratedClip> parents = {}) const;
    [[nodiscard]] GenerationResult moreLikeThis(const CompositionBundle& source,
                                                std::uint32_t variationIndex,
                                                std::int64_t createdUnixMs = 0) const;
    [[nodiscard]] GenerationResult surpriseMe(const MusicIntent& source,
                                              std::uint32_t surpriseIndex,
                                              std::int64_t createdUnixMs = 0) const;
};
}
