#include "CompositionJson.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace folkpark::persistence
{
namespace
{
constexpr int maximumPayloadDepth = 24;
constexpr int maximumPayloadStringBytes = 4096;
constexpr int maximumPayloadTokens = 500000;

juce::Result scanPayload(const juce::String& json)
{
    const auto byteCount = static_cast<std::int64_t>(json.getNumBytesAsUTF8());
    if (byteCount <= 0 || byteCount > maximumHistoryPayloadBytes)
        return juce::Result::fail("History JSON payload exceeds its 4 MiB bound");
    const auto* bytes = json.toRawUTF8();
    std::array<char, maximumPayloadDepth> nesting{};
    std::array<bool, maximumPayloadDepth> objectContainer{};
    std::array<bool, maximumPayloadDepth> expectKey{};
    std::array<std::set<juce::String>, maximumPayloadDepth> keys;
    int depth = 0;
    int tokens = 0;
    int stringBytes = 0;
    std::int64_t stringStart = -1;
    bool inString = false;
    bool escaped = false;
    for (std::int64_t index = 0; index < byteCount; ++index)
    {
        const auto current = static_cast<unsigned char>(bytes[index]);
        if (inString)
        {
            if (current < 0x20 || ++stringBytes > maximumPayloadStringBytes)
                return juce::Result::fail("History JSON contains an invalid or oversized string");
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (current == '\\')
            {
                escaped = true;
                continue;
            }
            if (current == '"')
            {
                inString = false;
                ++tokens;
                if (depth > 0 && objectContainer[static_cast<std::size_t>(depth - 1)]
                    && expectKey[static_cast<std::size_t>(depth - 1)])
                {
                    const auto token = juce::String::fromUTF8(
                        bytes + stringStart, static_cast<int>(index - stringStart + 1));
                    const auto decoded = juce::JSON::fromString(token);
                    if (!decoded.isString()
                        || !keys[static_cast<std::size_t>(depth - 1)].insert(decoded.toString()).second)
                        return juce::Result::fail("History JSON contains a duplicate object key");
                }
            }
            continue;
        }
        if (current == '"')
        {
            inString = true;
            escaped = false;
            stringBytes = 0;
            stringStart = index;
            continue;
        }
        if (current == '{' || current == '[')
        {
            if (depth >= maximumPayloadDepth)
                return juce::Result::fail("History JSON exceeds its nesting bound");
            const auto container = static_cast<std::size_t>(depth);
            nesting[container] = static_cast<char>(current);
            objectContainer[container] = current == '{';
            expectKey[container] = current == '{';
            keys[container].clear();
            ++depth;
            ++tokens;
        }
        else if (current == '}' || current == ']')
        {
            if (depth <= 0)
                return juce::Result::fail("History JSON has mismatched containers");
            const auto open = nesting[static_cast<std::size_t>(--depth)];
            if ((current == '}' && open != '{') || (current == ']' && open != '['))
                return juce::Result::fail("History JSON has mismatched containers");
        }
        else if (current == ',' || current == ':')
        {
            if (depth > 0 && objectContainer[static_cast<std::size_t>(depth - 1)])
                expectKey[static_cast<std::size_t>(depth - 1)] = current == ',';
            ++tokens;
        }
        if (tokens > maximumPayloadTokens)
            return juce::Result::fail("History JSON exceeds its structural-token bound");
    }
    if (inString || escaped || depth != 0)
        return juce::Result::fail("History JSON is structurally incomplete");
    return juce::Result::ok();
}

const juce::DynamicObject* objectOf(const juce::var& value)
{
    return value.getDynamicObject();
}

bool allowed(const juce::DynamicObject& object, std::initializer_list<const char*> fields)
{
    for (const auto& property : object.getProperties())
    {
        const auto name = property.name.toString();
        if (std::none_of(fields.begin(), fields.end(), [&name](const char* field)
                         { return name == field; }))
            return false;
    }
    return true;
}

bool stringField(const juce::DynamicObject& object,
                 const char* name,
                 juce::String& output,
                 int maximumLength,
                 bool allowEmpty = false)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!value.isString())
        return false;
    output = value.toString();
    return output.length() <= maximumLength && (allowEmpty || !output.isEmpty());
}

bool integerField(const juce::DynamicObject& object,
                  const char* name,
                  std::int64_t minimum,
                  std::int64_t maximum,
                  std::int64_t& output)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!(value.isInt() || value.isInt64()))
        return false;
    output = static_cast<std::int64_t>(value);
    return output >= minimum && output <= maximum;
}

bool numberField(const juce::DynamicObject& object,
                 const char* name,
                 double minimum,
                 double maximum,
                 double& output)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!(value.isInt() || value.isInt64() || value.isDouble()))
        return false;
    output = static_cast<double>(value);
    return std::isfinite(output) && output >= minimum && output <= maximum;
}

bool boolField(const juce::DynamicObject& object, const char* name, bool& output)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!value.isBool())
        return false;
    output = static_cast<bool>(value);
    return true;
}

template <typename Enum, typename StableId>
std::optional<Enum> parseEnum(const juce::String& text, Enum count, StableId stableId)
{
    for (int raw = 0; raw < static_cast<int>(count); ++raw)
    {
        const auto value = static_cast<Enum>(raw);
        if (stableId(value) == text)
            return value;
    }
    return std::nullopt;
}

juce::var timeSignatureObject(const midi::TimeSignature& signature)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("numerator", signature.numerator);
    object->setProperty("denominator", signature.denominator);
    return juce::var(object);
}

bool parseTimeSignature(const juce::var& value, midi::TimeSignature& signature)
{
    const auto* object = objectOf(value);
    std::int64_t numerator = 0, denominator = 0;
    if (object == nullptr || !allowed(*object, {"numerator", "denominator"})
        || !integerField(*object, "numerator", 2, 12, numerator)
        || !integerField(*object, "denominator", 2, 16, denominator))
        return false;
    signature = {static_cast<int>(numerator), static_cast<int>(denominator)};
    return true;
}

juce::var intentObject(const midi::MusicIntent& intent)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", intent.schemaVersion);
    root->setProperty("requestId", intent.requestId);
    root->setProperty("seed", static_cast<std::int64_t>(intent.seed));
    root->setProperty("key", midi::stableId(intent.key));
    root->setProperty("scale", midi::stableId(intent.scale));
    root->setProperty("tempoBpm", intent.tempoBpm);
    root->setProperty("timeSignature", timeSignatureObject(intent.timeSignature));
    root->setProperty("lengthBars", intent.lengthBars);
    juce::Array<juce::var> parts;
    for (std::size_t index = 0; index < intent.partCount; ++index)
        parts.add(midi::stableId(intent.parts[index]));
    root->setProperty("parts", juce::var(parts));
    root->setProperty("genreProfile", midi::stableId(intent.genreProfile));
    root->setProperty("emotion", midi::stableId(intent.emotion));
    root->setProperty("density", static_cast<double>(intent.density));
    root->setProperty("rhythmComplexity", static_cast<double>(intent.rhythmComplexity));
    root->setProperty("tension", static_cast<double>(intent.tension));
    root->setProperty("humanization", static_cast<double>(intent.humanization));
    root->setProperty("repetition", static_cast<double>(intent.repetition));
    root->setProperty("variation", static_cast<double>(intent.variation));
    auto* arp = new juce::DynamicObject();
    arp->setProperty("mode", midi::stableId(intent.arp.mode));
    arp->setProperty("rateDivision", midi::stableId(intent.arp.rateDivision));
    arp->setProperty("gate", static_cast<double>(intent.arp.gate));
    arp->setProperty("octaveSpan", intent.arp.octaveSpan);
    arp->setProperty("latch", intent.arp.latch);
    arp->setProperty("sync", intent.arp.sync);
    root->setProperty("arp", juce::var(arp));
    auto* constraints = new juce::DynamicObject();
    constraints->setProperty("lowestMidiNote", intent.constraints.lowestMidiNote);
    constraints->setProperty("highestMidiNote", intent.constraints.highestMidiNote);
    constraints->setProperty("maxPolyphony", intent.constraints.maxPolyphony);
    constraints->setProperty("maximumEvents", intent.constraints.maximumEvents);
    root->setProperty("constraints", juce::var(constraints));
    return juce::var(root);
}

juce::Result parseIntentObject(const juce::DynamicObject& root, midi::MusicIntent& intent)
{
    if (!allowed(root, {"schemaVersion", "requestId", "seed", "key", "scale", "tempoBpm",
                        "timeSignature", "lengthBars", "parts", "genreProfile", "emotion",
                        "density", "rhythmComplexity", "tension", "humanization", "repetition",
                        "variation", "arp", "constraints"}))
        return juce::Result::fail("History MusicIntent contains an unknown field");
    std::int64_t schema = 0, seed = 0, bars = 0;
    double tempo = 0.0, density = 0.0, complexity = 0.0, tension = 0.0;
    double humanization = 0.0, repetition = 0.0, variation = 0.0;
    juce::String key, scale, genre, emotion;
    if (!integerField(root, "schemaVersion", 1, 1, schema)
        || !stringField(root, "requestId", intent.requestId, 36)
        || !integerField(root, "seed", 0, std::numeric_limits<std::uint32_t>::max(), seed)
        || !stringField(root, "key", key, 2) || !stringField(root, "scale", scale, 32)
        || !numberField(root, "tempoBpm", 20.0, 400.0, tempo)
        || !parseTimeSignature(root.getProperty("timeSignature"), intent.timeSignature)
        || !integerField(root, "lengthBars", 1, 64, bars)
        || !stringField(root, "genreProfile", genre, 32)
        || !stringField(root, "emotion", emotion, 32)
        || !numberField(root, "density", 0.0, 1.0, density)
        || !numberField(root, "rhythmComplexity", 0.0, 1.0, complexity)
        || !numberField(root, "tension", 0.0, 1.0, tension)
        || !numberField(root, "humanization", 0.0, 1.0, humanization)
        || !numberField(root, "repetition", 0.0, 1.0, repetition)
        || !numberField(root, "variation", 0.0, 1.0, variation))
        return juce::Result::fail("History MusicIntent fields are invalid");
    const auto keyValue = midi::parseKeyRoot(key);
    const auto scaleValue = midi::parseScaleType(scale);
    const auto genreValue = parseEnum(genre, midi::GenreProfile::count,
                                      [](auto value) { return midi::stableId(value); });
    const auto emotionValue = parseEnum(emotion, midi::Emotion::count,
                                        [](auto value) { return midi::stableId(value); });
    if (!keyValue || !scaleValue || !genreValue || !emotionValue)
        return juce::Result::fail("History MusicIntent uses an unsupported enum ID");
    intent.schemaVersion = static_cast<int>(schema);
    intent.seed = static_cast<std::uint32_t>(seed);
    intent.key = *keyValue;
    intent.scale = *scaleValue;
    intent.tempoBpm = tempo;
    intent.lengthBars = static_cast<int>(bars);
    intent.genreProfile = *genreValue;
    intent.emotion = *emotionValue;
    intent.density = static_cast<float>(density);
    intent.rhythmComplexity = static_cast<float>(complexity);
    intent.tension = static_cast<float>(tension);
    intent.humanization = static_cast<float>(humanization);
    intent.repetition = static_cast<float>(repetition);
    intent.variation = static_cast<float>(variation);

    const auto partsValue = root.getProperty("parts");
    const auto* parts = partsValue.getArray();
    if (parts == nullptr || parts->isEmpty() || parts->size() > 4)
        return juce::Result::fail("History MusicIntent parts are invalid");
    intent.partCount = static_cast<std::size_t>(parts->size());
    for (int index = 0; index < parts->size(); ++index)
    {
        if (!parts->getReference(index).isString())
            return juce::Result::fail("History MusicIntent part must be text");
        const auto part = parseEnum(parts->getReference(index).toString(), midi::PartType::count,
                                    [](auto value) { return midi::stableId(value); });
        if (!part)
            return juce::Result::fail("History MusicIntent part is unsupported");
        intent.parts[static_cast<std::size_t>(index)] = *part;
    }

    const auto arpValue = root.getProperty("arp");
    const auto* arp = objectOf(arpValue);
    juce::String mode, division;
    double gate = 0.0;
    std::int64_t octaveSpan = 0;
    if (arp == nullptr || !allowed(*arp, {"mode", "rateDivision", "gate", "octaveSpan", "latch", "sync"})
        || !stringField(*arp, "mode", mode, 24)
        || !stringField(*arp, "rateDivision", division, 24)
        || !numberField(*arp, "gate", 0.01, 1.0, gate)
        || !integerField(*arp, "octaveSpan", 1, 4, octaveSpan)
        || !boolField(*arp, "latch", intent.arp.latch)
        || !boolField(*arp, "sync", intent.arp.sync))
        return juce::Result::fail("History MusicIntent arpeggiator is invalid");
    const auto modeValue = parseEnum(mode, midi::ArpMode::count,
                                     [](auto value) { return midi::stableId(value); });
    const auto divisionValue = parseEnum(division, midi::ArpRateDivision::count,
                                         [](auto value) { return midi::stableId(value); });
    if (!modeValue || !divisionValue)
        return juce::Result::fail("History MusicIntent arpeggiator enum is unsupported");
    intent.arp.mode = *modeValue;
    intent.arp.rateDivision = *divisionValue;
    intent.arp.gate = static_cast<float>(gate);
    intent.arp.octaveSpan = static_cast<int>(octaveSpan);

    const auto constraintsValue = root.getProperty("constraints");
    const auto* constraints = objectOf(constraintsValue);
    std::int64_t lowest = 0, highest = 0, polyphony = 0, maximumEvents = 0;
    if (constraints == nullptr || !allowed(*constraints,
            {"lowestMidiNote", "highestMidiNote", "maxPolyphony", "maximumEvents"})
        || !integerField(*constraints, "lowestMidiNote", 0, 127, lowest)
        || !integerField(*constraints, "highestMidiNote", 0, 127, highest)
        || !integerField(*constraints, "maxPolyphony", 1, 16, polyphony)
        || !integerField(*constraints, "maximumEvents", 1,
                         static_cast<std::int64_t>(midi::maximumGeneratedEvents), maximumEvents))
        return juce::Result::fail("History MusicIntent constraints are invalid");
    intent.constraints = {static_cast<int>(lowest), static_cast<int>(highest),
                          static_cast<int>(polyphony), static_cast<int>(maximumEvents)};
    return midi::validateMusicIntent(intent);
}

juce::var clipObject(const midi::GeneratedClip& clip)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", clip.schemaVersion);
    root->setProperty("id", clip.id);
    root->setProperty("part", midi::stableId(clip.part));
    root->setProperty("ppq", clip.ppq);
    root->setProperty("lengthTicks", clip.lengthTicks);
    root->setProperty("tempoBpm", clip.tempoBpm);
    root->setProperty("timeSignature", timeSignatureObject(clip.timeSignature));
    root->setProperty("key", midi::stableId(clip.key));
    root->setProperty("scale", midi::stableId(clip.scale));
    root->setProperty("seed", static_cast<std::int64_t>(clip.seed));
    root->setProperty("generatorVersion", clip.generatorVersion);
    root->setProperty("parentClipId", clip.parentClipId);
    root->setProperty("createdUnixMs", clip.createdUnixMs);
    juce::Array<juce::var> events;
    for (const auto& event : clip.events)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("startTick", event.startTick);
        object->setProperty("durationTicks", event.durationTicks);
        object->setProperty("pitch", event.pitch);
        object->setProperty("velocity", event.velocity);
        object->setProperty("channel", event.channel);
        object->setProperty("probability", static_cast<double>(event.probability));
        object->setProperty("articulation", midi::stableId(event.articulation));
        events.add(juce::var(object));
    }
    root->setProperty("events", juce::var(events));
    juce::Array<juce::var> labels;
    for (const auto& label : clip.chordLabels)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("startTick", label.startTick);
        object->setProperty("durationTicks", label.durationTicks);
        object->setProperty("symbol", label.symbol);
        object->setProperty("scaleDegree", label.scaleDegree);
        labels.add(juce::var(object));
    }
    root->setProperty("chordLabels", juce::var(labels));
    return juce::var(root);
}

juce::Result parseClipObject(const juce::DynamicObject& root, midi::GeneratedClip& clip)
{
    if (!allowed(root, {"schemaVersion", "id", "part", "ppq", "lengthTicks", "tempoBpm",
                        "timeSignature", "key", "scale", "seed", "generatorVersion",
                        "parentClipId", "createdUnixMs", "events", "chordLabels"}))
        return juce::Result::fail("History GeneratedClip contains an unknown field");
    std::int64_t schema = 0, ppq = 0, length = 0, seed = 0, created = 0;
    double tempo = 0.0;
    juce::String part, key, scale;
    if (!integerField(root, "schemaVersion", 1, 1, schema)
        || !stringField(root, "id", clip.id, 36)
        || !stringField(root, "part", part, 16)
        || !integerField(root, "ppq", 24, 9600, ppq)
        || !integerField(root, "lengthTicks", 1, 983040, length)
        || !numberField(root, "tempoBpm", 20.0, 400.0, tempo)
        || !parseTimeSignature(root.getProperty("timeSignature"), clip.timeSignature)
        || !stringField(root, "key", key, 2) || !stringField(root, "scale", scale, 32)
        || !integerField(root, "seed", 0, std::numeric_limits<std::uint32_t>::max(), seed)
        || !stringField(root, "generatorVersion", clip.generatorVersion, 32)
        || !stringField(root, "parentClipId", clip.parentClipId, 36, true)
        || !integerField(root, "createdUnixMs", 0, std::numeric_limits<std::int64_t>::max(), created))
        return juce::Result::fail("History GeneratedClip fields are invalid");
    const auto partValue = parseEnum(part, midi::PartType::count,
                                     [](auto value) { return midi::stableId(value); });
    const auto keyValue = midi::parseKeyRoot(key);
    const auto scaleValue = midi::parseScaleType(scale);
    if (!partValue || !keyValue || !scaleValue)
        return juce::Result::fail("History GeneratedClip uses an unsupported enum ID");
    clip.schemaVersion = static_cast<int>(schema);
    clip.part = *partValue;
    clip.ppq = static_cast<int>(ppq);
    clip.lengthTicks = length;
    clip.tempoBpm = tempo;
    clip.key = *keyValue;
    clip.scale = *scaleValue;
    clip.seed = static_cast<std::uint32_t>(seed);
    clip.createdUnixMs = created;

    const auto eventsValue = root.getProperty("events");
    const auto* events = eventsValue.getArray();
    if (events == nullptr || events->size() > static_cast<int>(midi::maximumGeneratedEvents))
        return juce::Result::fail("History GeneratedClip events exceed their bound");
    clip.events.reserve(static_cast<std::size_t>(events->size()));
    for (const auto& eventValue : *events)
    {
        const auto* event = objectOf(eventValue);
        std::int64_t start = 0, duration = 0, pitch = 0, velocity = 0, channel = 0;
        double probability = 0.0;
        juce::String articulation;
        if (event == nullptr || !allowed(*event,
                {"startTick", "durationTicks", "pitch", "velocity", "channel", "probability", "articulation"})
            || !integerField(*event, "startTick", 0, length, start)
            || !integerField(*event, "durationTicks", 1, length, duration)
            || !integerField(*event, "pitch", 0, 127, pitch)
            || !integerField(*event, "velocity", 1, 127, velocity)
            || !integerField(*event, "channel", 1, 16, channel)
            || !numberField(*event, "probability", 0.0, 1.0, probability)
            || !stringField(*event, "articulation", articulation, 16))
            return juce::Result::fail("History GeneratedClip contains an invalid event");
        const auto articulationValue = parseEnum(articulation, midi::Articulation::count,
                                                  [](auto value) { return midi::stableId(value); });
        if (!articulationValue)
            return juce::Result::fail("History event articulation is unsupported");
        clip.events.push_back({start, duration, static_cast<int>(pitch), static_cast<int>(velocity),
                               static_cast<int>(channel), static_cast<float>(probability),
                               *articulationValue});
    }

    const auto labelsValue = root.getProperty("chordLabels");
    const auto* labels = labelsValue.getArray();
    if (labels == nullptr || labels->size() > 512)
        return juce::Result::fail("History GeneratedClip chord labels exceed their bound");
    clip.chordLabels.reserve(static_cast<std::size_t>(labels->size()));
    for (const auto& labelValue : *labels)
    {
        const auto* label = objectOf(labelValue);
        std::int64_t start = 0, duration = 0, degree = 0;
        juce::String symbol;
        if (label == nullptr || !allowed(*label,
                {"startTick", "durationTicks", "symbol", "scaleDegree"})
            || !integerField(*label, "startTick", 0, length, start)
            || !integerField(*label, "durationTicks", 1, length, duration)
            || !stringField(*label, "symbol", symbol, 24)
            || !integerField(*label, "scaleDegree", 1, 7, degree))
            return juce::Result::fail("History GeneratedClip chord label is invalid");
        clip.chordLabels.push_back({start, duration, symbol, static_cast<int>(degree)});
    }
    return midi::validateGeneratedClip(clip);
}

juce::Result parseJsonObject(const juce::String& json, juce::var& parsed)
{
    if (const auto scan = scanPayload(json); scan.failed())
        return scan;
    const auto parse = juce::JSON::parse(json, parsed);
    if (parse.failed() || parsed.getDynamicObject() == nullptr)
        return juce::Result::fail("History JSON payload is malformed: " + parse.getErrorMessage());
    return juce::Result::ok();
}
}

MusicIntentJsonResult encodeMusicIntentJson(const midi::MusicIntent& intent)
{
    MusicIntentJsonResult result;
    result.intent = intent;
    if (const auto validation = midi::validateMusicIntent(intent); validation.failed())
    {
        result.status = validation;
        return result;
    }
    result.json = juce::JSON::toString(intentObject(intent), true, 9);
    result.status = scanPayload(result.json);
    return result;
}

MusicIntentJsonResult decodeMusicIntentJson(const juce::String& json)
{
    MusicIntentJsonResult result;
    juce::var parsed;
    if (const auto parse = parseJsonObject(json, parsed); parse.failed())
    {
        result.status = parse;
        return result;
    }
    result.status = parseIntentObject(*parsed.getDynamicObject(), result.intent);
    if (result.status.wasOk())
        result.json = encodeMusicIntentJson(result.intent).json;
    return result;
}

CompositionJsonResult encodeCompositionJson(const midi::CompositionBundle& bundle)
{
    CompositionJsonResult result;
    result.bundle = bundle;
    if (const auto validation = midi::validateBundle(bundle); validation.failed())
    {
        result.status = validation;
        return result;
    }
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", 1);
    root->setProperty("intent", intentObject(bundle.intent));
    juce::Array<juce::var> clips;
    for (const auto& clip : bundle.clips)
        clips.add(clipObject(clip));
    root->setProperty("clips", juce::var(clips));
    result.json = juce::JSON::toString(juce::var(root), true, 9);
    result.status = scanPayload(result.json);
    return result;
}

CompositionJsonResult decodeCompositionJson(const juce::String& json)
{
    CompositionJsonResult result;
    juce::var parsed;
    if (const auto parse = parseJsonObject(json, parsed); parse.failed())
    {
        result.status = parse;
        return result;
    }
    const auto& root = *parsed.getDynamicObject();
    std::int64_t schema = 0;
    if (!allowed(root, {"schemaVersion", "intent", "clips"})
        || !integerField(root, "schemaVersion", 1, 1, schema))
    {
        result.status = juce::Result::fail("History composition root is invalid");
        return result;
    }
    const auto intentValue = root.getProperty("intent");
    const auto* intent = objectOf(intentValue);
    if (intent == nullptr)
    {
        result.status = juce::Result::fail("History composition intent is missing");
        return result;
    }
    if (const auto intentResult = parseIntentObject(*intent, result.bundle.intent); intentResult.failed())
    {
        result.status = intentResult;
        return result;
    }
    const auto clipsValue = root.getProperty("clips");
    const auto* clips = clipsValue.getArray();
    if (clips == nullptr || clips->isEmpty() || clips->size() > 4)
    {
        result.status = juce::Result::fail("History composition clips are invalid");
        return result;
    }
    result.bundle.clips.reserve(static_cast<std::size_t>(clips->size()));
    for (const auto& clipValue : *clips)
    {
        const auto* clipObjectValue = objectOf(clipValue);
        if (clipObjectValue == nullptr)
        {
            result.status = juce::Result::fail("History composition clip must be an object");
            return result;
        }
        midi::GeneratedClip clip;
        if (const auto clipResult = parseClipObject(*clipObjectValue, clip); clipResult.failed())
        {
            result.status = clipResult;
            return result;
        }
        result.bundle.clips.push_back(std::move(clip));
    }
    result.status = midi::validateBundle(result.bundle);
    if (result.status.wasOk())
        result.json = encodeCompositionJson(result.bundle).json;
    return result;
}
}
