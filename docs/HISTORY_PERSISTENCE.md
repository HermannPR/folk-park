# History persistence

Release 0.1 keeps generated compositions and explicit saved-state references in a local SQLite database behind `HistoryRepository`. The database is not part of the DAW project state, and no history/database operation is reachable from `processBlock`.

## Record contract

Each immutable stored entry has a stable UUID, optional parent UUID, created/updated timestamps, entry and generator versions, a privacy-controlled prompt summary, an exact `MusicIntent` macro snapshot, the complete versioned `CompositionBundle`, an optional native-preset UUID reference, favorite status, bounded unique tags, and soft-delete status. Recall parses and validates both payloads and confirms that the macro snapshot, bundle intent, and every clip generator version agree before returning a candidate.

Search returns lightweight summaries rather than parsing every clip payload. User text is length-bounded, bound through prepared statements, and escaped for SQL `LIKE`; favorites and deleted records are explicit filters. Full payload parsing happens only for an explicit UUID recall.

## Database versions

SQLite `PRAGMA user_version` is the append-only migration surface:

- version 1 creates the core entry table, lineage foreign key, privacy fields, macro JSON, clip JSON, and creation/parent indexes;
- version 2 appends preset reference, favorite, canonical tag JSON, soft deletion, updated/favorite/deleted indexes, and the retention preference table.

All migrations and writes use transactions. A failed migration rolls back without advancing `user_version`. Unsupported future versions are never downgraded. The connection enables foreign keys, WAL journaling, full synchronous writes, a bounded busy timeout, and prepared statements.

## Retention and recovery

The default preference is 180 days and can be changed within 1–3650 days. Cleanup is an explicit command with a cutoff; it can preserve favorites. Soft-deleted records remain recoverable through an explicit include-deleted query until cleanup removes them.

Database open, migration, corruption, constraint, query, and decoding failures return typed errors. They do not mutate caller-owned composition snapshots or touch the active audio engine. History archive import/export is intentionally deferred until a separate bounded path/container contract exists.
