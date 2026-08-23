#pragma once

#include "HistoryRepository.h"
#include "Preset.h"
#include "synth/WavetableConverter.h"

#include <juce_core/juce_core.h>

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace folkpark::persistence
{
struct PersistenceConfiguration
{
    bool enabled = true;
    juce::File rootDirectory;
};

struct PresetLibrarySummary
{
    juce::String id;
    juce::String name;
    juce::String author;
    std::vector<juce::String> tags;
    juce::String genre;
    juce::String emotion;
    bool favorite = false;
    bool missingAssets = false;
    juce::String fileName;
};

struct PresetLibraryResult
{
    juce::Result status = juce::Result::fail("Preset library scan did not run");
    std::vector<PresetLibrarySummary> presets;
};

struct PresetCandidateResult
{
    juce::Result status = juce::Result::fail("Preset candidate load did not run");
    PresetDocument document;
    std::vector<AssetReference> missingAssets;
    bool migrated = false;

    [[nodiscard]] bool readyToApply() const noexcept
    {
        return status.wasOk() && missingAssets.empty();
    }
};

struct PersistenceStatusSnapshot
{
    bool enabled = false;
    bool presetAvailable = false;
    bool historyAvailable = false;
    juce::String message;
    juce::String currentPresetId;
    juce::String currentPresetName{"Init / session"};
    bool currentPresetDirty = false;
    std::vector<AssetReference> missingAssets;
    int retentionDays = 180;
};

struct PreparedPresetWavetables
{
    juce::Result status = juce::Result::fail("Preset wavetable preparation did not run");
    std::array<std::unique_ptr<synth::WavetableBank>, 2> banks;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status.wasOk() && banks[0] != nullptr && banks[1] != nullptr;
    }
};

class PersistenceCoordinator final
{
public:
    PersistenceCoordinator(PersistenceConfiguration configuration,
                           PresetDocument migrationDefaults);

    [[nodiscard]] juce::Result initialise();
    [[nodiscard]] PersistenceStatusSnapshot status() const;
    [[nodiscard]] PresetLibraryResult listPresets();
    [[nodiscard]] juce::Result savePreset(const PresetDocument& document,
                                          bool allowOverwrite);
    [[nodiscard]] PresetCandidateResult loadLibraryPreset(const juce::String& presetId);
    [[nodiscard]] PresetCandidateResult importExternalPreset(const juce::File& file);
    [[nodiscard]] PresetCandidateResult relinkPendingAsset(AssetSlot slot,
                                                           const juce::File& selectedFile);
    [[nodiscard]] juce::Result setPresetFavorite(const juce::String& presetId,
                                                 bool favorite);
    [[nodiscard]] juce::Result importWavetableSource(const juce::File& source,
                                                     AssetSlot slot,
                                                     AssetReference& imported);
    [[nodiscard]] PreparedPresetWavetables prepareWavetables(
        const PresetDocument& document);

    void markPresetApplied(const PresetDocument& document);
    void markCurrentSoundDirty(const juce::String& message);
    void report(const juce::String& message);
    [[nodiscard]] juce::String currentPresetId() const;

    [[nodiscard]] juce::Result storeHistory(const HistoryEntry& entry);
    [[nodiscard]] HistorySearchResult searchHistory(const HistorySearchQuery& query);
    [[nodiscard]] HistoryRecallResult recallHistory(const juce::String& id);
    [[nodiscard]] juce::Result setHistoryFavorite(const juce::String& id, bool favorite);
    [[nodiscard]] juce::Result setHistorySoftDeleted(const juce::String& id, bool deleted);
    [[nodiscard]] juce::Result setRetentionDays(int days);
    [[nodiscard]] HistoryCleanupResult cleanupHistory(std::int64_t nowUnixMs,
                                                      bool keepFavorites);

private:
    struct PendingPreset
    {
        PresetDocument document;
        juce::File destination;
        bool saveBeforeApply = false;
        bool migrated = false;
        std::vector<AssetReference> missingAssets;
    };

    [[nodiscard]] juce::Result initialiseLocked();
    [[nodiscard]] juce::Result requirePresetLocked();
    [[nodiscard]] juce::Result requireHistoryLocked();
    [[nodiscard]] PresetLoadResult loadPresetFileLocked(const juce::File& file,
                                                        const juce::File& assetRoot) const;
    [[nodiscard]] juce::Result localiseAvailableAssetsLocked(const PresetDocument& document,
                                                             const juce::File& sourceRoot);
    [[nodiscard]] juce::File findPresetFileByIdLocked(const juce::String& presetId,
                                                      bool& duplicate) const;
    [[nodiscard]] juce::File destinationForExternalPresetLocked(
        const PresetDocument& document) const;
    [[nodiscard]] PresetCandidateResult finishCandidateLocked(PresetDocument document,
                                                               bool migrated,
                                                               juce::File destination,
                                                               bool saveBeforeApply);
    void updateMissingStatusLocked(std::vector<AssetReference> missing,
                                   const juce::String& context);

    PersistenceConfiguration configuration;
    PresetDocument migrationDefaults;
    juce::File presetRoot;
    juce::File databaseFile;
    std::unique_ptr<HistoryRepository> history;
    mutable std::mutex mutex;
    PersistenceStatusSnapshot currentStatus;
    std::optional<PendingPreset> pendingPreset;
};
}
