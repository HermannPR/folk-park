# Native preset format

Release 0.1 uses the `.folkparkpreset` extension and deterministic, non-executable JSON. The current frozen contract is schema version 2 in `schemas/preset.schema.json`; the pre-M6 placeholder is retained as the oldest supported schema version 1 in `schemas/preset-v1.schema.json` and can be loaded only through the tested `preset-v1-to-v2` migration.

## Version 2 document

A preset contains:

- product identifier, user-facing name, and semantic product version;
- bounded metadata: UUID, preset name, author, unique tags, genre, emotion, description, and favorite status;
- exactly the complete normalized C++ parameter catalog through M5, split into 73 synth/modulation values and 29 values in the six ordered effect records;
- up to 32 validated modulation routes;
- the fixed Distortion, Chorus, Delay, Reverb, Compressor, EQ order and state;
- up to two content-addressed user-WAV wavetable references with lowercase SHA-256, byte length, preset-relative path, and safe recovery display metadata; a general sample oscillator requires a future schema because it is outside Release 0.1;
- optional bounded WAV preview metadata;
- original schema version and ordered migration-step provenance;
- inert unknown top-level fields preserved for compatible future metadata.

Normalized parameter values are stored because their stable host parameter IDs are the compatibility surface. C++ remains authoritative for the exact catalog, ranges, defaults, effect order, modulation enums, and transactional application.

## Bounds and path policy

The native codec must reject input before publication when any of these checks fail:

- maximum preset bytes, JSON nesting depth, member count, array count, or string length;
- malformed UTF-8/JSON, duplicate security-sensitive IDs, non-finite numbers, missing required fields, or unsupported schema version;
- unknown nested fields in asset/path, route, effect, or parameter records;
- absolute paths, backslashes, empty segments, `.`/`..`, percent-encoded traversal, or anything outside `assets/<lowercase-sha256>.wav`;
- a file, decoded WAV, converted table, or declared length outside the documented bound;
- a declared hash/length that does not match the selected local asset;
- symlink resolution outside the preset/asset root.

Loading is local-only: it never fetches a URL and never executes HTML, JavaScript, shell content, plug-in state, or another binary payload. Recovery requires an explicit user-selected file whose content hash matches the missing reference.

## Atomic and transactional behavior

Saving writes deterministic bytes to a unique sibling temporary file, closes them, reopens and validates the result, and only then atomically renames it into place. Replacing an existing preset requires explicit overwrite authorization. Failure removes only the temporary work and leaves the existing destination intact.

Loading performs byte/depth validation, parses into a candidate model, migrates if required, validates the complete model and every asset, and constructs an immutable replacement snapshot. Processor parameters, routes, and wavetable banks are published only after the entire candidate succeeds. No JSON, filesystem, hash, migration, database, allocation, or object destruction is reachable from `processBlock`.

## Migration policy

Migrations are pure deterministic transformations. The v1-to-v2 step receives explicit defaults/catalog context, never wall-clock time or machine state, and records `preset-v1-to-v2` in provenance. Unsupported future versions and malformed required legacy data return a recoverable error without modifying the current sound.

Serum private state and factory assets remain outside this format and outside Release 0.1.
