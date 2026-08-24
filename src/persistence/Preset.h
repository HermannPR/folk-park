#pragma once

#include "synth/Modulation.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace folkpark::persistence
{
inline constexpr int oldestPresetSchemaVersion = 1;
inline constexpr int currentPresetSchemaVersion = 2;
inline constexpr std::int64_t maximumPresetBytes = 1 * 1024 * 1024;
inline constexpr int maximumJsonDepth = 32;
inline constexpr int maximumJsonStringBytes = 4096;
inline constexpr int maximumJsonStructuralTokens = 8192;
inline constexpr std::int64_t maximumAssetBytes = 64 * 1024 * 1024;

struct ParameterValue
{
    juce::String id;
    float normalized = 0.0f;

    friend bool operator==(const ParameterValue&, const ParameterValue&) = default;
};

struct EffectState
{
    juce::String id;
    std::vector<ParameterValue> parameters;

    friend bool operator==(const EffectState&, const EffectState&) = default;
};

struct PresetMetadata
{
    juce::String id;
    juce::String name;
    juce::String author;
    std::vector<juce::String> tags;
    juce::String genre;
    juce::String emotion;
    juce::String description;
    bool favorite = false;

    friend bool operator==(const PresetMetadata&, const PresetMetadata&) = default;
};

enum class AssetKind : std::uint8_t
{
    wavetableSource,
    count
};

enum class AssetSlot : std::uint8_t
{
    oscillatorA,
    oscillatorB,
    count
};

struct AssetReference
{
    AssetKind kind = AssetKind::wavetableSource;
    AssetSlot slot = AssetSlot::oscillatorA;
    juce::String sha256;
    juce::String relativePath;
    std::int64_t byteSize = 0;
    juce::String recoveryDisplayName;
    juce::String originalSha256;

    friend bool operator==(const AssetReference&, const AssetReference&) = default;
};

struct PreviewMetadata
{
    juce::String sha256;
    double durationSeconds = 0.0;

    friend bool operator==(const PreviewMetadata&, const PreviewMetadata&) = default;
};

struct MigrationProvenance
{
    int originalSchemaVersion = currentPresetSchemaVersion;
    std::vector<juce::String> steps;

    friend bool operator==(const MigrationProvenance&, const MigrationProvenance&) = default;
};

struct ExtensionField
{
    juce::String name;
    juce::var value;
};

struct PresetDocument
{
    int schemaVersion = currentPresetSchemaVersion;
    juce::String productIdentifier{"com.folkpark.audio.folkpark"};
    juce::String productName{"folk park"};
    juce::String productVersion{"0.1.0"};
    PresetMetadata metadata;
    std::vector<ParameterValue> parameters;
    std::vector<synth::ModulationRoute> modulationRoutes;
    std::vector<EffectState> effects;
    std::vector<AssetReference> assets;
    std::optional<PreviewMetadata> preview;
    MigrationProvenance migration;
    std::vector<ExtensionField> unknownTopLevelFields;
};

struct PresetCodecResult
{
    juce::Result status = juce::Result::fail("Preset codec did not run");
    PresetDocument document;
    juce::String canonicalJson;
    bool migrated = false;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

struct AssetValidationResult
{
    juce::Result status = juce::Result::fail("Asset validation did not run");
    std::vector<AssetReference> missing;

    [[nodiscard]] bool ready() const noexcept { return status.wasOk() && missing.empty(); }
};

struct PresetLoadResult
{
    juce::Result status = juce::Result::fail("Preset load did not run");
    PresetDocument document;
    juce::String canonicalJson;
    std::vector<AssetReference> missingAssets;
    bool migrated = false;

    [[nodiscard]] bool readyToApply() const noexcept
    {
        return status.wasOk() && missingAssets.empty();
    }
};

[[nodiscard]] juce::String stableId(AssetKind value);
[[nodiscard]] juce::String stableId(AssetSlot value);
[[nodiscard]] PresetDocument makePresetTemplate(const juce::String& productVersion,
                                                const juce::String& presetId,
                                                const juce::String& presetName);
[[nodiscard]] juce::Result validatePreset(const PresetDocument& document);
[[nodiscard]] juce::String sanitisePresetFilename(const juce::String& requestedName);

class PresetCodec final
{
public:
    [[nodiscard]] static PresetCodecResult decode(const juce::String& json,
                                                  const PresetDocument& migrationDefaults);
    [[nodiscard]] static PresetCodecResult encode(const PresetDocument& document);
};

class PresetAssetStore final
{
public:
    [[nodiscard]] static AssetValidationResult validate(const PresetDocument& document,
                                                        const juce::File& presetRoot);
    [[nodiscard]] static juce::Result importWavetableSource(const juce::File& source,
                                                           const juce::File& presetRoot,
                                                           AssetSlot slot,
                                                           AssetReference& imported);
    [[nodiscard]] static juce::Result relink(const AssetReference& reference,
                                            const juce::File& selectedSource,
                                            const juce::File& presetRoot);
};

class PresetStore final
{
public:
    [[nodiscard]] static PresetLoadResult load(const juce::File& file,
                                              const PresetDocument& migrationDefaults,
                                              const juce::File& presetRoot);
    [[nodiscard]] static juce::Result save(const PresetDocument& document,
                                          const juce::File& destination,
                                          bool allowOverwrite);
};
}
