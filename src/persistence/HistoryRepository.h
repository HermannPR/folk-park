#pragma once

#include "midi/Composition.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <span>
#include <vector>

namespace folkpark::persistence
{
struct HistoryEntry
{
    static constexpr int currentSchemaVersion = 1;

    int schemaVersion = currentSchemaVersion;
    juce::String id;
    juce::String parentId;
    std::int64_t createdUnixMs = 0;
    std::int64_t updatedUnixMs = 0;
    juce::String generatorVersion;
    bool storePromptSummary = false;
    juce::String promptSummary;
    midi::MusicIntent macroSnapshot;
    midi::CompositionBundle composition;
    juce::String presetId;
    bool favorite = false;
    std::vector<juce::String> tags;
    bool deleted = false;
};

struct HistorySummary
{
    int schemaVersion = HistoryEntry::currentSchemaVersion;
    juce::String id;
    juce::String parentId;
    std::int64_t createdUnixMs = 0;
    std::int64_t updatedUnixMs = 0;
    juce::String generatorVersion;
    bool storePromptSummary = false;
    juce::String promptSummary;
    juce::String presetId;
    bool favorite = false;
    std::vector<juce::String> tags;
    bool deleted = false;
};

struct HistorySearchQuery
{
    juce::String text;
    bool favoritesOnly = false;
    bool includeDeleted = false;
    int limit = 50;
};

struct HistorySearchResult
{
    juce::Result status = juce::Result::fail("History search did not run");
    std::vector<HistorySummary> entries;
};

struct HistoryRecallResult
{
    juce::Result status = juce::Result::fail("History recall did not run");
    HistoryEntry entry;

    [[nodiscard]] bool succeeded() const noexcept { return status.wasOk(); }
};

struct HistoryCleanupResult
{
    juce::Result status = juce::Result::fail("History cleanup did not run");
    int removedCount = 0;
};

[[nodiscard]] juce::Result validateHistoryEntry(const HistoryEntry& entry);
[[nodiscard]] juce::Result validateHistoryTags(std::span<const juce::String> tags);

class HistoryRepository
{
public:
    virtual ~HistoryRepository() = default;

    [[nodiscard]] virtual juce::Result initialise() = 0;
    [[nodiscard]] virtual int schemaVersion() const noexcept = 0;
    [[nodiscard]] virtual juce::Result store(const HistoryEntry& entry) = 0;
    [[nodiscard]] virtual HistoryRecallResult recall(const juce::String& id) = 0;
    [[nodiscard]] virtual HistorySearchResult search(const HistorySearchQuery& query) = 0;
    [[nodiscard]] virtual HistorySearchResult childrenOf(const juce::String& parentId,
                                                         int limit = 50) = 0;
    [[nodiscard]] virtual juce::Result setFavorite(const juce::String& id,
                                                   bool favorite,
                                                   std::int64_t updatedUnixMs) = 0;
    [[nodiscard]] virtual juce::Result setTags(const juce::String& id,
                                              std::span<const juce::String> tags,
                                              std::int64_t updatedUnixMs) = 0;
    [[nodiscard]] virtual juce::Result setSoftDeleted(const juce::String& id,
                                                     bool deleted,
                                                     std::int64_t updatedUnixMs) = 0;
    [[nodiscard]] virtual juce::Result setRetentionDays(int days) = 0;
    [[nodiscard]] virtual juce::Result retentionDays(int& days) = 0;
    [[nodiscard]] virtual HistoryCleanupResult cleanupOlderThan(std::int64_t cutoffUnixMs,
                                                               bool keepFavorites) = 0;
};

class SqliteHistoryRepository final : public HistoryRepository
{
public:
    explicit SqliteHistoryRepository(juce::File databaseFile);
    ~SqliteHistoryRepository() override;

    SqliteHistoryRepository(const SqliteHistoryRepository&) = delete;
    SqliteHistoryRepository& operator=(const SqliteHistoryRepository&) = delete;

    [[nodiscard]] juce::Result initialise() override;
    [[nodiscard]] int schemaVersion() const noexcept override;
    [[nodiscard]] juce::Result store(const HistoryEntry& entry) override;
    [[nodiscard]] HistoryRecallResult recall(const juce::String& id) override;
    [[nodiscard]] HistorySearchResult search(const HistorySearchQuery& query) override;
    [[nodiscard]] HistorySearchResult childrenOf(const juce::String& parentId,
                                                 int limit = 50) override;
    [[nodiscard]] juce::Result setFavorite(const juce::String& id,
                                           bool favorite,
                                           std::int64_t updatedUnixMs) override;
    [[nodiscard]] juce::Result setTags(const juce::String& id,
                                      std::span<const juce::String> tags,
                                      std::int64_t updatedUnixMs) override;
    [[nodiscard]] juce::Result setSoftDeleted(const juce::String& id,
                                             bool deleted,
                                             std::int64_t updatedUnixMs) override;
    [[nodiscard]] juce::Result setRetentionDays(int days) override;
    [[nodiscard]] juce::Result retentionDays(int& days) override;
    [[nodiscard]] HistoryCleanupResult cleanupOlderThan(std::int64_t cutoffUnixMs,
                                                       bool keepFavorites) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
