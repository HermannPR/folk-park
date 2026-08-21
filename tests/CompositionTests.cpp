#include "midi/Composition.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

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

bool eventsMatch(const folkpark::midi::GeneratedClip& first,
                 const folkpark::midi::GeneratedClip& second)
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
    return first.chordLabels == second.chordLabels;
}

const folkpark::midi::GeneratedClip* clipFor(const folkpark::midi::CompositionBundle& bundle,
                                             folkpark::midi::PartType part)
{
    const auto found = std::find_if(bundle.clips.begin(), bundle.clips.end(), [part](const auto& clip)
    {
        return clip.part == part;
    });
    return found == bundle.clips.end() ? nullptr : &*found;
}

folkpark::midi::MusicIntent standardIntent()
{
    folkpark::midi::MusicIntent intent;
    intent.requestId = folkpark::midi::deterministicUuid(intent.seed, "composition-test-request");
    return intent;
}

void testParsingAndValidation()
{
    using namespace folkpark::midi;
    expect(parseKeyRoot("Db") == KeyRoot::cSharp, "Flat key alias must parse canonically");
    expect(parseKeyRoot("Bb") == KeyRoot::aSharp, "B-flat alias must parse canonically");
    expect(!parseKeyRoot("H").has_value(), "Unsupported key must be rejected");
    expect(parseScaleType("natural minor") == ScaleType::naturalMinor,
           "Scale names with spaces must parse canonically");
    expect(parseScaleType("minor") == ScaleType::naturalMinor,
           "Common minor alias must resolve to natural minor");
    expect(!parseScaleType("copyrighted artist scale").has_value(),
           "Unsupported scale label must be rejected");

    auto candidate = standardIntent();
    candidate.tempoBpm = 900.0;
    candidate.lengthBars = 100;
    candidate.density = -1.0f;
    candidate.tension = 2.0f;
    candidate.constraints.maximumEvents = 99999;
    expect(normaliseAndValidate(candidate).wasOk(),
           "Explicit normalization must clamp numeric candidates before validation");
    expect(candidate.tempoBpm == 400.0 && candidate.lengthBars == 64
               && candidate.density == 0.0f && candidate.tension == 1.0f
               && candidate.constraints.maximumEvents == static_cast<int>(maximumGeneratedEvents),
           "Numeric MusicIntent clamps must match documented bounds");

    auto invalid = standardIntent();
    invalid.scale = static_cast<ScaleType>(255);
    expect(normaliseAndValidate(invalid).failed(), "Unsupported enum must be rejected, not clamped");
    invalid = standardIntent();
    invalid.parts[1] = invalid.parts[0];
    expect(validateMusicIntent(invalid).failed(), "Duplicate requested parts must be rejected");
    invalid = standardIntent();
    invalid.requestId = "not-a-uuid";
    expect(validateMusicIntent(invalid).failed(), "Malformed request ID must be rejected");
    invalid = standardIntent();
    invalid.constraints.maxPolyphony = 2;
    expect(validateMusicIntent(invalid).failed(),
           "Chord/arp request must reject insufficient polyphony");
}

void testDeterminismAndMusicalProperties()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    const auto intent = standardIntent();
    const auto first = engine.generate(intent, 123456789);
    const auto second = engine.generate(intent, 123456789);
    expect(first.succeeded() && second.succeeded(), "Standard four-part generation must succeed");
    if (!first.succeeded())
        std::cerr << "  generation error: " << first.status.getErrorMessage() << '\n';
    if (!first.succeeded() || !second.succeeded())
        return;
    expect(validateBundle(first.bundle).wasOk(), "Generated four-part bundle must validate");
    expect(first.bundle.clips.size() == 4, "Every requested part must produce one clip");
    for (std::size_t index = 0; index < first.bundle.clips.size(); ++index)
    {
        const auto& left = first.bundle.clips[index];
        const auto& right = second.bundle.clips[index];
        expect(left.id == right.id && left.seed == right.seed && eventsMatch(left, right),
               "Same intent, seed, generator version, and creation metadata must be deterministic");
        expect(!left.events.empty(), "Every standard generated part must contain notes");
    }

    const auto* chords = clipFor(first.bundle, PartType::chords);
    const auto* melody = clipFor(first.bundle, PartType::melody);
    const auto* bass = clipFor(first.bundle, PartType::bass);
    const auto* arp = clipFor(first.bundle, PartType::arp);
    expect(chords != nullptr && melody != nullptr && bass != nullptr && arp != nullptr,
           "Bundle lookup must find chords, melody, bass, and arp");
    if (chords == nullptr || melody == nullptr || bass == nullptr || arp == nullptr)
        return;

    expect(chords->chordLabels.size() >= 2, "Chord generator must expose harmonic labels");
    if (chords->chordLabels.size() >= 2)
    {
        expect(chords->chordLabels[chords->chordLabels.size() - 2].scaleDegree == 5
                   && chords->chordLabels.back().scaleDegree == 1,
               "Functional progression must close with a deterministic V-I cadence");
    }
    for (std::size_t index = 1; index < melody->events.size(); ++index)
        expect(std::abs(melody->events[index].pitch - melody->events[index - 1].pitch) <= 12,
               "Melody must prevent excessive leaps");
    expect(melody->events.size() < static_cast<std::size_t>(melody->lengthTicks / (compositionPpq / 4)),
           "Melody must contain rests instead of all-notes-on-grid degeneration");
    for (const auto& event : bass->events)
        expect(event.pitch <= std::min(intent.constraints.highestMidiNote, 60),
               "Bass must respect its bounded low register");
    for (std::size_t index = 1; index < arp->events.size(); ++index)
        expect(arp->events[index - 1].startTick + arp->events[index - 1].durationTicks
                   <= arp->events[index].startTick,
               "Arpeggio clip must be monophonic with complete note lifecycles");

    const auto preview = createPianoRollPreview(first.bundle);
    expect(preview.sourceEventCount > 0 && !preview.notes.empty(),
           "Piano-roll preview must project generated events");
    for (const auto& note : preview.notes)
        expect(note.normalisedStart >= 0.0f && note.normalisedStart <= 1.0f
                   && note.normalisedDuration > 0.0f && note.normalisedDuration <= 1.0f
                   && note.normalisedPitch >= 0.0f && note.normalisedPitch <= 1.0f
                   && note.normalisedVelocity > 0.0f && note.normalisedVelocity <= 1.0f,
               "Piano-roll preview values must remain normalized");
}

void testKeysScalesBoundsAndMacros()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    for (int key = 0; key < static_cast<int>(KeyRoot::count); ++key)
    {
        for (int scale = 0; scale < static_cast<int>(ScaleType::count); ++scale)
        {
            auto intent = standardIntent();
            intent.key = static_cast<KeyRoot>(key);
            intent.scale = static_cast<ScaleType>(scale);
            intent.lengthBars = 2;
            intent.humanization = 0.3f;
            intent.seed = static_cast<std::uint32_t>(1000 + key * 31 + scale);
            intent.requestId = deterministicUuid(intent.seed, "key-scale-property");
            const auto generated = engine.generate(intent, 1);
            expect(generated.succeeded(), "Every supported key/scale pair must generate valid clips");
            if (!generated.succeeded() && key == 0 && scale == 0)
                std::cerr << "  key/scale generation error: "
                          << generated.status.getErrorMessage() << '\n';
            if (generated.succeeded())
                expect(validateBundle(generated.bundle).wasOk(),
                       "Every supported key/scale pair must preserve all event properties");
        }
    }

    auto bounded = standardIntent();
    bounded.lengthBars = 64;
    bounded.timeSignature = {7, 8};
    bounded.constraints.lowestMidiNote = 48;
    bounded.constraints.highestMidiNote = 72;
    bounded.constraints.maximumEvents = 32;
    bounded.humanization = 1.0f;
    const auto generated = engine.generate(bounded, 1);
    expect(generated.succeeded(), "Maximum bars, odd meter, tight range, event cap, and humanization must remain valid");
    if (generated.succeeded())
    {
        auto count = std::size_t{0};
        for (const auto& clip : generated.bundle.clips)
            count += clip.events.size();
        expect(count <= 32, "Generated bundle must obey the requested total event cap");
    }

    auto sparse = standardIntent();
    sparse.partCount = 1;
    sparse.parts[0] = PartType::melody;
    sparse.density = 0.1f;
    sparse.requestId = deterministicUuid(sparse.seed, "sparse");
    auto dense = sparse;
    dense.density = 0.95f;
    dense.requestId = deterministicUuid(dense.seed, "dense");
    const auto sparseResult = engine.generate(sparse, 1);
    const auto denseResult = engine.generate(dense, 1);
    expect(sparseResult.succeeded() && denseResult.succeeded(),
           "Sparse and dense macro fixtures must both generate");
    if (sparseResult.succeeded() && denseResult.succeeded())
        expect(denseResult.bundle.clips[0].events.size() > sparseResult.bundle.clips[0].events.size(),
               "Density macro must materially map to melody event count");
}

void testVariationLineage()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    const auto source = engine.generate(standardIntent(), 100);
    expect(source.succeeded(), "Variation source must generate");
    if (!source.succeeded())
        return;

    const auto variation = engine.moreLikeThis(source.bundle, 1, 200);
    expect(variation.succeeded(), "More Like This must generate a validated child bundle");
    if (variation.succeeded())
    {
        expect(variation.bundle.intent.seed != source.bundle.intent.seed,
               "More Like This must derive a new seed");
        expect(variation.bundle.intent.key == source.bundle.intent.key
                   && variation.bundle.intent.scale == source.bundle.intent.scale
                   && variation.bundle.intent.parts == source.bundle.intent.parts
                   && variation.bundle.intent.partCount == source.bundle.intent.partCount
                   && variation.bundle.intent.lengthBars == source.bundle.intent.lengthBars,
               "More Like This must retain selected musical features");
        auto changed = false;
        for (const auto& child : variation.bundle.clips)
        {
            const auto* parent = clipFor(source.bundle, child.part);
            expect(parent != nullptr && child.parentClipId == parent->id,
                   "More Like This clip must retain explicit parent lineage");
            if (parent != nullptr)
                changed = changed || !eventsMatch(child, *parent);
        }
        expect(changed, "More Like This must make a controlled musical difference");
    }

    const auto surprise = engine.surpriseMe(source.bundle.intent, 4, 300);
    expect(surprise.succeeded(), "Surprise Me must return a bounded reviewable bundle");
    if (surprise.succeeded())
    {
        expect(surprise.bundle.intent.seed != source.bundle.intent.seed
                   && surprise.bundle.intent.variation >= 0.55f,
               "Surprise Me must derive a new seed and deliberate variation");
        expect(validateBundle(surprise.bundle).wasOk(),
               "Surprise Me cannot bypass normal intent and clip validation");
    }
}

void testMalformedClipRejection()
{
    using namespace folkpark::midi;
    CompositionEngine engine;
    auto generated = engine.generate(standardIntent(), 1);
    expect(generated.succeeded(), "Malformed fixture source must generate");
    if (!generated.succeeded())
        return;
    auto clip = *clipFor(generated.bundle, PartType::melody);
    clip.events[0].durationTicks = 0;
    expect(validateGeneratedClip(clip).failed(), "Zero-duration event must be rejected");
    clip = *clipFor(generated.bundle, PartType::melody);
    if (clip.events.size() >= 2)
    {
        clip.events[1].startTick = clip.events[0].startTick;
        expect(validateGeneratedClip(clip).failed(),
               "Overlapping/reordered monophonic events must be rejected");
    }
    clip = *clipFor(generated.bundle, PartType::chords);
    clip.events[0].pitch = 128;
    expect(validateGeneratedClip(clip).failed(), "Out-of-range MIDI pitch must be rejected");
}
}

int main()
{
    testParsingAndValidation();
    testDeterminismAndMusicalProperties();
    testKeysScalesBoundsAndMacros();
    testVariationLineage();
    testMalformedClipRejection();
    if (failures == 0)
        std::cout << "PASS: M3 intent, deterministic composition, properties, preview, and lineage\n";
    return failures == 0 ? 0 : 1;
}
