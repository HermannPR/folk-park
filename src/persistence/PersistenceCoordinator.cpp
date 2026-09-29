#include "PersistenceCoordinator.h"

#include <algorithm>

namespace folkpark::persistence
{
namespace
{
constexpr int maximumLibraryEntries = 512;

juce::String shortPresetId(const juce::String& id)
{
    return id.substring(0, juce::jmin(8, id.length()));
}
}

PersistenceCoordinator::PersistenceCoordinator(PersistenceConfiguration newConfiguration,
                                               PresetDocument newMigrationDefaults)
    : configuration(std::move(newConfiguration)),
      migrationDefaults(std::move(newMigrationDefaults))
{
    currentStatus.enabled = configuration.enabled;
    currentStatus.message = configuration.enabled
        ? "Local preset and history storage has not been opened"
        : "Persistence is disabled for this isolated processor";
    if (configuration.enabled)
    {
        presetRoot = configuration.rootDirectory.getChildFile("Presets");
        databaseFile = configuration.rootDirectory.getChildFile("history.sqlite3");
    }
}

juce::Result PersistenceCoordinator::initialise()
{
    const std::lock_guard lock(mutex);
    return initialiseLocked();
}

juce::Result PersistenceCoordinator::initialiseLocked()
{
    if (!configuration.enabled)
        return juce::Result::fail("Persistence is disabled for this processor");
    if (configuration.rootDirectory == juce::File{} || configuration.rootDirectory.isSymbolicLink()
        || presetRoot.isSymbolicLink() || presetRoot.getChildFile("assets").isSymbolicLink())
    {
        currentStatus.presetAvailable = false;
        currentStatus.historyAvailable = false;
        currentStatus.message = "Local storage root is missing or uses an unsafe symbolic link";
        return juce::Result::fail(currentStatus.message);
    }
    if ((!configuration.rootDirectory.isDirectory()
         && !configuration.rootDirectory.createDirectory())
        || (!presetRoot.isDirectory() && !presetRoot.createDirectory()))
    {
        currentStatus.presetAvailable = false;
        currentStatus.historyAvailable = false;
        currentStatus.message = "Local preset directory could not be created";
        return juce::Result::fail(currentStatus.message);
    }
    currentStatus.presetAvailable = true;

    if (!currentStatus.historyAvailable)
    {
        if (databaseFile.isSymbolicLink())
        {
            history.reset();
            currentStatus.historyAvailable = false;
            currentStatus.message =
                "Presets are available; history database path uses an unsafe symbolic link";
            return juce::Result::ok();
        }
        auto candidate = std::make_unique<SqliteHistoryRepository>(databaseFile);
        const auto result = candidate->initialise();
        if (result.wasOk())
        {
            history = std::move(candidate);
            currentStatus.historyAvailable = true;
            auto days = currentStatus.retentionDays;
            if (history->retentionDays(days).wasOk())
                currentStatus.retentionDays = days;
        }
        else
        {
            history.reset();
            currentStatus.historyAvailable = false;
            currentStatus.message = "Presets are available; history database is unavailable: "
                + result.getErrorMessage();
            return juce::Result::ok();
        }
    }
    currentStatus.message = "Local presets and composition history are available";
    return juce::Result::ok();
}

juce::Result PersistenceCoordinator::requirePresetLocked()
{
    const auto result = initialiseLocked();
    if (result.failed() || !currentStatus.presetAvailable)
        return result.failed() ? result : juce::Result::fail("Preset storage is unavailable");
    return juce::Result::ok();
}

juce::Result PersistenceCoordinator::requireHistoryLocked()
{
    const auto result = initialiseLocked();
    if (result.failed())
        return result;
    if (!currentStatus.historyAvailable || history == nullptr)
        return juce::Result::fail("History database is unavailable");
    return juce::Result::ok();
}

PersistenceStatusSnapshot PersistenceCoordinator::status() const
{
    const std::lock_guard lock(mutex);
    return currentStatus;
}

PresetLoadResult PersistenceCoordinator::loadPresetFileLocked(const juce::File& file,
                                                              const juce::File& assetRoot) const
{
    return PresetStore::load(file, migrationDefaults, assetRoot);
}

juce::File PersistenceCoordinator::findPresetFileByIdLocked(const juce::String& presetId,
                                                            bool& duplicate) const
{
    duplicate = false;
    juce::File match;
    auto count = 0;
    for (const auto& entry : juce::RangedDirectoryIterator(
             presetRoot, false, "*.folkparkpreset", juce::File::findFiles))
    {
        if (++count > maximumLibraryEntries)
            break;
        const auto loaded = loadPresetFileLocked(entry.getFile(), presetRoot);
        if (loaded.status.wasOk() && loaded.document.metadata.id == presetId)
        {
            if (match != juce::File{})
            {
                duplicate = true;
                return {};
            }
            match = entry.getFile();
        }
    }
    return match;
}

PresetLibraryResult PersistenceCoordinator::listPresets()
{
    const std::lock_guard lock(mutex);
    PresetLibraryResult result;
    if (const auto availability = requirePresetLocked(); availability.failed())
    {
        result.status = availability;
        return result;
    }
    auto count = 0;
    for (const auto& entry : juce::RangedDirectoryIterator(
             presetRoot, false, "*.folkparkpreset", juce::File::findFiles))
    {
        if (++count > maximumLibraryEntries)
            break;
        const auto loaded = loadPresetFileLocked(entry.getFile(), presetRoot);
        if (loaded.status.failed())
            continue;
        const auto& metadata = loaded.document.metadata;
        result.presets.push_back({metadata.id, metadata.name, metadata.author, metadata.tags,
                                  metadata.genre, metadata.emotion, metadata.favorite,
                                  !loaded.missingAssets.empty(), entry.getFile().getFileName()});
    }
    std::sort(result.presets.begin(), result.presets.end(), [](const auto& left, const auto& right)
    {
        const auto byName = left.name.compareIgnoreCase(right.name);
        return byName == 0 ? left.id < right.id : byName < 0;
    });
    result.status = juce::Result::ok();
    return result;
}

juce::Result PersistenceCoordinator::savePreset(const PresetDocument& document,
                                                bool allowOverwrite)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requirePresetLocked(); availability.failed())
        return availability;
    bool duplicate = false;
    auto destination = findPresetFileByIdLocked(document.metadata.id, duplicate);
    if (duplicate)
        return juce::Result::fail("Preset library contains a duplicate stable ID");
    if (destination == juce::File{})
        destination = presetRoot.getChildFile(sanitisePresetFilename(document.metadata.name));
    if (destination.existsAsFile())
    {
        const auto existing = loadPresetFileLocked(destination, presetRoot);
        if (existing.status.failed() || existing.document.metadata.id != document.metadata.id)
            return juce::Result::fail("Another preset already owns that safe filename");
    }
    const auto result = PresetStore::save(document, destination, allowOverwrite);
    currentStatus.message = result.wasOk() ? "Preset saved atomically to the local library"
                                           : result.getErrorMessage();
    if (result.wasOk())
    {
        currentStatus.currentPresetId = document.metadata.id;
        currentStatus.currentPresetName = document.metadata.name;
        currentStatus.currentPresetDirty = false;
        currentStatus.missingAssets.clear();
    }
    return result;
}

juce::Result PersistenceCoordinator::localiseAvailableAssetsLocked(
    const PresetDocument& document, const juce::File& sourceRoot)
{
    for (const auto& reference : document.assets)
    {
        const auto installed = presetRoot.getChildFile(reference.relativePath);
        if (installed.existsAsFile())
            continue;
        const auto source = sourceRoot.getChildFile(reference.relativePath);
        if (!source.existsAsFile())
            continue;
        if (const auto result = PresetAssetStore::relink(reference, source, presetRoot);
            result.failed())
            return result;
    }
    return juce::Result::ok();
}

juce::File PersistenceCoordinator::destinationForExternalPresetLocked(
    const PresetDocument& document) const
{
    auto destination = presetRoot.getChildFile(sanitisePresetFilename(document.metadata.name));
    if (!destination.existsAsFile())
        return destination;
    const auto stem = sanitisePresetFilename(document.metadata.name)
                          .dropLastCharacters(juce::String(".folkparkpreset").length());
    return presetRoot.getChildFile(sanitisePresetFilename(
        stem + "-" + shortPresetId(document.metadata.id)));
}

void PersistenceCoordinator::updateMissingStatusLocked(std::vector<AssetReference> missing,
                                                       const juce::String& context)
{
    currentStatus.missingAssets = std::move(missing);
    currentStatus.message = currentStatus.missingAssets.empty()
        ? context
        : context + ": select the matching WAV for each missing oscillator asset";
}

PresetCandidateResult PersistenceCoordinator::finishCandidateLocked(
    PresetDocument document, bool migrated, juce::File destination, bool saveBeforeApply)
{
    PresetCandidateResult result;
    result.document = document;
    result.migrated = migrated;
    const auto assets = PresetAssetStore::validate(document, presetRoot);
    if (assets.status.failed())
    {
        result.status = assets.status;
        currentStatus.message = assets.status.getErrorMessage();
        return result;
    }
    result.missingAssets = assets.missing;
    if (!result.missingAssets.empty())
    {
        pendingPreset = PendingPreset{std::move(document), destination, saveBeforeApply,
                                      migrated, result.missingAssets};
        updateMissingStatusLocked(result.missingAssets, "Preset is valid but its asset is missing");
        result.status = juce::Result::ok();
        return result;
    }
    if (saveBeforeApply)
    {
        if (destination.existsAsFile())
        {
            result.status = juce::Result::fail(
                "This external preset is already present; load the local library copy instead");
            currentStatus.message = result.status.getErrorMessage();
            return result;
        }
        if (const auto saved = PresetStore::save(document, destination, false); saved.failed())
        {
            result.status = saved;
            currentStatus.message = saved.getErrorMessage();
            return result;
        }
    }
    pendingPreset.reset();
    currentStatus.missingAssets.clear();
    currentStatus.message = migrated ? "Legacy preset migrated and prepared transactionally"
                                     : "Preset prepared transactionally";
    result.status = juce::Result::ok();
    return result;
}

PresetCandidateResult PersistenceCoordinator::loadLibraryPreset(const juce::String& presetId)
{
    const std::lock_guard lock(mutex);
    PresetCandidateResult result;
    pendingPreset.reset();
    currentStatus.missingAssets.clear();
    if (!midi::isUuid(presetId))
    {
        result.status = juce::Result::fail("Preset ID is malformed");
        return result;
    }
    if (const auto availability = requirePresetLocked(); availability.failed())
    {
        result.status = availability;
        return result;
    }
    bool duplicate = false;
    const auto file = findPresetFileByIdLocked(presetId, duplicate);
    if (duplicate || file == juce::File{})
    {
        result.status = juce::Result::fail(duplicate ? "Preset ID is duplicated in the local library"
                                                     : "Preset was not found in the local library");
        return result;
    }
    const auto loaded = loadPresetFileLocked(file, presetRoot);
    if (loaded.status.failed())
    {
        result.status = loaded.status;
        currentStatus.message = loaded.status.getErrorMessage();
        return result;
    }
    return finishCandidateLocked(loaded.document, loaded.migrated, file, false);
}

PresetCandidateResult PersistenceCoordinator::importExternalPreset(const juce::File& file)
{
    const std::lock_guard lock(mutex);
    PresetCandidateResult result;
    pendingPreset.reset();
    currentStatus.missingAssets.clear();
    if (const auto availability = requirePresetLocked(); availability.failed())
    {
        result.status = availability;
        return result;
    }
    const auto loaded = loadPresetFileLocked(file, file.getParentDirectory());
    if (loaded.status.failed())
    {
        result.status = loaded.status;
        currentStatus.message = loaded.status.getErrorMessage();
        return result;
    }
    if (const auto localised = localiseAvailableAssetsLocked(
            loaded.document, file.getParentDirectory()); localised.failed())
    {
        result.status = localised;
        currentStatus.message = localised.getErrorMessage();
        return result;
    }
    return finishCandidateLocked(loaded.document, loaded.migrated,
                                 destinationForExternalPresetLocked(loaded.document), true);
}

PresetCandidateResult PersistenceCoordinator::prepareSessionPresetJson(const juce::String& json)
{
    const std::lock_guard lock(mutex);
    PresetCandidateResult result;
    pendingPreset.reset();
    currentStatus.missingAssets.clear();
    const auto decoded = PresetCodec::decode(json, migrationDefaults);
    if (decoded.status.failed())
    {
        result.status = decoded.status;
        currentStatus.message = decoded.status.getErrorMessage();
        return result;
    }
    if (!decoded.document.assets.empty())
    {
        if (const auto availability = requirePresetLocked(); availability.failed())
        {
            result.status = availability;
            return result;
        }
    }
    return finishCandidateLocked(decoded.document, decoded.migrated, {}, false);
}

PresetCandidateResult PersistenceCoordinator::relinkPendingAsset(
    AssetSlot slot, const juce::File& selectedFile)
{
    const std::lock_guard lock(mutex);
    PresetCandidateResult result;
    if (const auto availability = requirePresetLocked(); availability.failed())
    {
        result.status = availability;
        return result;
    }
    if (!pendingPreset.has_value())
    {
        result.status = juce::Result::fail("No preset is waiting for missing-asset recovery");
        return result;
    }
    const auto found = std::find_if(pendingPreset->missingAssets.begin(),
                                    pendingPreset->missingAssets.end(),
                                    [slot](const auto& reference) { return reference.slot == slot; });
    if (found == pendingPreset->missingAssets.end())
    {
        result.status = juce::Result::fail("That oscillator is not missing an asset");
        return result;
    }
    if (const auto relinked = PresetAssetStore::relink(*found, selectedFile, presetRoot);
        relinked.failed())
    {
        result.status = relinked;
        currentStatus.message = relinked.getErrorMessage();
        return result;
    }
    auto pending = *pendingPreset;
    return finishCandidateLocked(std::move(pending.document), pending.migrated,
                                 pending.destination, pending.saveBeforeApply);
}

juce::Result PersistenceCoordinator::setPresetFavorite(const juce::String& presetId,
                                                       bool favorite)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requirePresetLocked(); availability.failed())
        return availability;
    bool duplicate = false;
    const auto file = findPresetFileByIdLocked(presetId, duplicate);
    if (duplicate || file == juce::File{})
        return juce::Result::fail("Preset was not found uniquely in the local library");
    auto loaded = loadPresetFileLocked(file, presetRoot);
    if (!loaded.readyToApply())
        return loaded.status.failed() ? loaded.status
                                      : juce::Result::fail("Relink missing assets before favoriting this preset");
    loaded.document.metadata.favorite = favorite;
    const auto result = PresetStore::save(loaded.document, file, true);
    currentStatus.message = result.wasOk() ? "Preset favorite updated atomically"
                                           : result.getErrorMessage();
    return result;
}

juce::Result PersistenceCoordinator::importWavetableSource(const juce::File& source,
                                                           AssetSlot slot,
                                                           AssetReference& imported)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requirePresetLocked(); availability.failed())
        return availability;
    const auto result = PresetAssetStore::importWavetableSource(source, presetRoot, slot, imported);
    currentStatus.message = result.wasOk() ? "Confirmed WAV retained in content-addressed storage"
                                           : result.getErrorMessage();
    return result;
}

PreparedPresetWavetables PersistenceCoordinator::prepareWavetables(
    const PresetDocument& document)
{
    const std::lock_guard lock(mutex);
    PreparedPresetWavetables prepared;
    if (!document.assets.empty())
    {
        if (const auto availability = requirePresetLocked(); availability.failed())
        {
            prepared.status = availability;
            return prepared;
        }
    }
    const auto assets = PresetAssetStore::validate(document, presetRoot);
    if (assets.status.failed() || !assets.missing.empty())
    {
        prepared.status = assets.status.failed()
            ? assets.status
            : juce::Result::fail("Preset assets must be relinked before sound preparation");
        return prepared;
    }
    prepared.banks[0] = synth::WavetableBank::createBuiltIn();
    prepared.banks[1] = synth::WavetableBank::createBuiltIn();
    if (prepared.banks[0] == nullptr || prepared.banks[1] == nullptr)
    {
        prepared.status = juce::Result::fail("Built-in wavetable fallback could not be created");
        return prepared;
    }
    const synth::WavetableConverter converter;
    for (const auto& reference : document.assets)
    {
        auto converted = converter.convertWavFile(
            presetRoot.getChildFile(reference.relativePath));
        if (!converted.succeeded())
        {
            prepared.status = juce::Result::fail("Preset wavetable could not be converted: "
                                                  + converted.status.getErrorMessage());
            return prepared;
        }
        prepared.banks[reference.slot == AssetSlot::oscillatorA ? 0U : 1U]
            = std::move(converted.bank);
    }
    prepared.status = juce::Result::ok();
    return prepared;
}

void PersistenceCoordinator::markPresetApplied(const PresetDocument& document)
{
    const std::lock_guard lock(mutex);
    currentStatus.currentPresetId = document.metadata.id;
    currentStatus.currentPresetName = document.metadata.name;
    currentStatus.currentPresetDirty = false;
    currentStatus.missingAssets.clear();
    currentStatus.message = "Preset applied at a safe audio-block boundary";
    pendingPreset.reset();
}

void PersistenceCoordinator::restoreSessionStatus(const PresetDocument& document, bool dirty)
{
    const std::lock_guard lock(mutex);
    currentStatus.currentPresetId = document.metadata.id;
    currentStatus.currentPresetName = document.metadata.name;
    currentStatus.currentPresetDirty = dirty;
    currentStatus.message = dirty
        ? "Host project restored a modified native sound snapshot"
        : "Host project restored the saved native sound snapshot";
}

void PersistenceCoordinator::markCurrentSoundDirty(const juce::String& message)
{
    const std::lock_guard lock(mutex);
    currentStatus.currentPresetDirty = true;
    currentStatus.message = message;
}

void PersistenceCoordinator::report(const juce::String& message)
{
    const std::lock_guard lock(mutex);
    currentStatus.message = message.substring(0, 512);
}

juce::String PersistenceCoordinator::currentPresetId() const
{
    const std::lock_guard lock(mutex);
    return currentStatus.currentPresetId;
}

juce::Result PersistenceCoordinator::storeHistory(const HistoryEntry& entry)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
    {
        currentStatus.message = availability.getErrorMessage();
        return availability;
    }
    const auto result = history->store(entry);
    currentStatus.message = result.wasOk() ? "Accepted composition stored in local history"
                                           : "Composition accepted, but history was not stored: "
                                               + result.getErrorMessage();
    return result;
}

HistorySearchResult PersistenceCoordinator::searchHistory(const HistorySearchQuery& query)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return {availability, {}};
    return history->search(query);
}

HistoryRecallResult PersistenceCoordinator::recallHistory(const juce::String& id)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return {availability, {}};
    return history->recall(id);
}

juce::Result PersistenceCoordinator::setHistoryFavorite(const juce::String& id, bool favorite)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return availability;
    const auto result = history->setFavorite(id, favorite, juce::Time::currentTimeMillis());
    currentStatus.message = result.wasOk() ? "History favorite updated" : result.getErrorMessage();
    return result;
}

juce::Result PersistenceCoordinator::setHistorySoftDeleted(const juce::String& id, bool deleted)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return availability;
    const auto result = history->setSoftDeleted(id, deleted, juce::Time::currentTimeMillis());
    currentStatus.message = result.wasOk() ? (deleted ? "History entry moved to recoverable trash"
                                                       : "History entry restored")
                                           : result.getErrorMessage();
    return result;
}

juce::Result PersistenceCoordinator::setRetentionDays(int days)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return availability;
    const auto result = history->setRetentionDays(days);
    if (result.wasOk())
        currentStatus.retentionDays = days;
    currentStatus.message = result.wasOk() ? "History retention preference saved"
                                           : result.getErrorMessage();
    return result;
}

HistoryCleanupResult PersistenceCoordinator::cleanupHistory(std::int64_t nowUnixMs,
                                                            bool keepFavorites)
{
    const std::lock_guard lock(mutex);
    if (const auto availability = requireHistoryLocked(); availability.failed())
        return {availability, 0};
    constexpr std::int64_t millisecondsPerDay = 24LL * 60LL * 60LL * 1000LL;
    const auto cutoff = nowUnixMs
        - static_cast<std::int64_t>(currentStatus.retentionDays) * millisecondsPerDay;
    const auto result = history->cleanupOlderThan(cutoff, keepFavorites);
    currentStatus.message = result.status.wasOk()
        ? juce::String(result.removedCount) + " expired history entries permanently cleaned"
        : result.status.getErrorMessage();
    return result;
}
}
