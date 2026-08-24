#include "Preset.h"

#include "common/ParameterIds.h"
#include "synth/WavetableConverter.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <utility>

namespace folkpark::persistence
{
namespace
{
constexpr std::array reservedTopLevel{
    "schemaVersion", "product", "productVersion", "metadata", "parameters",
    "modulationRoutes", "effects", "assets", "preview", "migrationProvenance"};

juce::Result validateJsonBounds(const juce::String& json)
{
    const auto* utf8 = json.toRawUTF8();
    const auto byteCount = static_cast<std::int64_t>(json.getNumBytesAsUTF8());
    if (byteCount <= 0 || byteCount > maximumPresetBytes)
        return juce::Result::fail("Preset JSON byte length is outside the 1 MiB bound");

    std::array<char, maximumJsonDepth> nesting{};
    int depth = 0;
    int tokens = 0;
    int stringBytes = 0;
    std::int64_t stringStart = -1;
    bool inString = false;
    bool escaped = false;
    std::array<bool, maximumJsonDepth> objectContainer{};
    std::array<bool, maximumJsonDepth> expectObjectKey{};
    std::array<std::set<juce::String>, maximumJsonDepth> objectKeys;

    const auto* bytes = utf8;
    for (std::int64_t index = 0; index < byteCount; ++index)
    {
        const auto current = static_cast<unsigned char>(bytes[index]);
        if (inString)
        {
            if (current < 0x20)
                return juce::Result::fail("Preset JSON contains a control character in a string");
            if (++stringBytes > maximumJsonStringBytes)
                return juce::Result::fail("Preset JSON string exceeds the 4096-byte bound");
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
                    && expectObjectKey[static_cast<std::size_t>(depth - 1)])
                {
                    const auto tokenBytes = static_cast<int>(index - stringStart + 1);
                    const auto token = juce::String::fromUTF8(bytes + stringStart, tokenBytes);
                    const auto decoded = juce::JSON::fromString(token);
                    if (!decoded.isString()
                        || !objectKeys[static_cast<std::size_t>(depth - 1)]
                                .insert(decoded.toString()).second)
                        return juce::Result::fail("Preset JSON contains a duplicate object key");
                }
            }
            continue;
        }

        if (current == '"')
        {
            inString = true;
            stringBytes = 0;
            stringStart = index;
            continue;
        }
        if (current == '{' || current == '[')
        {
            if (depth >= maximumJsonDepth)
                return juce::Result::fail("Preset JSON nesting exceeds the depth bound");
            const auto containerIndex = static_cast<std::size_t>(depth);
            nesting[containerIndex] = static_cast<char>(current);
            objectContainer[containerIndex] = current == '{';
            expectObjectKey[containerIndex] = current == '{';
            objectKeys[containerIndex].clear();
            ++depth;
            ++tokens;
        }
        else if (current == '}' || current == ']')
        {
            if (depth <= 0)
                return juce::Result::fail("Preset JSON has mismatched containers");
            const auto open = nesting[static_cast<std::size_t>(--depth)];
            if ((current == '}' && open != '{') || (current == ']' && open != '['))
                return juce::Result::fail("Preset JSON has mismatched containers");
        }
        else if (current == ',' || current == ':')
        {
            if (depth > 0 && objectContainer[static_cast<std::size_t>(depth - 1)])
                expectObjectKey[static_cast<std::size_t>(depth - 1)] = current == ',';
            ++tokens;
        }

        if (tokens > maximumJsonStructuralTokens)
            return juce::Result::fail("Preset JSON exceeds the structural-token bound");
    }

    if (inString || escaped || depth != 0)
        return juce::Result::fail("Preset JSON is structurally incomplete");
    return juce::Result::ok();
}

bool isUuid(const juce::String& value)
{
    if (value.length() != 36)
        return false;
    for (int index = 0; index < value.length(); ++index)
    {
        const auto character = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23)
        {
            if (character != '-')
                return false;
        }
        else if (juce::CharacterFunctions::getHexDigitValue(character) < 0)
        {
            return false;
        }
    }
    return true;
}

bool isLowerSha256(const juce::String& value)
{
    if (value.length() != 64)
        return false;
    for (const auto character : value)
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
    return true;
}

bool validText(const juce::String& value, int maximumLength, bool allowEmpty = true)
{
    if (!allowEmpty && value.trim().isEmpty())
        return false;
    if (value.length() > maximumLength)
        return false;
    for (const auto character : value)
        if (character < 0x20 && character != '\t' && character != '\n')
            return false;
    return true;
}

bool validSemanticVersion(const juce::String& value)
{
    if (!validText(value, 32, false))
        return false;
    const auto core = value.upToFirstOccurrenceOf("-", false, false)
                           .upToFirstOccurrenceOf("+", false, false);
    juce::StringArray components;
    components.addTokens(core, ".", {});
    if (components.size() != 3)
        return false;
    for (const auto& component : components)
        if (component.isEmpty() || !component.containsOnly("0123456789"))
            return false;
    for (const auto character : value)
        if (!(juce::CharacterFunctions::isLetterOrDigit(character)
              || character == '.' || character == '-' || character == '+'))
            return false;
    return true;
}

bool validateExtensionValue(const juce::var& value, int depth, int& nodes)
{
    if (depth > maximumJsonDepth || ++nodes > maximumJsonStructuralTokens)
        return false;
    if (value.isUndefined() || value.isBinaryData() || value.isMethod())
        return false;
    if (value.isVoid() || value.isBool() || value.isInt() || value.isInt64())
        return true;
    if (value.isDouble())
        return std::isfinite(static_cast<double>(value));
    if (value.isString())
        return validText(value.toString(), maximumJsonStringBytes);
    if (const auto* array = value.getArray())
    {
        if (array->size() > maximumJsonStructuralTokens)
            return false;
        for (const auto& item : *array)
            if (!validateExtensionValue(item, depth + 1, nodes))
                return false;
        return true;
    }
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;
    std::set<juce::String> keys;
    for (const auto& property : object->getProperties())
    {
        const auto name = property.name.toString();
        if (!validText(name, 64, false) || !keys.insert(name).second
            || !validateExtensionValue(property.value, depth + 1, nodes))
            return false;
    }
    return true;
}

juce::var canonicalExtensionValue(const juce::var& value)
{
    if (const auto* array = value.getArray())
    {
        juce::Array<juce::var> canonical;
        for (const auto& item : *array)
            canonical.add(canonicalExtensionValue(item));
        return juce::var(canonical);
    }
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return value;
    std::vector<std::pair<juce::String, juce::var>> properties;
    properties.reserve(static_cast<std::size_t>(object->getProperties().size()));
    for (const auto& property : object->getProperties())
        properties.emplace_back(property.name.toString(), property.value);
    std::sort(properties.begin(), properties.end(), [](const auto& left, const auto& right)
    {
        return left.first < right.first;
    });
    auto* canonical = new juce::DynamicObject();
    for (const auto& [name, propertyValue] : properties)
        canonical->setProperty(name, canonicalExtensionValue(propertyValue));
    return juce::var(canonical);
}

bool hasAllowedProperties(const juce::DynamicObject& object,
                          std::initializer_list<const char*> allowed)
{
    for (const auto& property : object.getProperties())
    {
        const auto name = property.name.toString();
        const auto found = std::any_of(allowed.begin(), allowed.end(), [&name](const char* item)
        {
            return name == item;
        });
        if (!found)
            return false;
    }
    return true;
}

const juce::DynamicObject* asObject(const juce::var& value)
{
    return value.getDynamicObject();
}

bool readString(const juce::DynamicObject& object,
                const char* name,
                juce::String& output,
                int maximumLength,
                bool allowEmpty = true)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!value.isString())
        return false;
    output = value.toString();
    return validText(output, maximumLength, allowEmpty);
}

bool readBool(const juce::DynamicObject& object, const char* name, bool& output)
{
    if (!object.hasProperty(name))
        return false;
    const auto value = object.getProperty(name);
    if (!value.isBool())
        return false;
    output = static_cast<bool>(value);
    return true;
}

bool readInteger(const juce::DynamicObject& object,
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

bool readFiniteNumber(const juce::DynamicObject& object,
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

juce::String deterministicUuidFromText(const juce::String& text)
{
    const auto* utf8 = text.toRawUTF8();
    auto hash = juce::SHA256(utf8, text.getNumBytesAsUTF8())
                    .toHexString().substring(0, 32).toLowerCase();
    hash = hash.substring(0, 12) + "4" + hash.substring(13);
    const auto variant = juce::CharacterFunctions::getHexDigitValue(hash[16]);
    hash = hash.substring(0, 16) + juce::String::toHexString((variant & 0x3) | 0x8)
        + hash.substring(17);
    return hash.substring(0, 8) + "-" + hash.substring(8, 12) + "-"
        + hash.substring(12, 16) + "-" + hash.substring(16, 20) + "-"
        + hash.substring(20, 32);
}

template <std::size_t Size>
bool exactParameterIds(const std::vector<ParameterValue>& values,
                       const std::array<const char*, Size>& expected)
{
    if (values.size() != expected.size())
        return false;
    std::set<juce::String> actual;
    for (const auto& value : values)
    {
        if (!std::isfinite(value.normalized) || value.normalized < 0.0f || value.normalized > 1.0f)
            return false;
        if (!actual.insert(value.id).second)
            return false;
    }
    return std::all_of(expected.begin(), expected.end(), [&actual](const char* id)
    {
        return actual.contains(id);
    });
}

template <std::size_t Size>
std::vector<ParameterValue> makeZeroParameters(const std::array<const char*, Size>& ids)
{
    std::vector<ParameterValue> result;
    result.reserve(ids.size());
    for (const auto* id : ids)
        result.push_back({id, 0.0f});
    return result;
}

std::span<const char* const> effectIdsFor(const juce::String& effect)
{
    if (effect == "distortion") return parameterIds::distortion;
    if (effect == "chorus") return parameterIds::chorus;
    if (effect == "delay") return parameterIds::delay;
    if (effect == "reverb") return parameterIds::reverb;
    if (effect == "compressor") return parameterIds::compressor;
    if (effect == "eq") return parameterIds::equalizer;
    return {};
}

bool exactEffectParameterIds(const EffectState& effect)
{
    const auto expected = effectIdsFor(effect.id);
    if (expected.empty() || effect.parameters.size() != expected.size())
        return false;
    std::set<juce::String> actual;
    for (const auto& value : effect.parameters)
    {
        if (!std::isfinite(value.normalized) || value.normalized < 0.0f || value.normalized > 1.0f)
            return false;
        if (!actual.insert(value.id).second)
            return false;
    }
    return std::all_of(expected.begin(), expected.end(), [&actual](const char* id)
    {
        return actual.contains(id);
    });
}

const ParameterValue* findParameter(const PresetDocument& document, const juce::String& id)
{
    for (const auto& parameter : document.parameters)
        if (parameter.id == id)
            return &parameter;
    for (const auto& effect : document.effects)
        for (const auto& parameter : effect.parameters)
            if (parameter.id == id)
                return &parameter;
    return nullptr;
}

ParameterValue* findParameter(PresetDocument& document, const juce::String& id)
{
    return const_cast<ParameterValue*>(findParameter(std::as_const(document), id));
}

std::optional<synth::ModulationSource> parseSource(const juce::String& id)
{
    for (const auto& descriptor : synth::ModulationRegistry::sources())
        if (id == descriptor.stableId)
            return descriptor.source;
    return std::nullopt;
}

std::optional<synth::ModulationDestination> parseDestination(const juce::String& id)
{
    for (const auto& descriptor : synth::ModulationRegistry::destinations())
        if (id == descriptor.stableId)
            return descriptor.destination;
    return std::nullopt;
}

juce::String curveId(synth::ModulationCurve curve)
{
    switch (curve)
    {
        case synth::ModulationCurve::linear: return "linear";
        case synth::ModulationCurve::exponential: return "exponential";
        case synth::ModulationCurve::sCurve: return "sCurve";
        case synth::ModulationCurve::count: break;
    }
    return {};
}

std::optional<synth::ModulationCurve> parseCurve(const juce::String& id)
{
    if (id == "linear") return synth::ModulationCurve::linear;
    if (id == "exponential") return synth::ModulationCurve::exponential;
    if (id == "sCurve") return synth::ModulationCurve::sCurve;
    return std::nullopt;
}

juce::Result parseParameters(const juce::var& value, std::vector<ParameterValue>& output)
{
    const auto* object = asObject(value);
    if (object == nullptr)
        return juce::Result::fail("Preset parameters must be an object");
    output.clear();
    output.reserve(static_cast<std::size_t>(object->getProperties().size()));
    for (const auto& property : object->getProperties())
    {
        const auto id = property.name.toString();
        const auto number = property.value;
        if (id.isEmpty() || id.length() > 64
            || !(number.isInt() || number.isInt64() || number.isDouble()))
            return juce::Result::fail("Preset contains an invalid parameter field");
        const auto normalized = static_cast<double>(number);
        if (!std::isfinite(normalized) || normalized < 0.0 || normalized > 1.0)
            return juce::Result::fail("Preset parameter values must be finite and normalized");
        output.push_back({id, static_cast<float>(normalized)});
    }
    return juce::Result::ok();
}

juce::Result parseRoutes(const juce::var& value, std::vector<synth::ModulationRoute>& output)
{
    const auto* array = value.getArray();
    if (array == nullptr || array->size() > static_cast<int>(synth::ModulationSnapshot::maximumRoutes))
        return juce::Result::fail("Preset modulation routes are not a bounded array");
    output.clear();
    output.reserve(static_cast<std::size_t>(array->size()));
    for (const auto& item : *array)
    {
        const auto* object = asObject(item);
        if (object == nullptr || !hasAllowedProperties(*object,
                {"source", "destination", "amount", "curve", "enabled"}))
            return juce::Result::fail("Preset contains a malformed modulation route");
        juce::String sourceId, destinationId, curveName;
        double amount = 0.0;
        bool enabled = false;
        if (!readString(*object, "source", sourceId, 32, false)
            || !readString(*object, "destination", destinationId, 64, false)
            || !readFiniteNumber(*object, "amount", -1.0, 1.0, amount)
            || !readString(*object, "curve", curveName, 16, false)
            || !readBool(*object, "enabled", enabled))
            return juce::Result::fail("Preset modulation route fields are invalid");
        const auto source = parseSource(sourceId);
        const auto destination = parseDestination(destinationId);
        const auto curve = parseCurve(curveName);
        if (!source || !destination || !curve)
            return juce::Result::fail("Preset modulation route uses an unsupported stable ID");
        output.push_back({*source, *destination, static_cast<float>(amount), *curve, enabled});
    }
    return synth::ModulationRegistry::validate(output);
}

juce::Result parseMetadata(const juce::var& value, PresetMetadata& output)
{
    const auto* object = asObject(value);
    if (object == nullptr || !hasAllowedProperties(*object,
            {"id", "name", "author", "tags", "genre", "emotion", "description", "favorite"}))
        return juce::Result::fail("Preset metadata is malformed or contains an unknown field");
    if (!readString(*object, "id", output.id, 36, false) || !isUuid(output.id)
        || !readString(*object, "name", output.name, 96, false)
        || !readString(*object, "author", output.author, 96)
        || !readString(*object, "genre", output.genre, 64)
        || !readString(*object, "emotion", output.emotion, 64)
        || !readString(*object, "description", output.description, 1024)
        || !readBool(*object, "favorite", output.favorite))
        return juce::Result::fail("Preset metadata fields are outside their bounds");

    const auto tagsValue = object->getProperty("tags");
    const auto* tags = tagsValue.getArray();
    if (tags == nullptr || tags->size() > 24)
        return juce::Result::fail("Preset tags must be a bounded array");
    std::set<juce::String> unique;
    output.tags.clear();
    for (const auto& tagValue : *tags)
    {
        if (!tagValue.isString())
            return juce::Result::fail("Preset tag must be text");
        const auto tag = tagValue.toString();
        if (!validText(tag, 48, false) || !unique.insert(tag).second)
            return juce::Result::fail("Preset tags must be bounded and unique");
        output.tags.push_back(tag);
    }
    return juce::Result::ok();
}

juce::Result parseEffects(const juce::var& value, std::vector<EffectState>& output)
{
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != static_cast<int>(parameterIds::effectOrder.size()))
        return juce::Result::fail("Preset must contain the complete ordered effect chain");
    output.clear();
    for (int index = 0; index < array->size(); ++index)
    {
        const auto* object = asObject(array->getReference(index));
        if (object == nullptr || !hasAllowedProperties(*object, {"id", "parameters"}))
            return juce::Result::fail("Preset contains a malformed effect state");
        EffectState effect;
        if (!readString(*object, "id", effect.id, 16, false)
            || effect.id != parameterIds::effectOrder[static_cast<std::size_t>(index)])
            return juce::Result::fail("Preset effect order is unsupported");
        if (const auto result = parseParameters(object->getProperty("parameters"), effect.parameters);
            result.failed())
            return result;
        output.push_back(std::move(effect));
    }
    return juce::Result::ok();
}

juce::Result parseAssets(const juce::var& value, std::vector<AssetReference>& output)
{
    const auto* array = value.getArray();
    if (array == nullptr || array->size() > 2)
        return juce::Result::fail("Preset asset references exceed the two-oscillator bound");
    output.clear();
    std::set<AssetSlot> slots;
    for (const auto& item : *array)
    {
        const auto* object = asObject(item);
        if (object == nullptr || !hasAllowedProperties(*object,
                {"kind", "slot", "sha256", "relativePath", "byteSize", "recovery"}))
            return juce::Result::fail("Preset contains a malformed asset reference");
        juce::String kind, slot, sha256, relativePath;
        std::int64_t byteSize = 0;
        if (!readString(*object, "kind", kind, 32, false) || kind != "wavetableSource"
            || !readString(*object, "slot", slot, 32, false)
            || !readString(*object, "sha256", sha256, 64, false)
            || !readString(*object, "relativePath", relativePath, 82, false)
            || !readInteger(*object, "byteSize", 1, maximumAssetBytes, byteSize))
            return juce::Result::fail("Preset asset fields are outside their bounds");
        AssetReference reference;
        reference.kind = AssetKind::wavetableSource;
        if (slot == "oscillatorA") reference.slot = AssetSlot::oscillatorA;
        else if (slot == "oscillatorB") reference.slot = AssetSlot::oscillatorB;
        else return juce::Result::fail("Preset asset slot is unsupported");
        reference.sha256 = sha256;
        reference.relativePath = relativePath;
        reference.byteSize = byteSize;

        const auto recoveryValue = object->getProperty("recovery");
        const auto* recovery = asObject(recoveryValue);
        if (recovery == nullptr || !hasAllowedProperties(*recovery, {"displayName", "originalSha256"})
            || !readString(*recovery, "displayName", reference.recoveryDisplayName, 128, false)
            || !readString(*recovery, "originalSha256", reference.originalSha256, 64, false))
            return juce::Result::fail("Preset asset recovery metadata is invalid");
        if (!slots.insert(reference.slot).second)
            return juce::Result::fail("Preset contains duplicate asset slots");
        output.push_back(std::move(reference));
    }
    return juce::Result::ok();
}

juce::var parameterObject(const std::vector<ParameterValue>& values,
                          std::span<const char* const> order)
{
    auto* object = new juce::DynamicObject();
    for (const auto* id : order)
    {
        const auto iterator = std::find_if(values.begin(), values.end(), [id](const auto& value)
        {
            return value.id == id;
        });
        if (iterator != values.end())
            object->setProperty(id, static_cast<double>(iterator->normalized));
    }
    return juce::var(object);
}

juce::var routeArray(const std::vector<synth::ModulationRoute>& routes)
{
    juce::Array<juce::var> result;
    for (const auto& route : routes)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("source", synth::ModulationRegistry::descriptor(route.source)->stableId);
        object->setProperty("destination",
                            synth::ModulationRegistry::descriptor(route.destination)->stableId);
        object->setProperty("amount", static_cast<double>(route.amount));
        object->setProperty("curve", curveId(route.curve));
        object->setProperty("enabled", route.enabled);
        result.add(juce::var(object));
    }
    return juce::var(result);
}

juce::var buildCurrentObject(const PresetDocument& document)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", currentPresetSchemaVersion);

    auto* product = new juce::DynamicObject();
    product->setProperty("identifier", document.productIdentifier);
    product->setProperty("name", document.productName);
    product->setProperty("version", document.productVersion);
    root->setProperty("product", juce::var(product));

    auto* metadata = new juce::DynamicObject();
    metadata->setProperty("id", document.metadata.id);
    metadata->setProperty("name", document.metadata.name);
    metadata->setProperty("author", document.metadata.author);
    auto tags = document.metadata.tags;
    std::sort(tags.begin(), tags.end());
    juce::Array<juce::var> tagValues;
    for (const auto& tag : tags)
        tagValues.add(tag);
    metadata->setProperty("tags", juce::var(tagValues));
    metadata->setProperty("genre", document.metadata.genre);
    metadata->setProperty("emotion", document.metadata.emotion);
    metadata->setProperty("description", document.metadata.description);
    metadata->setProperty("favorite", document.metadata.favorite);
    root->setProperty("metadata", juce::var(metadata));

    root->setProperty("parameters", parameterObject(document.parameters, parameterIds::synthAndModulation));
    root->setProperty("modulationRoutes", routeArray(document.modulationRoutes));

    juce::Array<juce::var> effects;
    for (const auto* effectId : parameterIds::effectOrder)
    {
        const auto iterator = std::find_if(document.effects.begin(), document.effects.end(),
                                           [effectId](const auto& effect) { return effect.id == effectId; });
        auto* object = new juce::DynamicObject();
        object->setProperty("id", effectId);
        if (iterator != document.effects.end())
            object->setProperty("parameters", parameterObject(iterator->parameters,
                                                               effectIdsFor(effectId)));
        effects.add(juce::var(object));
    }
    root->setProperty("effects", juce::var(effects));

    juce::Array<juce::var> assets;
    for (const auto& reference : document.assets)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("kind", stableId(reference.kind));
        object->setProperty("slot", stableId(reference.slot));
        object->setProperty("sha256", reference.sha256);
        object->setProperty("relativePath", reference.relativePath);
        object->setProperty("byteSize", reference.byteSize);
        auto* recovery = new juce::DynamicObject();
        recovery->setProperty("displayName", reference.recoveryDisplayName);
        recovery->setProperty("originalSha256", reference.originalSha256);
        object->setProperty("recovery", juce::var(recovery));
        assets.add(juce::var(object));
    }
    root->setProperty("assets", juce::var(assets));

    if (document.preview)
    {
        auto* preview = new juce::DynamicObject();
        preview->setProperty("kind", "audio/wav");
        preview->setProperty("sha256", document.preview->sha256);
        preview->setProperty("durationSeconds", document.preview->durationSeconds);
        root->setProperty("preview", juce::var(preview));
    }

    auto* migration = new juce::DynamicObject();
    migration->setProperty("originalSchemaVersion", document.migration.originalSchemaVersion);
    juce::Array<juce::var> steps;
    for (const auto& step : document.migration.steps)
        steps.add(step);
    migration->setProperty("steps", juce::var(steps));
    root->setProperty("migrationProvenance", juce::var(migration));

    auto extensions = document.unknownTopLevelFields;
    std::sort(extensions.begin(), extensions.end(), [](const auto& left, const auto& right)
    {
        return left.name < right.name;
    });
    for (const auto& extension : extensions)
        root->setProperty(extension.name, canonicalExtensionValue(extension.value));
    return juce::var(root);
}

juce::Result parseCurrent(const juce::DynamicObject& root, PresetDocument& document)
{
    const auto productValue = root.getProperty("product");
    const auto* product = asObject(productValue);
    if (product == nullptr || !hasAllowedProperties(*product, {"identifier", "name", "version"})
        || !readString(*product, "identifier", document.productIdentifier, 64, false)
        || !readString(*product, "name", document.productName, 32, false)
        || !readString(*product, "version", document.productVersion, 32, false))
        return juce::Result::fail("Preset product metadata is invalid");

    if (const auto result = parseMetadata(root.getProperty("metadata"), document.metadata);
        result.failed())
        return result;
    if (const auto result = parseParameters(root.getProperty("parameters"), document.parameters);
        result.failed())
        return result;
    if (const auto result = parseRoutes(root.getProperty("modulationRoutes"), document.modulationRoutes);
        result.failed())
        return result;
    if (const auto result = parseEffects(root.getProperty("effects"), document.effects);
        result.failed())
        return result;
    if (const auto result = parseAssets(root.getProperty("assets"), document.assets); result.failed())
        return result;

    if (root.hasProperty("preview") && !root.getProperty("preview").isVoid())
    {
        const auto previewValue = root.getProperty("preview");
        if (previewValue.isVoid() || previewValue.isUndefined())
            return juce::Result::fail("Preset preview metadata is invalid");
        const auto* preview = asObject(previewValue);
        juce::String kind;
        PreviewMetadata metadata;
        if (preview == nullptr || !hasAllowedProperties(*preview, {"kind", "sha256", "durationSeconds"})
            || !readString(*preview, "kind", kind, 16, false) || kind != "audio/wav"
            || !readString(*preview, "sha256", metadata.sha256, 64, false)
            || !readFiniteNumber(*preview, "durationSeconds", 0.0, 900.0,
                                 metadata.durationSeconds))
            return juce::Result::fail("Preset preview metadata is invalid");
        document.preview = metadata;
    }

    const auto migrationValue = root.getProperty("migrationProvenance");
    const auto* migration = asObject(migrationValue);
    std::int64_t original = 0;
    if (migration == nullptr || !hasAllowedProperties(*migration, {"originalSchemaVersion", "steps"})
        || !readInteger(*migration, "originalSchemaVersion", oldestPresetSchemaVersion,
                        currentPresetSchemaVersion, original))
        return juce::Result::fail("Preset migration provenance is invalid");
    document.migration.originalSchemaVersion = static_cast<int>(original);
    const auto stepsValue = migration->getProperty("steps");
    const auto* steps = stepsValue.getArray();
    if (steps == nullptr || steps->size() > 8)
        return juce::Result::fail("Preset migration steps are invalid");
    std::set<juce::String> uniqueSteps;
    for (const auto& stepValue : *steps)
    {
        if (!stepValue.isString())
            return juce::Result::fail("Preset migration step must be text");
        const auto step = stepValue.toString();
        if (!validText(step, 64, false) || !uniqueSteps.insert(step).second)
            return juce::Result::fail("Preset migration steps must be bounded and unique");
        document.migration.steps.push_back(step);
    }

    for (const auto& property : root.getProperties())
    {
        const auto name = property.name.toString();
        const auto reserved = std::any_of(reservedTopLevel.begin(), reservedTopLevel.end(),
                                          [&name](const char* field) { return name == field; });
        if (!reserved)
        {
            if (!validText(name, 64, false))
                return juce::Result::fail("Preset extension field name is invalid");
            document.unknownTopLevelFields.push_back({name, property.value});
        }
    }
    return validatePreset(document);
}

juce::Result migrateLegacy(const juce::DynamicObject& root,
                           const juce::String& sourceJson,
                           const PresetDocument& defaults,
                           PresetDocument& output)
{
    if (const auto defaultsResult = validatePreset(defaults); defaultsResult.failed())
        return juce::Result::fail("Preset migration defaults are invalid: "
                                  + defaultsResult.getErrorMessage());
    output = defaults;
    output.schemaVersion = currentPresetSchemaVersion;
    output.migration.originalSchemaVersion = oldestPresetSchemaVersion;
    output.migration.steps = {"preset-v1-to-v2"};
    output.unknownTopLevelFields.clear();
    output.metadata = {};

    if (!readString(root, "productVersion", output.productVersion, 32, false))
        return juce::Result::fail("Legacy preset productVersion is invalid");

    const auto metadataValue = root.getProperty("metadata");
    const auto* metadata = asObject(metadataValue);
    if (metadata == nullptr || metadata->getProperties().size() > 32)
        return juce::Result::fail("Legacy preset metadata is invalid");
    output.metadata.id = deterministicUuidFromText(sourceJson);
    output.metadata.name = "Migrated preset";
    if (metadata->hasProperty("id"))
    {
        juce::String id;
        if (!readString(*metadata, "id", id, 36, false) || !isUuid(id))
            return juce::Result::fail("Legacy preset metadata ID is invalid");
        output.metadata.id = id;
    }
    if (metadata->hasProperty("name")
        && !readString(*metadata, "name", output.metadata.name, 96, false))
        return juce::Result::fail("Legacy preset name is invalid");
    if (metadata->hasProperty("author")
        && !readString(*metadata, "author", output.metadata.author, 96))
        return juce::Result::fail("Legacy preset author is invalid");
    if (metadata->hasProperty("genre")
        && !readString(*metadata, "genre", output.metadata.genre, 64))
        return juce::Result::fail("Legacy preset genre is invalid");
    if (metadata->hasProperty("emotion")
        && !readString(*metadata, "emotion", output.metadata.emotion, 64))
        return juce::Result::fail("Legacy preset emotion is invalid");
    if (metadata->hasProperty("description")
        && !readString(*metadata, "description", output.metadata.description, 1024))
        return juce::Result::fail("Legacy preset description is invalid");
    if (metadata->hasProperty("favorite")
        && !readBool(*metadata, "favorite", output.metadata.favorite))
        return juce::Result::fail("Legacy preset favorite value is invalid");
    if (metadata->hasProperty("tags"))
    {
        const auto tagsValue = metadata->getProperty("tags");
        const auto* tags = tagsValue.getArray();
        if (tags == nullptr || tags->size() > 24)
            return juce::Result::fail("Legacy preset tags are invalid");
        std::set<juce::String> uniqueTags;
        for (const auto& tagValue : *tags)
        {
            if (!tagValue.isString())
                return juce::Result::fail("Legacy preset tag must be text");
            const auto tag = tagValue.toString();
            if (!validText(tag, 48, false) || !uniqueTags.insert(tag).second)
                return juce::Result::fail("Legacy preset tags must be bounded and unique");
            output.metadata.tags.push_back(tag);
        }
    }

    std::vector<ParameterValue> legacyParameters;
    if (const auto result = parseParameters(root.getProperty("parameters"), legacyParameters);
        result.failed())
        return result;
    for (const auto& legacy : legacyParameters)
    {
        auto* destination = findParameter(output, legacy.id);
        if (destination == nullptr)
            return juce::Result::fail("Legacy preset contains an unknown parameter ID");
        destination->normalized = legacy.normalized;
    }

    if (root.hasProperty("modulationRoutes"))
    {
        if (const auto result = parseRoutes(root.getProperty("modulationRoutes"),
                                            output.modulationRoutes); result.failed())
            return result;
    }
    else
    {
        output.modulationRoutes.clear();
    }

    if (root.hasProperty("assets"))
    {
        const auto assetsValue = root.getProperty("assets");
        const auto* assets = assetsValue.getArray();
        if (assets == nullptr || !assets->isEmpty())
            return juce::Result::fail("Legacy preset assets have no safe version-1 semantics");
    }
    output.assets.clear();
    output.preview.reset();
    return validatePreset(output);
}

struct TemporaryFileCleaner
{
    explicit TemporaryFileCleaner(juce::File target) : file(std::move(target)) {}
    ~TemporaryFileCleaner() { if (active) file.deleteFile(); }
    void release() noexcept { active = false; }
    juce::File file;
    bool active = true;
};

juce::Result verifyWavetableSource(const juce::File& file, const AssetReference& reference)
{
    if (file.isSymbolicLink())
        return juce::Result::fail("Preset asset symbolic links are not accepted");
    if (!file.existsAsFile())
        return juce::Result::fail("Preset asset is missing");
    if (file.getSize() != reference.byteSize || file.getSize() <= 0
        || file.getSize() > maximumAssetBytes)
        return juce::Result::fail("Preset asset byte length differs from its reference");
    const auto actualHash = juce::SHA256(file).toHexString().toLowerCase();
    if (actualHash != reference.sha256)
        return juce::Result::fail("Preset asset SHA-256 differs from its reference");
    const synth::WavetableConverter converter;
    const auto conversion = converter.convertWavFile(file);
    if (!conversion.succeeded())
        return juce::Result::fail("Preset wavetable asset is not decodable within bounds: "
                                  + conversion.status.getErrorMessage());
    return juce::Result::ok();
}
}

juce::String stableId(AssetKind value)
{
    return value == AssetKind::wavetableSource ? "wavetableSource" : juce::String{};
}

juce::String stableId(AssetSlot value)
{
    if (value == AssetSlot::oscillatorA) return "oscillatorA";
    if (value == AssetSlot::oscillatorB) return "oscillatorB";
    return {};
}

PresetDocument makePresetTemplate(const juce::String& productVersion,
                                  const juce::String& presetId,
                                  const juce::String& presetName)
{
    PresetDocument document;
    document.productVersion = productVersion;
    document.metadata.id = presetId;
    document.metadata.name = presetName;
    document.parameters = makeZeroParameters(parameterIds::synthAndModulation);
    document.effects = {
        {"distortion", makeZeroParameters(parameterIds::distortion)},
        {"chorus", makeZeroParameters(parameterIds::chorus)},
        {"delay", makeZeroParameters(parameterIds::delay)},
        {"reverb", makeZeroParameters(parameterIds::reverb)},
        {"compressor", makeZeroParameters(parameterIds::compressor)},
        {"eq", makeZeroParameters(parameterIds::equalizer)}};
    return document;
}

juce::Result validatePreset(const PresetDocument& document)
{
    if (document.schemaVersion != currentPresetSchemaVersion)
        return juce::Result::fail("Preset schema version is unsupported");
    if (document.productIdentifier != "com.folkpark.audio.folkpark"
        || document.productName != "folk park" || !validSemanticVersion(document.productVersion))
        return juce::Result::fail("Preset product identity is unsupported");
    if (!isUuid(document.metadata.id) || !validText(document.metadata.name, 96, false)
        || !validText(document.metadata.author, 96) || !validText(document.metadata.genre, 64)
        || !validText(document.metadata.emotion, 64)
        || !validText(document.metadata.description, 1024) || document.metadata.tags.size() > 24)
        return juce::Result::fail("Preset metadata is outside its bounds");
    std::set<juce::String> tags;
    for (const auto& tag : document.metadata.tags)
        if (!validText(tag, 48, false) || !tags.insert(tag).second)
            return juce::Result::fail("Preset tags must be bounded and unique");
    if (!exactParameterIds(document.parameters, parameterIds::synthAndModulation))
        return juce::Result::fail("Preset must contain the exact 73-ID synth parameter catalog");
    if (document.effects.size() != parameterIds::effectOrder.size())
        return juce::Result::fail("Preset must contain six ordered effects");
    for (std::size_t index = 0; index < document.effects.size(); ++index)
        if (document.effects[index].id != parameterIds::effectOrder[index]
            || !exactEffectParameterIds(document.effects[index]))
            return juce::Result::fail("Preset effect order or parameter catalog is invalid");
    if (const auto routeResult = synth::ModulationRegistry::validate(document.modulationRoutes);
        routeResult.failed())
        return routeResult;
    if (document.assets.size() > 2)
        return juce::Result::fail("Preset contains too many wavetable assets");
    std::set<AssetSlot> slots;
    for (const auto& asset : document.assets)
    {
        const auto expectedPath = "assets/" + asset.sha256 + ".wav";
        if (asset.kind != AssetKind::wavetableSource || stableId(asset.slot).isEmpty()
            || !slots.insert(asset.slot).second || !isLowerSha256(asset.sha256)
            || asset.relativePath != expectedPath || asset.byteSize <= 0
            || asset.byteSize > maximumAssetBytes
            || !validText(asset.recoveryDisplayName, 128, false)
            || asset.recoveryDisplayName.containsAnyOf("/\\")
            || !isLowerSha256(asset.originalSha256)
            || asset.originalSha256 != asset.sha256)
            return juce::Result::fail("Preset asset reference is invalid or unsafe");
    }
    if (document.preview
        && (!isLowerSha256(document.preview->sha256)
            || !std::isfinite(document.preview->durationSeconds)
            || document.preview->durationSeconds < 0.0
            || document.preview->durationSeconds > 900.0))
        return juce::Result::fail("Preset preview metadata is invalid");
    if (document.migration.originalSchemaVersion < oldestPresetSchemaVersion
        || document.migration.originalSchemaVersion > currentPresetSchemaVersion
        || document.migration.steps.size() > 8)
        return juce::Result::fail("Preset migration provenance is invalid");
    std::set<juce::String> steps;
    for (const auto& step : document.migration.steps)
        if (!validText(step, 64, false) || !steps.insert(step).second)
            return juce::Result::fail("Preset migration steps must be bounded and unique");
    std::set<juce::String> extensions;
    for (const auto& extension : document.unknownTopLevelFields)
    {
        const auto reserved = std::any_of(reservedTopLevel.begin(), reservedTopLevel.end(),
                                          [&extension](const char* field)
                                          { return extension.name == field; });
        int extensionNodes = 0;
        if (reserved || !validText(extension.name, 64, false)
            || !extensions.insert(extension.name).second
            || !validateExtensionValue(extension.value, 1, extensionNodes))
            return juce::Result::fail("Preset extension fields are invalid");
    }
    return juce::Result::ok();
}

juce::String sanitisePresetFilename(const juce::String& requestedName)
{
    juce::String stem;
    const auto source = requestedName.upToLastOccurrenceOf(".folkparkpreset", false, true);
    for (const auto character : source)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character) || character == ' '
            || character == '-' || character == '_')
            stem += character;
        if (stem.length() >= 64)
            break;
    }
    stem = stem.trim();
    while (stem.contains("  "))
        stem = stem.replace("  ", " ");
    if (stem.isEmpty() || stem == "." || stem == "..")
        stem = "Preset";
    return stem + ".folkparkpreset";
}

PresetCodecResult PresetCodec::decode(const juce::String& json,
                                      const PresetDocument& migrationDefaults)
{
    PresetCodecResult result;
    if (const auto bounds = validateJsonBounds(json); bounds.failed())
    {
        result.status = bounds;
        return result;
    }
    juce::var parsed;
    if (const auto parsing = juce::JSON::parse(json, parsed); parsing.failed())
    {
        result.status = juce::Result::fail("Preset JSON is malformed: " + parsing.getErrorMessage());
        return result;
    }
    const auto* root = asObject(parsed);
    if (root == nullptr || !root->hasProperty("schemaVersion"))
    {
        result.status = juce::Result::fail("Preset root must be an object with schemaVersion");
        return result;
    }
    std::int64_t schemaVersion = 0;
    if (!readInteger(*root, "schemaVersion", oldestPresetSchemaVersion,
                     currentPresetSchemaVersion, schemaVersion))
    {
        result.status = juce::Result::fail("Preset schema version is malformed or unsupported");
        return result;
    }

    result.document.schemaVersion = currentPresetSchemaVersion;
    result.status = schemaVersion == currentPresetSchemaVersion
        ? parseCurrent(*root, result.document)
        : migrateLegacy(*root, json, migrationDefaults, result.document);
    if (result.status.failed())
        return result;
    result.migrated = schemaVersion != currentPresetSchemaVersion;
    const auto encoded = encode(result.document);
    if (encoded.status.failed())
    {
        result.status = encoded.status;
        return result;
    }
    result.canonicalJson = encoded.canonicalJson;
    result.status = juce::Result::ok();
    return result;
}

PresetCodecResult PresetCodec::encode(const PresetDocument& document)
{
    PresetCodecResult result;
    result.document = document;
    if (const auto validation = validatePreset(document); validation.failed())
    {
        result.status = validation;
        return result;
    }
    result.canonicalJson = juce::JSON::toString(buildCurrentObject(document), false, 9) + "\n";
    if (const auto bounds = validateJsonBounds(result.canonicalJson); bounds.failed())
    {
        result.status = bounds;
        return result;
    }
    result.status = juce::Result::ok();
    return result;
}

AssetValidationResult PresetAssetStore::validate(const PresetDocument& document,
                                                  const juce::File& presetRoot)
{
    AssetValidationResult result;
    if (const auto validation = validatePreset(document); validation.failed())
    {
        result.status = validation;
        return result;
    }
    if (presetRoot.isSymbolicLink() || presetRoot.getChildFile("assets").isSymbolicLink())
    {
        result.status = juce::Result::fail("Preset root or asset directory may not be a symbolic link");
        return result;
    }
    for (const auto& reference : document.assets)
    {
        const auto file = presetRoot.getChildFile(reference.relativePath);
        if (!file.existsAsFile())
        {
            result.missing.push_back(reference);
            continue;
        }
        if (const auto verification = verifyWavetableSource(file, reference); verification.failed())
        {
            result.status = verification;
            return result;
        }
    }
    result.status = juce::Result::ok();
    return result;
}

juce::Result PresetAssetStore::importWavetableSource(const juce::File& source,
                                                     const juce::File& presetRoot,
                                                     AssetSlot slot,
                                                     AssetReference& imported)
{
    if (stableId(slot).isEmpty() || !source.existsAsFile() || source.isSymbolicLink()
        || !source.hasFileExtension("wav") || source.getSize() <= 0
        || source.getSize() > maximumAssetBytes)
        return juce::Result::fail("Selected wavetable source is missing, linked, oversized, or not WAV");
    if (presetRoot.isSymbolicLink() || presetRoot.getChildFile("assets").isSymbolicLink())
        return juce::Result::fail("Preset root or asset directory may not be a symbolic link");
    const synth::WavetableConverter converter;
    const auto conversion = converter.convertWavFile(source);
    if (!conversion.succeeded())
        return juce::Result::fail("Selected wavetable source is invalid: "
                                  + conversion.status.getErrorMessage());
    const auto hash = conversion.metadata.sourceSha256.toLowerCase();
    const auto assets = presetRoot.getChildFile("assets");
    if (!assets.isDirectory() && !assets.createDirectory())
        return juce::Result::fail("Preset asset directory could not be created");
    const auto destination = assets.getChildFile(hash + ".wav");
    AssetReference reference{AssetKind::wavetableSource, slot, hash,
                             "assets/" + hash + ".wav", source.getSize(),
                             source.getFileName(), hash};
    if (destination.existsAsFile())
    {
        if (const auto existing = verifyWavetableSource(destination, reference); existing.failed())
            return juce::Result::fail("Content-addressed destination exists but is invalid: "
                                      + existing.getErrorMessage());
        imported = reference;
        return juce::Result::ok();
    }
    const auto temporary = assets.getChildFile("." + hash + "." + juce::Uuid().toDashedString()
                                                + ".tmp.wav");
    TemporaryFileCleaner cleanup(temporary);
    if (!source.copyFileTo(temporary))
        return juce::Result::fail("Wavetable source could not be copied to temporary storage");
    if (const auto verification = verifyWavetableSource(temporary, reference); verification.failed())
        return verification;
    if (!temporary.moveFileTo(destination))
        return juce::Result::fail("Validated wavetable asset could not be atomically installed");
    cleanup.release();
    imported = reference;
    return juce::Result::ok();
}

juce::Result PresetAssetStore::relink(const AssetReference& reference,
                                      const juce::File& selectedSource,
                                      const juce::File& presetRoot)
{
    PresetDocument single = makePresetTemplate("0.1.0",
        "00000000-0000-4000-8000-000000000000", "Asset validation");
    single.assets.push_back(reference);
    if (const auto validation = validatePreset(single); validation.failed())
        return validation;
    if (!selectedSource.existsAsFile() || selectedSource.isSymbolicLink()
        || selectedSource.getSize() != reference.byteSize
        || juce::SHA256(selectedSource).toHexString().toLowerCase() != reference.sha256)
        return juce::Result::fail("Selected recovery file does not match the required asset hash and size");
    if (presetRoot.isSymbolicLink() || presetRoot.getChildFile("assets").isSymbolicLink())
        return juce::Result::fail("Preset root or asset directory may not be a symbolic link");
    const auto destination = presetRoot.getChildFile(reference.relativePath);
    if (destination.existsAsFile())
        return verifyWavetableSource(destination, reference);
    if (!destination.getParentDirectory().isDirectory()
        && !destination.getParentDirectory().createDirectory())
        return juce::Result::fail("Preset asset recovery directory could not be created");
    const auto temporary = destination.getSiblingFile("." + destination.getFileNameWithoutExtension()
                                                       + "." + juce::Uuid().toDashedString()
                                                       + ".tmp.wav");
    TemporaryFileCleaner cleanup(temporary);
    if (!selectedSource.copyFileTo(temporary))
        return juce::Result::fail("Selected recovery asset could not be copied");
    if (const auto verification = verifyWavetableSource(temporary, reference); verification.failed())
        return verification;
    if (!temporary.moveFileTo(destination))
        return juce::Result::fail("Validated recovery asset could not be atomically installed");
    cleanup.release();
    return juce::Result::ok();
}

PresetLoadResult PresetStore::load(const juce::File& file,
                                   const PresetDocument& migrationDefaults,
                                   const juce::File& presetRoot)
{
    PresetLoadResult result;
    if (!file.existsAsFile() || file.isSymbolicLink() || !file.hasFileExtension("folkparkpreset")
        || file.getSize() <= 0 || file.getSize() > maximumPresetBytes)
    {
        result.status = juce::Result::fail("Preset file is missing, linked, oversized, or has the wrong extension");
        return result;
    }
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes) || bytes.getSize() == 0
        || !juce::CharPointer_UTF8::isValidString(static_cast<const char*>(bytes.getData()),
                                                  static_cast<int>(bytes.getSize())))
    {
        result.status = juce::Result::fail("Preset file is not bounded valid UTF-8");
        return result;
    }
    const auto decoded = PresetCodec::decode(
        juce::String::fromUTF8(static_cast<const char*>(bytes.getData()),
                              static_cast<int>(bytes.getSize())), migrationDefaults);
    if (decoded.status.failed())
    {
        result.status = decoded.status;
        return result;
    }
    result.document = decoded.document;
    result.canonicalJson = decoded.canonicalJson;
    result.migrated = decoded.migrated;
    const auto assets = PresetAssetStore::validate(result.document, presetRoot);
    result.missingAssets = assets.missing;
    if (assets.status.failed())
    {
        result.status = assets.status;
        return result;
    }
    result.status = juce::Result::ok();
    return result;
}

juce::Result PresetStore::save(const PresetDocument& document,
                               const juce::File& destination,
                               bool allowOverwrite)
{
    if (!destination.hasFileExtension("folkparkpreset")
        || destination.getFileName() != sanitisePresetFilename(destination.getFileName()))
        return juce::Result::fail("Preset destination filename is unsafe");
    if (destination.getParentDirectory().isSymbolicLink())
        return juce::Result::fail("Preset destination directory may not be a symbolic link");
    if (destination.existsAsFile() && !allowOverwrite)
        return juce::Result::fail("Preset destination exists and overwrite was not authorized");
    if (!destination.getParentDirectory().isDirectory()
        && !destination.getParentDirectory().createDirectory())
        return juce::Result::fail("Preset destination directory could not be created");
    const auto assetValidation = PresetAssetStore::validate(document,
                                                             destination.getParentDirectory());
    if (assetValidation.status.failed())
        return assetValidation.status;
    if (!assetValidation.missing.empty())
        return juce::Result::fail("Preset cannot be saved while referenced assets are missing");
    const auto encoded = PresetCodec::encode(document);
    if (encoded.status.failed())
        return encoded.status;

    const auto temporary = destination.getSiblingFile("." + destination.getFileName()
                                                       + "." + juce::Uuid().toDashedString() + ".tmp");
    TemporaryFileCleaner cleanup(temporary);
    {
        auto stream = temporary.createOutputStream();
        if (stream == nullptr || !stream->openedOk()
            || !stream->writeText(encoded.canonicalJson, false, false, nullptr))
            return juce::Result::fail("Preset temporary file could not be written and flushed");
        stream->flush();
        if (stream->getStatus().failed())
            return juce::Result::fail("Preset temporary file could not be written and flushed");
    }
    juce::MemoryBlock verificationBytes;
    if (!temporary.loadFileAsData(verificationBytes))
        return juce::Result::fail("Preset temporary file could not be reopened");
    const auto verified = PresetCodec::decode(
        juce::String::fromUTF8(static_cast<const char*>(verificationBytes.getData()),
                              static_cast<int>(verificationBytes.getSize())), document);
    if (verified.status.failed() || verified.canonicalJson != encoded.canonicalJson)
        return juce::Result::fail("Preset temporary file failed deterministic verification");

    const auto replaced = destination.existsAsFile()
        ? temporary.replaceFileIn(destination)
        : temporary.moveFileTo(destination);
    if (!replaced)
        return juce::Result::fail("Validated preset could not atomically replace its destination");
    cleanup.release();
    return juce::Result::ok();
}
}
