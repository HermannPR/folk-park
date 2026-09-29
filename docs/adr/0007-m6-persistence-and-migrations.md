# ADR-0007: Native preset compatibility and local history persistence

- Status: Accepted for M6 implementation
- Date: 2026-08-21

## Context

M6 must persist complete synth states, user-owned wavetable assets, and accepted composition history without introducing filesystem or database work into the audio callback. The repository already contains a permissive schema-version-1 preset placeholder, but no production codec, saved presets, asset store, or database. Retrofitting strict semantics into version 1 would make the same version number describe two incompatible formats.

macOS 12 and the current macOS SDK expose the SQLite C API and system library. SQLite provides the transactions, migrations, indexes, and failure semantics required by the product contract without vendoring another runtime dependency.

## Decision

- Freeze the old permissive version-1 document as the oldest supported legacy input in `schemas/preset-v1.schema.json`. The first production preset contract is schema version 2 in `schemas/preset.schema.json`.
- Loaders accept only versions 1 and 2. Version 1 is bounded and validated before a pure deterministic `preset-v1-to-v2` transformation. Unsupported future versions fail without changing the active state.
- Version 2 stores a complete normalized host-parameter snapshot, validated modulation routes, the fixed ordered M5 effects and their normalized state, content-addressed assets, optional preview metadata, and explicit migration provenance.
- Unknown version-2 top-level fields are inert and preserved during load/save so a compatible reader does not destroy future optional metadata. Unknown nested fields in security-sensitive path, asset, parameter, route, and effect objects are rejected.
- Preset parsing is bounded before recursive JSON parsing: maximum file bytes, nesting depth, strings, arrays, objects, and decoded assets are enforced. The whole candidate is validated before any processor state is published.
- Preset files and content-addressed assets are written to a sibling temporary file, flushed/closed, reopened and validated, then atomically renamed or replaced only with explicit overwrite authorization.
- Asset references use lowercase SHA-256 and preset-relative `assets/<hash>.<extension>` paths. Absolute paths, separators outside that form, `..`, symlinks escaping the selected root, hash mismatches, and oversized decoded content are rejected. Recovery relinks only user-selected content whose hash matches.
- Define a small `HistoryRepository` interface using immutable composition/state records. The Release 0.1 implementation uses the macOS system SQLite library through its C API, with schema migrations in transactions, prepared statements, bounded search results, soft deletion, lineage, favorites, tags, retention, and exact versioned payload recall.
- No DSP module depends on preset JSON, filesystem code, SQLite, or history. Database and preset work is invoked only from non-audio coordinator/message/background threads. Repository failures are typed non-fatal results; active audio continues unchanged.

## Compatibility policy

- Preset and database schema versions are append-only. Any semantic change requires a new version and a tested migration.
- Parameter IDs remain authoritative in C++. Version 2 requires the exact 73 pre-effect and 29 effect parameter IDs introduced through M5. A future parameter addition requires a later preset schema or an explicit defaulting migration.
- Migration steps never add wall-clock time, random identifiers, machine paths, or network data. Identical legacy bytes and migration context produce identical version-2 bytes.
- Database migrations advance monotonically under `PRAGMA user_version`; a failed step rolls back and leaves the previous database readable.

## Dependency and license boundary

The SQLite runtime is supplied by macOS and linked as a system library. No SQLite source or binary is committed or redistributed by this repository. The system boundary is still recorded in `LICENSES.md`; public distribution remains blocked on the broader JUCE/signing/asset review.

## Consequences

- Current, legacy, malformed, future-version, unknown-field, missing-asset, oversized, hash-mismatch, and traversal fixtures must prove the codec boundary.
- Atomic-save tests must prove the old destination survives validation or replacement failure.
- History tests must cover fresh creation, every migration, transactions, search, favorite/tag changes, parent lineage, soft delete, retention cleanup, exact clip/state recall, and unavailable/corrupt/read-only database behavior.
- FL Studio preset save/load, project restart, missing-asset relink, and database-unavailable behavior remain human-required until executed in the actual host.
