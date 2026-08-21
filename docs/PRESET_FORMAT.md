# Native preset format

The future extension is `.folkparkpreset`. Release 0.1 uses a versioned, non-executable JSON representation with content-addressed user assets and transactional validation before publication to the engine.

Required top-level surfaces are `schemaVersion`, product/version metadata, author/tags/genre/emotion/description, parameter state, modulation routes, ordered effects, asset hashes/recovery metadata, and optional preview metadata. The initial schema is `schemas/preset.schema.json` and is intentionally incomplete until M6.

Loading never fetches a network resource or touches the audio thread. Saving must use a temporary file followed by atomic rename. Filenames, paths, JSON depth, asset size, decoded sample length, and hashes require validation. Migrations are pure tested transformations. Serum private state is outside this format and outside Release 0.1.
