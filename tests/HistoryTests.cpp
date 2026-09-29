#include "midi/Composition.h"
#include "persistence/CompositionJson.h"
#include "persistence/HistoryRepository.h"

#include <juce_core/juce_core.h>
#include <sqlite3.h>

#include <iostream>

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

struct TemporaryDirectory
{
    TemporaryDirectory()
    {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("folk-park-history-tests", {}, false);
        expect(directory.createDirectory(), "Temporary history test directory must be created");
    }

    ~TemporaryDirectory()
    {
        if (directory.isAChildOf(juce::File::getSpecialLocation(juce::File::tempDirectory)))
            directory.deleteRecursively(false);
    }

    juce::File directory;
};

folkpark::midi::CompositionBundle makeBundle(std::uint32_t seed, std::int64_t createdUnixMs)
{
    folkpark::midi::MusicIntent intent;
    intent.seed = seed;
    intent.requestId = folkpark::midi::deterministicUuid(seed, "history-request");
    const folkpark::midi::CompositionEngine engine;
    const auto generated = engine.generate(intent, createdUnixMs);
    expect(generated.succeeded(), "History fixture composition must generate");
    return generated.bundle;
}

folkpark::persistence::HistoryEntry makeEntry(std::uint32_t seed,
                                              std::int64_t timestamp,
                                              const juce::String& parent = {})
{
    folkpark::persistence::HistoryEntry entry;
    entry.id = folkpark::midi::deterministicUuid(seed, "history-entry");
    entry.parentId = parent;
    entry.createdUnixMs = timestamp;
    entry.updatedUnixMs = timestamp;
    entry.generatorVersion = folkpark::midi::compositionGeneratorVersion;
    entry.storePromptSummary = true;
    entry.promptSummary = "dark melodic techno progression";
    entry.composition = makeBundle(seed, timestamp);
    entry.macroSnapshot = entry.composition.intent;
    entry.tags = {"dark", "techno"};
    return entry;
}

int scalarInt(const juce::File& databaseFile, const char* sql)
{
    sqlite3* database = nullptr;
    if (sqlite3_open_v2(databaseFile.getFullPathName().toRawUTF8(), &database,
                        SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
    {
        if (database != nullptr) sqlite3_close(database);
        return -1;
    }
    sqlite3_stmt* statement = nullptr;
    int value = -1;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK
        && sqlite3_step(statement) == SQLITE_ROW)
        value = sqlite3_column_int(statement, 0);
    if (statement != nullptr) sqlite3_finalize(statement);
    sqlite3_close(database);
    return value;
}

bool executeSql(const juce::File& databaseFile, const char* sql)
{
    sqlite3* database = nullptr;
    if (sqlite3_open_v2(databaseFile.getFullPathName().toRawUTF8(), &database,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        if (database != nullptr) sqlite3_close(database);
        return false;
    }
    const auto code = sqlite3_exec(database, sql, nullptr, nullptr, nullptr);
    sqlite3_close(database);
    return code == SQLITE_OK;
}

void testCompositionPayloadCodec()
{
    const auto bundle = makeBundle(7001, 1000);
    const auto encoded = folkpark::persistence::encodeCompositionJson(bundle);
    const auto decoded = folkpark::persistence::decodeCompositionJson(encoded.json);
    expect(encoded.succeeded() && decoded.succeeded(),
           "Versioned composition history payload must encode and decode");
    expect(decoded.json == encoded.json,
           "Composition history payload recall must be byte-canonical");
    const auto intent = folkpark::persistence::encodeMusicIntentJson(bundle.intent);
    const auto recalledIntent = folkpark::persistence::decodeMusicIntentJson(intent.json);
    expect(intent.succeeded() && recalledIntent.succeeded() && recalledIntent.json == intent.json,
           "Macro snapshot must round-trip exactly");
    expect(folkpark::persistence::decodeCompositionJson("{broken").status.failed(),
           "Malformed history composition payload must be rejected");
    const auto duplicate = encoded.json.replaceFirstOccurrenceOf(
        "\"schemaVersion\": 1", "\"schemaVersion\": 1, \"schemaVersion\": 1");
    expect(folkpark::persistence::decodeCompositionJson(duplicate).status.failed(),
           "Duplicate history JSON keys must be rejected before parser overwrite");
}

void testRepositoryRecallSearchAndLineage()
{
    using namespace folkpark::persistence;
    TemporaryDirectory temporary;
    const auto databaseFile = temporary.directory.getChildFile("history.sqlite3");
    SqliteHistoryRepository repository(databaseFile);
    expect(repository.initialise().wasOk() && repository.schemaVersion() == 2,
           "Fresh history repository must migrate transactionally to schema 2");
    int retention = 0;
    expect(repository.retentionDays(retention).wasOk() && retention == 180,
           "Fresh history repository must expose the bounded default retention preference");
    expect(repository.setRetentionDays(365).wasOk()
               && repository.retentionDays(retention).wasOk() && retention == 365,
           "History retention preference must persist transactionally");

    auto parent = makeEntry(8001, 1000);
    expect(repository.store(parent).wasOk(), "Valid parent history entry must store");
    expect(repository.store(parent).failed(), "Duplicate stable history UUID must be rejected");
    auto child = makeEntry(8002, 2000, parent.id);
    child.promptSummary = "bright variation of dark techno";
    child.tags = {"bright", "variation"};
    expect(repository.store(child).wasOk(), "Valid variation lineage entry must store");

    const auto recall = repository.recall(parent.id);
    expect(recall.succeeded(), "Stored history entry must recall");
    expect(encodeCompositionJson(recall.entry.composition).json
               == encodeCompositionJson(parent.composition).json,
           "Recalled composition must be version-correct and event-exact");
    expect(encodeMusicIntentJson(recall.entry.macroSnapshot).json
               == encodeMusicIntentJson(parent.macroSnapshot).json,
           "Recalled macro snapshot must be exact");
    expect(recall.entry.promptSummary == parent.promptSummary && recall.entry.tags == parent.tags,
           "Recalled privacy-approved summary and tags must be preserved");

    const auto lineage = repository.childrenOf(parent.id);
    expect(lineage.status.wasOk() && lineage.entries.size() == 1
               && lineage.entries.front().id == child.id,
           "History lineage query must return the stable child record");
    const auto search = repository.search({"variation", false, false, 20});
    expect(search.status.wasOk() && search.entries.size() == 1
               && search.entries.front().id == child.id,
           "Bounded history search must find prompt/tag metadata");
    expect(repository.search({"%_", false, false, 20}).entries.empty(),
           "History LIKE wildcards from user text must be escaped");

    expect(repository.setFavorite(parent.id, true, 3000).wasOk(),
           "Favorite update must commit transactionally");
    const std::vector<juce::String> updatedTags{"favorite", "lead"};
    expect(repository.setTags(parent.id, updatedTags, 3100).wasOk(),
           "Tag update must commit transactionally");
    const auto favorites = repository.search({"", true, false, 20});
    expect(favorites.status.wasOk() && favorites.entries.size() == 1
               && favorites.entries.front().favorite
               && favorites.entries.front().tags == updatedTags,
           "Favorite-only search must return updated canonical tags");

    expect(repository.setSoftDeleted(child.id, true, 3200).wasOk(),
           "Soft deletion must commit transactionally");
    expect(repository.search({"", false, false, 20}).entries.size() == 1,
           "Default history search must exclude soft-deleted records");
    expect(repository.search({"", false, true, 20}).entries.size() == 2,
           "Explicit recovery search must include soft-deleted records");

    const auto cleanup = repository.cleanupOlderThan(4000, true);
    expect(cleanup.status.wasOk() && cleanup.removedCount == 1,
           "Retention cleanup must remove old non-favorites and retain favorites");
    expect(repository.recall(parent.id).succeeded() && repository.recall(child.id).status.failed(),
           "Retention cleanup must preserve the favorite and remove the old variation");
}

void testPrivacyTransactionsAndFailureIsolation()
{
    using namespace folkpark::persistence;
    TemporaryDirectory temporary;
    const auto databaseFile = temporary.directory.getChildFile("privacy.sqlite3");
    SqliteHistoryRepository repository(databaseFile);
    expect(repository.initialise().wasOk(), "Privacy history repository must initialize");

    auto privateEntry = makeEntry(9001, 1000);
    privateEntry.storePromptSummary = false;
    privateEntry.promptSummary.clear();
    expect(repository.store(privateEntry).wasOk(),
           "History entry with prompt storage disabled must store without prompt text");
    expect(scalarInt(databaseFile,
                     "SELECT prompt_summary IS NULL FROM history_entries LIMIT 1;") == 1,
           "Privacy-disabled prompt summary must be SQL NULL, not hidden serialized text");
    auto leaking = makeEntry(9002, 1100);
    leaking.storePromptSummary = false;
    expect(repository.store(leaking).failed(),
           "Repository must reject prompt text when privacy storage is disabled");

    auto orphan = makeEntry(9003, 1200, folkpark::midi::deterministicUuid(1, "missing-parent"));
    expect(repository.store(orphan).failed(),
           "Foreign-key lineage failure must reject an orphan transaction");
    expect(repository.search({"", false, true, 20}).entries.size() == 1,
           "Failed orphan transaction must not partially insert a record");

    const auto callerCopy = encodeCompositionJson(privateEntry.composition).json;
    expect(executeSql(databaseFile,
                      "UPDATE history_entries SET clip_json='{broken' WHERE id IS NOT NULL;"),
           "Corrupt-payload fixture must update the isolated test database");
    expect(repository.recall(privateEntry.id).status.failed(),
           "Corrupt database payload must fail recall without throwing or partial output");
    expect(encodeCompositionJson(privateEntry.composition).json == callerCopy,
           "Database recall failure must not mutate the active caller composition snapshot");

    const auto corruptFile = temporary.directory.getChildFile("corrupt.sqlite3");
    expect(corruptFile.replaceWithText("not a database", false, false, "\n"),
           "Corrupt database fixture must be created");
    SqliteHistoryRepository corrupt(corruptFile);
    expect(corrupt.initialise().failed(),
           "Corrupt database must report unavailable without affecting composition state");
    const auto blocker = temporary.directory.getChildFile("blocker");
    expect(blocker.replaceWithText("file", false, false, "\n"),
           "Unavailable-parent fixture must be created");
    SqliteHistoryRepository unavailable(blocker.getChildFile("history.sqlite3"));
    expect(unavailable.initialise().failed(),
           "Database path below a regular file must fail safely");
}

void testVersionOneDatabaseMigration()
{
    using namespace folkpark::persistence;
    TemporaryDirectory temporary;
    const auto databaseFile = temporary.directory.getChildFile("legacy.sqlite3");
    expect(executeSql(databaseFile, R"sql(
        CREATE TABLE history_entries (
            id TEXT PRIMARY KEY NOT NULL,
            parent_id TEXT REFERENCES history_entries(id) ON DELETE SET NULL,
            created_ms INTEGER NOT NULL,
            updated_ms INTEGER NOT NULL,
            entry_schema_version INTEGER NOT NULL,
            generator_version TEXT NOT NULL,
            prompt_summary TEXT,
            prompt_stored INTEGER NOT NULL,
            macro_json TEXT NOT NULL,
            clip_json TEXT NOT NULL
        );
        CREATE INDEX history_entries_created_idx ON history_entries(created_ms DESC);
        CREATE INDEX history_entries_parent_idx ON history_entries(parent_id);
        PRAGMA user_version=1;
    )sql"), "Version-one history database fixture must be created");
    SqliteHistoryRepository repository(databaseFile);
    expect(repository.initialise().wasOk() && repository.schemaVersion() == 2,
           "Oldest supported database must migrate to schema 2");
    int retention = 0;
    expect(repository.retentionDays(retention).wasOk() && retention == 180,
           "Database migration must create the retention preference exactly once");
    expect(repository.store(makeEntry(10001, 5000)).wasOk(),
           "Migrated database must accept and recall current history records");

    const auto brokenFile = temporary.directory.getChildFile("broken-migration.sqlite3");
    expect(executeSql(brokenFile, "PRAGMA user_version=1;"),
           "Broken migration fixture must declare the oldest schema version");
    SqliteHistoryRepository broken(brokenFile);
    expect(broken.initialise().failed() && scalarInt(brokenFile, "PRAGMA user_version;") == 1,
           "Failed database migration must roll back and retain its previous schema version");

    const auto futureFile = temporary.directory.getChildFile("future.sqlite3");
    expect(executeSql(futureFile, "PRAGMA user_version=99;"),
           "Future database fixture must be created");
    SqliteHistoryRepository future(futureFile);
    expect(future.initialise().failed(),
           "Unsupported future database schema must fail without downgrade");
}
}

int main()
{
    testCompositionPayloadCodec();
    testRepositoryRecallSearchAndLineage();
    testPrivacyTransactionsAndFailureIsolation();
    testVersionOneDatabaseMigration();
    if (failures == 0)
    {
        std::cout << "folk park history repository tests passed\n";
        return 0;
    }
    std::cerr << failures << " history repository test(s) failed\n";
    return 1;
}
