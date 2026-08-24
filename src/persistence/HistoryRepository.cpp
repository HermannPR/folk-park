#include "HistoryRepository.h"

#include "CompositionJson.h"

#include <sqlite3.h>

#include <algorithm>
#include <mutex>
#include <set>

namespace folkpark::persistence
{
namespace
{
constexpr int currentDatabaseSchemaVersion = 2;
constexpr int maximumRetentionDays = 3650;
constexpr int maximumSearchResults = 200;
constexpr std::int64_t maximumDatabaseBytes = 512LL * 1024LL * 1024LL;

bool boundedText(const juce::String& text, int maximumLength, bool allowEmpty = true)
{
    if (text.length() > maximumLength || (!allowEmpty && text.trim().isEmpty()))
        return false;
    for (const auto character : text)
        if (character < 0x20 && character != '\t' && character != '\n')
            return false;
    return true;
}

juce::Result sqliteFailure(sqlite3* database, const juce::String& prefix, int code = SQLITE_ERROR)
{
    const auto* detail = database != nullptr ? sqlite3_errmsg(database) : sqlite3_errstr(code);
    return juce::Result::fail(prefix + " (SQLite " + juce::String(code) + ": "
                              + juce::String::fromUTF8(detail != nullptr ? detail : "unknown") + ")");
}

juce::Result execute(sqlite3* database, const char* sql)
{
    char* error = nullptr;
    const auto code = sqlite3_exec(database, sql, nullptr, nullptr, &error);
    if (code == SQLITE_OK)
        return juce::Result::ok();
    const auto detail = error != nullptr ? juce::String::fromUTF8(error)
                                         : juce::String::fromUTF8(sqlite3_errmsg(database));
    sqlite3_free(error);
    return juce::Result::fail("SQLite statement failed (" + juce::String(code) + "): " + detail);
}

class Statement final
{
public:
    Statement(sqlite3* database, const char* sql) : db(database)
    {
        status = sqlite3_prepare_v2(db, sql, -1, &statement, nullptr);
    }

    ~Statement()
    {
        if (statement != nullptr)
            sqlite3_finalize(statement);
    }

    [[nodiscard]] bool prepared() const noexcept { return status == SQLITE_OK; }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement; }
    [[nodiscard]] int code() const noexcept { return status; }

    [[nodiscard]] bool bindText(int index, const juce::String& value)
    {
        const auto utf8 = value.toRawUTF8();
        return sqlite3_bind_text(statement, index, utf8,
                                 static_cast<int>(value.getNumBytesAsUTF8()), SQLITE_TRANSIENT)
            == SQLITE_OK;
    }

    [[nodiscard]] bool bindOptionalText(int index, const juce::String& value)
    {
        return value.isEmpty() ? sqlite3_bind_null(statement, index) == SQLITE_OK
                               : bindText(index, value);
    }

    [[nodiscard]] bool bindInt(int index, int value)
    {
        return sqlite3_bind_int(statement, index, value) == SQLITE_OK;
    }

    [[nodiscard]] bool bindInt64(int index, std::int64_t value)
    {
        return sqlite3_bind_int64(statement, index, value) == SQLITE_OK;
    }

private:
    sqlite3* db = nullptr;
    sqlite3_stmt* statement = nullptr;
    int status = SQLITE_ERROR;
};

class Transaction final
{
public:
    explicit Transaction(sqlite3* database) : db(database)
    {
        active = execute(db, "BEGIN IMMEDIATE;").wasOk();
    }

    ~Transaction()
    {
        if (active)
            execute(db, "ROLLBACK;");
    }

    [[nodiscard]] bool began() const noexcept { return active; }

    [[nodiscard]] juce::Result commit()
    {
        if (!active)
            return juce::Result::fail("SQLite transaction did not begin");
        const auto result = execute(db, "COMMIT;");
        if (result.wasOk())
            active = false;
        return result;
    }

private:
    sqlite3* db = nullptr;
    bool active = false;
};

juce::String columnText(sqlite3_stmt* statement, int column)
{
    const auto* text = sqlite3_column_text(statement, column);
    const auto bytes = sqlite3_column_bytes(statement, column);
    return text != nullptr && bytes >= 0
        ? juce::String::fromUTF8(reinterpret_cast<const char*>(text), bytes)
        : juce::String{};
}

juce::Result encodeTags(std::span<const juce::String> tags, juce::String& json)
{
    if (const auto validation = validateHistoryTags(tags); validation.failed())
        return validation;
    auto sorted = std::vector<juce::String>(tags.begin(), tags.end());
    std::sort(sorted.begin(), sorted.end());
    juce::Array<juce::var> values;
    for (const auto& tag : sorted)
        values.add(tag);
    json = juce::JSON::toString(juce::var(values), true, 9);
    return juce::Result::ok();
}

juce::Result decodeTags(const juce::String& json, std::vector<juce::String>& tags)
{
    if (json.getNumBytesAsUTF8() > 4096)
        return juce::Result::fail("History tag payload exceeds its bound");
    juce::var parsed;
    if (const auto parse = juce::JSON::parse(json, parsed); parse.failed())
        return juce::Result::fail("History tag payload is malformed");
    const auto* array = parsed.getArray();
    if (array == nullptr || array->size() > 24)
        return juce::Result::fail("History tag payload is not a bounded array");
    tags.clear();
    for (const auto& value : *array)
    {
        if (!value.isString())
            return juce::Result::fail("History tag payload contains a non-string value");
        tags.push_back(value.toString());
    }
    return validateHistoryTags(tags);
}

juce::Result readUserVersion(sqlite3* database, int& version)
{
    Statement statement(database, "PRAGMA user_version;");
    if (!statement.prepared())
        return sqliteFailure(database, "Could not inspect history database schema", statement.code());
    if (sqlite3_step(statement.get()) != SQLITE_ROW)
        return sqliteFailure(database, "Could not read history database schema");
    version = sqlite3_column_int(statement.get(), 0);
    return juce::Result::ok();
}

juce::Result migrate(sqlite3* database, int& version)
{
    if (const auto read = readUserVersion(database, version); read.failed())
        return read;
    if (version < 0 || version > currentDatabaseSchemaVersion)
        return juce::Result::fail("History database schema is newer than this folk park build");
    if (version == currentDatabaseSchemaVersion)
        return juce::Result::ok();

    Transaction transaction(database);
    if (!transaction.began())
        return sqliteFailure(database, "Could not begin history database migration");
    if (version == 0)
    {
        if (const auto step = execute(database, R"sql(
            CREATE TABLE history_entries (
                id TEXT PRIMARY KEY NOT NULL,
                parent_id TEXT REFERENCES history_entries(id) ON DELETE SET NULL,
                created_ms INTEGER NOT NULL,
                updated_ms INTEGER NOT NULL,
                entry_schema_version INTEGER NOT NULL,
                generator_version TEXT NOT NULL,
                prompt_summary TEXT,
                prompt_stored INTEGER NOT NULL CHECK(prompt_stored IN (0,1)),
                macro_json TEXT NOT NULL,
                clip_json TEXT NOT NULL
            );
            CREATE INDEX history_entries_created_idx ON history_entries(created_ms DESC);
            CREATE INDEX history_entries_parent_idx ON history_entries(parent_id);
            PRAGMA user_version = 1;
        )sql"); step.failed())
            return step;
        version = 1;
    }
    if (version == 1)
    {
        if (const auto step = execute(database, R"sql(
            ALTER TABLE history_entries ADD COLUMN preset_id TEXT;
            ALTER TABLE history_entries ADD COLUMN favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1));
            ALTER TABLE history_entries ADD COLUMN tags_json TEXT NOT NULL DEFAULT '[]';
            ALTER TABLE history_entries ADD COLUMN deleted INTEGER NOT NULL DEFAULT 0 CHECK(deleted IN (0,1));
            CREATE INDEX history_entries_updated_idx ON history_entries(updated_ms DESC);
            CREATE INDEX history_entries_favorite_idx ON history_entries(favorite, updated_ms DESC);
            CREATE INDEX history_entries_deleted_idx ON history_entries(deleted, updated_ms DESC);
            CREATE TABLE history_settings (
                key TEXT PRIMARY KEY NOT NULL,
                value TEXT NOT NULL
            );
            INSERT INTO history_settings(key, value) VALUES('retentionDays', '180');
            PRAGMA user_version = 2;
        )sql"); step.failed())
            return step;
        version = 2;
    }
    return transaction.commit();
}

juce::Result parseSummary(sqlite3_stmt* statement, HistorySummary& summary)
{
    summary.schemaVersion = sqlite3_column_int(statement, 0);
    summary.id = columnText(statement, 1);
    summary.parentId = columnText(statement, 2);
    summary.createdUnixMs = sqlite3_column_int64(statement, 3);
    summary.updatedUnixMs = sqlite3_column_int64(statement, 4);
    summary.generatorVersion = columnText(statement, 5);
    summary.storePromptSummary = sqlite3_column_int(statement, 6) != 0;
    summary.promptSummary = columnText(statement, 7);
    summary.presetId = columnText(statement, 8);
    summary.favorite = sqlite3_column_int(statement, 9) != 0;
    summary.deleted = sqlite3_column_int(statement, 11) != 0;
    if (const auto tags = decodeTags(columnText(statement, 10), summary.tags); tags.failed())
        return tags;
    if (summary.schemaVersion != HistoryEntry::currentSchemaVersion
        || !midi::isUuid(summary.id)
        || (!summary.parentId.isEmpty() && !midi::isUuid(summary.parentId))
        || (!summary.presetId.isEmpty() && !midi::isUuid(summary.presetId))
        || summary.createdUnixMs < 0 || summary.updatedUnixMs < summary.createdUnixMs
        || !boundedText(summary.generatorVersion, 32, false)
        || (!summary.storePromptSummary && !summary.promptSummary.isEmpty())
        || !boundedText(summary.promptSummary, 512))
        return juce::Result::fail("History database contains an invalid summary record");
    return juce::Result::ok();
}

juce::String escapedLikePattern(const juce::String& text)
{
    return "%" + text.toLowerCase().replace("\\", "\\\\").replace("%", "\\%")
        .replace("_", "\\_") + "%";
}
}

struct SqliteHistoryRepository::Impl
{
    explicit Impl(juce::File file) : databaseFile(std::move(file)) {}

    juce::File databaseFile;
    sqlite3* database = nullptr;
    mutable std::mutex mutex;
    int version = 0;
    bool initialized = false;
};

juce::Result validateHistoryTags(std::span<const juce::String> tags)
{
    if (tags.size() > 24)
        return juce::Result::fail("History tags exceed the 24-item bound");
    std::set<juce::String> unique;
    for (const auto& tag : tags)
        if (!boundedText(tag, 48, false) || !unique.insert(tag).second)
            return juce::Result::fail("History tags must be bounded and unique");
    return juce::Result::ok();
}

juce::Result validateHistoryEntry(const HistoryEntry& entry)
{
    if (entry.schemaVersion != HistoryEntry::currentSchemaVersion
        || !midi::isUuid(entry.id)
        || (!entry.parentId.isEmpty() && !midi::isUuid(entry.parentId))
        || entry.parentId == entry.id
        || (!entry.presetId.isEmpty() && !midi::isUuid(entry.presetId)))
        return juce::Result::fail("History entry IDs or schema version are invalid");
    if (entry.createdUnixMs < 0 || entry.updatedUnixMs < entry.createdUnixMs)
        return juce::Result::fail("History entry timestamps are invalid");
    if (!boundedText(entry.generatorVersion, 32, false)
        || !boundedText(entry.promptSummary, 512)
        || (!entry.storePromptSummary && !entry.promptSummary.isEmpty()))
        return juce::Result::fail("History entry generator or private prompt metadata is invalid");
    if (const auto tags = validateHistoryTags(entry.tags); tags.failed())
        return tags;
    if (const auto intent = midi::validateMusicIntent(entry.macroSnapshot); intent.failed())
        return intent;
    if (const auto bundle = midi::validateBundle(entry.composition); bundle.failed())
        return bundle;
    const auto macro = encodeMusicIntentJson(entry.macroSnapshot);
    const auto bundleIntent = encodeMusicIntentJson(entry.composition.intent);
    if (!macro.succeeded() || !bundleIntent.succeeded() || macro.json != bundleIntent.json)
        return juce::Result::fail("History macro snapshot differs from its recalled composition intent");
    for (const auto& clip : entry.composition.clips)
        if (clip.generatorVersion != entry.generatorVersion)
            return juce::Result::fail("History generator version differs from a recalled clip");
    return juce::Result::ok();
}

SqliteHistoryRepository::SqliteHistoryRepository(juce::File databaseFile)
    : impl(std::make_unique<Impl>(std::move(databaseFile)))
{
}

SqliteHistoryRepository::~SqliteHistoryRepository()
{
    const std::lock_guard lock(impl->mutex);
    if (impl->database != nullptr)
        sqlite3_close_v2(impl->database);
}

juce::Result SqliteHistoryRepository::initialise()
{
    const std::lock_guard lock(impl->mutex);
    if (impl->initialized)
        return juce::Result::ok();
    const auto& file = impl->databaseFile;
    if (!file.hasFileExtension("sqlite3") || file.isSymbolicLink()
        || file.getParentDirectory().isSymbolicLink()
        || (file.existsAsFile() && (file.getSize() <= 0 || file.getSize() > maximumDatabaseBytes)))
        return juce::Result::fail("History database path, type, or size is unsafe");
    if (!file.getParentDirectory().isDirectory() && !file.getParentDirectory().createDirectory())
        return juce::Result::fail("History database directory could not be created");

    const auto path = file.getFullPathName().toRawUTF8();
    const auto openCode = sqlite3_open_v2(path, &impl->database,
                                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                                              | SQLITE_OPEN_FULLMUTEX,
                                          nullptr);
    if (openCode != SQLITE_OK)
    {
        const auto failure = sqliteFailure(impl->database, "History database could not be opened",
                                           openCode);
        if (impl->database != nullptr)
            sqlite3_close_v2(impl->database);
        impl->database = nullptr;
        return failure;
    }
    sqlite3_extended_result_codes(impl->database, 1);
    sqlite3_busy_timeout(impl->database, 250);
    for (const auto* pragma : {"PRAGMA foreign_keys=ON;", "PRAGMA journal_mode=WAL;",
                               "PRAGMA synchronous=FULL;", "PRAGMA trusted_schema=OFF;"})
    {
        if (const auto result = execute(impl->database, pragma); result.failed())
        {
            sqlite3_close_v2(impl->database);
            impl->database = nullptr;
            return result;
        }
    }
    if (const auto migration = migrate(impl->database, impl->version); migration.failed())
    {
        sqlite3_close_v2(impl->database);
        impl->database = nullptr;
        return migration;
    }
    impl->initialized = true;
    return juce::Result::ok();
}

int SqliteHistoryRepository::schemaVersion() const noexcept
{
    const std::lock_guard lock(impl->mutex);
    return impl->version;
}

juce::Result SqliteHistoryRepository::store(const HistoryEntry& entry)
{
    if (const auto validation = validateHistoryEntry(entry); validation.failed())
        return validation;
    const auto macro = encodeMusicIntentJson(entry.macroSnapshot);
    const auto composition = encodeCompositionJson(entry.composition);
    juce::String tags;
    if (!macro.succeeded() || !composition.succeeded()
        || encodeTags(entry.tags, tags).failed())
        return juce::Result::fail("History entry could not be encoded within bounds");

    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Transaction transaction(impl->database);
    if (!transaction.began())
        return sqliteFailure(impl->database, "Could not begin history store transaction");
    Statement statement(impl->database, R"sql(
        INSERT INTO history_entries(
            id,parent_id,created_ms,updated_ms,entry_schema_version,generator_version,
            prompt_summary,prompt_stored,macro_json,clip_json,preset_id,favorite,tags_json,deleted)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);
    )sql");
    if (!statement.prepared())
        return sqliteFailure(impl->database, "Could not prepare history store", statement.code());
    const auto bound = statement.bindText(1, entry.id)
        && statement.bindOptionalText(2, entry.parentId)
        && statement.bindInt64(3, entry.createdUnixMs)
        && statement.bindInt64(4, entry.updatedUnixMs)
        && statement.bindInt(5, entry.schemaVersion)
        && statement.bindText(6, entry.generatorVersion)
        && statement.bindOptionalText(7, entry.storePromptSummary ? entry.promptSummary : juce::String{})
        && statement.bindInt(8, entry.storePromptSummary ? 1 : 0)
        && statement.bindText(9, macro.json)
        && statement.bindText(10, composition.json)
        && statement.bindOptionalText(11, entry.presetId)
        && statement.bindInt(12, entry.favorite ? 1 : 0)
        && statement.bindText(13, tags)
        && statement.bindInt(14, entry.deleted ? 1 : 0);
    if (!bound || sqlite3_step(statement.get()) != SQLITE_DONE)
        return sqliteFailure(impl->database, "History entry could not be stored");
    return transaction.commit();
}

HistoryRecallResult SqliteHistoryRepository::recall(const juce::String& id)
{
    HistoryRecallResult result;
    if (!midi::isUuid(id))
    {
        result.status = juce::Result::fail("History recall ID must be a UUID");
        return result;
    }
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
    {
        result.status = juce::Result::fail("History repository is unavailable");
        return result;
    }
    Statement statement(impl->database, R"sql(
        SELECT entry_schema_version,id,parent_id,created_ms,updated_ms,generator_version,
               prompt_stored,prompt_summary,preset_id,favorite,tags_json,deleted,macro_json,clip_json
        FROM history_entries WHERE id=?;
    )sql");
    if (!statement.prepared() || !statement.bindText(1, id))
    {
        result.status = sqliteFailure(impl->database, "Could not prepare history recall",
                                      statement.code());
        return result;
    }
    const auto step = sqlite3_step(statement.get());
    if (step == SQLITE_DONE)
    {
        result.status = juce::Result::fail("History entry was not found");
        return result;
    }
    if (step != SQLITE_ROW)
    {
        result.status = sqliteFailure(impl->database, "History entry could not be recalled", step);
        return result;
    }
    HistorySummary summary;
    if (const auto parsed = parseSummary(statement.get(), summary); parsed.failed())
    {
        result.status = parsed;
        return result;
    }
    result.entry.schemaVersion = summary.schemaVersion;
    result.entry.id = summary.id;
    result.entry.parentId = summary.parentId;
    result.entry.createdUnixMs = summary.createdUnixMs;
    result.entry.updatedUnixMs = summary.updatedUnixMs;
    result.entry.generatorVersion = summary.generatorVersion;
    result.entry.storePromptSummary = summary.storePromptSummary;
    result.entry.promptSummary = summary.promptSummary;
    result.entry.presetId = summary.presetId;
    result.entry.favorite = summary.favorite;
    result.entry.tags = summary.tags;
    result.entry.deleted = summary.deleted;
    const auto macro = decodeMusicIntentJson(columnText(statement.get(), 12));
    const auto composition = decodeCompositionJson(columnText(statement.get(), 13));
    if (!macro.succeeded() || !composition.succeeded())
    {
        result.status = juce::Result::fail("History entry payload is corrupt or unsupported");
        return result;
    }
    result.entry.macroSnapshot = macro.intent;
    result.entry.composition = composition.bundle;
    result.status = validateHistoryEntry(result.entry);
    return result;
}

HistorySearchResult SqliteHistoryRepository::search(const HistorySearchQuery& query)
{
    HistorySearchResult result;
    if (!boundedText(query.text, 128) || query.limit < 1 || query.limit > maximumSearchResults)
    {
        result.status = juce::Result::fail("History search query is outside its bounds");
        return result;
    }
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
    {
        result.status = juce::Result::fail("History repository is unavailable");
        return result;
    }
    Statement statement(impl->database, R"sql(
        SELECT entry_schema_version,id,parent_id,created_ms,updated_ms,generator_version,
               prompt_stored,prompt_summary,preset_id,favorite,tags_json,deleted
        FROM history_entries
        WHERE (?=1 OR deleted=0)
          AND (?=0 OR favorite=1)
          AND (?='' OR lower(coalesce(prompt_summary,'') || ' ' || tags_json || ' '
                              || generator_version || ' ' || id) LIKE ? ESCAPE '\')
        ORDER BY favorite DESC, updated_ms DESC, id ASC
        LIMIT ?;
    )sql");
    if (!statement.prepared())
    {
        result.status = sqliteFailure(impl->database, "Could not prepare history search",
                                      statement.code());
        return result;
    }
    const auto pattern = escapedLikePattern(query.text);
    const auto bound = statement.bindInt(1, query.includeDeleted ? 1 : 0)
        && statement.bindInt(2, query.favoritesOnly ? 1 : 0)
        && statement.bindText(3, query.text.toLowerCase())
        && statement.bindText(4, pattern)
        && statement.bindInt(5, query.limit);
    if (!bound)
    {
        result.status = sqliteFailure(impl->database, "Could not bind history search");
        return result;
    }
    while (true)
    {
        const auto step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE)
            break;
        if (step != SQLITE_ROW)
        {
            result.status = sqliteFailure(impl->database, "History search failed", step);
            return result;
        }
        HistorySummary summary;
        if (const auto parse = parseSummary(statement.get(), summary); parse.failed())
        {
            result.status = parse;
            return result;
        }
        result.entries.push_back(std::move(summary));
    }
    result.status = juce::Result::ok();
    return result;
}

HistorySearchResult SqliteHistoryRepository::childrenOf(const juce::String& parentId, int limit)
{
    HistorySearchResult result;
    if (!midi::isUuid(parentId) || limit < 1 || limit > maximumSearchResults)
    {
        result.status = juce::Result::fail("History lineage query is outside its bounds");
        return result;
    }
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
    {
        result.status = juce::Result::fail("History repository is unavailable");
        return result;
    }
    Statement statement(impl->database, R"sql(
        SELECT entry_schema_version,id,parent_id,created_ms,updated_ms,generator_version,
               prompt_stored,prompt_summary,preset_id,favorite,tags_json,deleted
        FROM history_entries WHERE parent_id=? AND deleted=0
        ORDER BY created_ms ASC, id ASC LIMIT ?;
    )sql");
    if (!statement.prepared() || !statement.bindText(1, parentId)
        || !statement.bindInt(2, limit))
    {
        result.status = sqliteFailure(impl->database, "Could not prepare history lineage query",
                                      statement.code());
        return result;
    }
    while (true)
    {
        const auto step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW)
        {
            result.status = sqliteFailure(impl->database, "History lineage query failed", step);
            return result;
        }
        HistorySummary summary;
        if (const auto parse = parseSummary(statement.get(), summary); parse.failed())
        {
            result.status = parse;
            return result;
        }
        result.entries.push_back(std::move(summary));
    }
    result.status = juce::Result::ok();
    return result;
}

namespace
{
juce::Result updateBoolean(sqlite3* database,
                           const char* column,
                           const juce::String& id,
                           bool value,
                           std::int64_t updatedUnixMs)
{
    if (!midi::isUuid(id) || updatedUnixMs < 0)
        return juce::Result::fail("History update ID or timestamp is invalid");
    const auto sql = "UPDATE history_entries SET " + juce::String(column)
        + "=?, updated_ms=? WHERE id=? AND updated_ms<=?;";
    Statement statement(database, sql.toRawUTF8());
    if (!statement.prepared() || !statement.bindInt(1, value ? 1 : 0)
        || !statement.bindInt64(2, updatedUnixMs) || !statement.bindText(3, id)
        || !statement.bindInt64(4, updatedUnixMs))
        return sqliteFailure(database, "Could not prepare history metadata update", statement.code());
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
        return sqliteFailure(database, "History metadata update failed");
    if (sqlite3_changes(database) != 1)
        return juce::Result::fail("History metadata update found no compatible entry");
    return juce::Result::ok();
}
}

juce::Result SqliteHistoryRepository::setFavorite(const juce::String& id,
                                                  bool favorite,
                                                  std::int64_t updatedUnixMs)
{
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Transaction transaction(impl->database);
    if (!transaction.began()) return sqliteFailure(impl->database, "Could not begin favorite update");
    if (const auto update = updateBoolean(impl->database, "favorite", id, favorite, updatedUnixMs);
        update.failed())
        return update;
    return transaction.commit();
}

juce::Result SqliteHistoryRepository::setTags(const juce::String& id,
                                              std::span<const juce::String> tags,
                                              std::int64_t updatedUnixMs)
{
    if (!midi::isUuid(id) || updatedUnixMs < 0)
        return juce::Result::fail("History tag update ID or timestamp is invalid");
    juce::String json;
    if (const auto encoding = encodeTags(tags, json); encoding.failed())
        return encoding;
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Transaction transaction(impl->database);
    if (!transaction.began()) return sqliteFailure(impl->database, "Could not begin tag update");
    Statement statement(impl->database,
                        "UPDATE history_entries SET tags_json=?, updated_ms=? WHERE id=? AND updated_ms<=?;");
    if (!statement.prepared() || !statement.bindText(1, json)
        || !statement.bindInt64(2, updatedUnixMs) || !statement.bindText(3, id)
        || !statement.bindInt64(4, updatedUnixMs)
        || sqlite3_step(statement.get()) != SQLITE_DONE)
        return sqliteFailure(impl->database, "History tag update failed", statement.code());
    if (sqlite3_changes(impl->database) != 1)
        return juce::Result::fail("History tag update found no compatible entry");
    return transaction.commit();
}

juce::Result SqliteHistoryRepository::setSoftDeleted(const juce::String& id,
                                                     bool deleted,
                                                     std::int64_t updatedUnixMs)
{
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Transaction transaction(impl->database);
    if (!transaction.began()) return sqliteFailure(impl->database, "Could not begin soft-delete update");
    if (const auto update = updateBoolean(impl->database, "deleted", id, deleted, updatedUnixMs);
        update.failed())
        return update;
    return transaction.commit();
}

juce::Result SqliteHistoryRepository::setRetentionDays(int days)
{
    if (days < 1 || days > maximumRetentionDays)
        return juce::Result::fail("History retention must be 1-3650 days");
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Transaction transaction(impl->database);
    if (!transaction.began()) return sqliteFailure(impl->database, "Could not begin retention update");
    Statement statement(impl->database,
                        "UPDATE history_settings SET value=? WHERE key='retentionDays';");
    if (!statement.prepared() || !statement.bindText(1, juce::String(days))
        || sqlite3_step(statement.get()) != SQLITE_DONE || sqlite3_changes(impl->database) != 1)
        return sqliteFailure(impl->database, "History retention preference could not be saved",
                             statement.code());
    return transaction.commit();
}

juce::Result SqliteHistoryRepository::retentionDays(int& days)
{
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
        return juce::Result::fail("History repository is unavailable");
    Statement statement(impl->database,
                        "SELECT value FROM history_settings WHERE key='retentionDays';");
    if (!statement.prepared() || sqlite3_step(statement.get()) != SQLITE_ROW)
        return sqliteFailure(impl->database, "History retention preference could not be read",
                             statement.code());
    const auto text = columnText(statement.get(), 0);
    if (!text.containsOnly("0123456789"))
        return juce::Result::fail("History retention preference is corrupt");
    days = text.getIntValue();
    if (days < 1 || days > maximumRetentionDays)
        return juce::Result::fail("History retention preference is outside its bound");
    return juce::Result::ok();
}

HistoryCleanupResult SqliteHistoryRepository::cleanupOlderThan(std::int64_t cutoffUnixMs,
                                                               bool keepFavorites)
{
    HistoryCleanupResult result;
    if (cutoffUnixMs < 0)
    {
        result.status = juce::Result::fail("History cleanup cutoff is invalid");
        return result;
    }
    const std::lock_guard lock(impl->mutex);
    if (!impl->initialized || impl->database == nullptr)
    {
        result.status = juce::Result::fail("History repository is unavailable");
        return result;
    }
    Transaction transaction(impl->database);
    if (!transaction.began())
    {
        result.status = sqliteFailure(impl->database, "Could not begin history cleanup");
        return result;
    }
    Statement statement(impl->database,
                        "DELETE FROM history_entries WHERE updated_ms < ? AND (?=0 OR favorite=0);");
    if (!statement.prepared() || !statement.bindInt64(1, cutoffUnixMs)
        || !statement.bindInt(2, keepFavorites ? 1 : 0)
        || sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        result.status = sqliteFailure(impl->database, "History cleanup failed", statement.code());
        return result;
    }
    result.removedCount = sqlite3_changes(impl->database);
    result.status = transaction.commit();
    return result;
}
}
