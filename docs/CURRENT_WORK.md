# Current work and agent handoff

Last updated: 2026-08-21, America/Monterrey

This is the first file a continuing coding agent must read. It records what is actually complete, what remains unverified, and how to resume without repeating or damaging previous work. Confirm every statement against the repository before acting.

## Product objective

Build `folk park` Release 0.1 as an original Intel macOS `x86_64` wavetable instrument in both Standalone and VST3 formats. It combines a playable dual-wavetable synth, effects, deterministic MIDI composition, native presets/history, and an optional guided AI sound-design workflow. It must remain offline-capable and must not clone Serum, import proprietary Serum state, or ship unlicensed factory assets.

The release is not complete until M8 is complete, the Release Standalone and VST3 build as `x86_64`, automated tests and validation artifacts pass, and all FL Studio checks are explicitly reported as either human-passed or still required. Never silently convert a human-required check into a pass.

## Authoritative source material

Read these in order before implementation:

1. `AGENTS.md`
2. this file
3. `plans/RELEASE_0_1.md`
4. `docs/PROGRESS.md`
5. the document relevant to the milestone, such as `docs/PRESET_FORMAT.md`
6. `/Users/hermannpr/Downloads/folk_park_Codex_Master_Build_Prompt.docx`

The DOCX is the master product and engineering contract. Read it without modifying it, for example with `textutil -convert txt -stdout`. If it is missing, stop and ask the user for its new location rather than inventing requirements.

## Repository and Git rules

- Private GitHub repository: `HermannPR/folk-park`.
- Current working branch: `feat/m6-presets-history`, stacked on the final M5 checkpoint.
- Current checkpoint commit when this handoff was written: `1c13e21 Verified and documented the M5 checkpoint`.
- M5 draft pull request: <https://github.com/HermannPR/folk-park/pull/5>, based exactly on `feat/m4-silicon-dreams-ui`.
- M6 work must remain stacked on `feat/m5-effects-preview`; its draft PR base must be that exact branch.
- Keep each stage buildable and commit meaningful stages separately. The user prefers a commit at every stage.
- Use concise impersonal commit subjects such as `Established ...`, `Implemented ...`, `Hardened ...`, or `Verified ...`. Do not write subjects such as `I established ...`.
- Review `git status` and the exact diff before every commit. Stage exact paths; do not use `git add .` or `git add -A`.
- Preserve unrelated user changes. Never rewrite or force-push established history without explicit permission.
- Keep the repository private. Do not publish releases, packages, assets, or source publicly.

## Completed milestones

- M0: repository/toolchain/risk spikes; automated gate passed.
- M1: playable vertical slice; automated gate passed, human host run pending.
- M2: dual wavetable/modulation/import; automated gate passed, human host checks pending.
- M3: deterministic composition and MIDI delivery; automated gate passed, FL drag/routing checks pending.
- M4: Silicon Dreams UI; automated gate passed. It includes live OSC A/B waveform visualization, an optional frame/morph surface, spectrum feedback, and a four-octave C2-B5 piano. Held computer keys are idempotent so macOS key repeat does not retrigger/spam notes.
- M5: ordered effects and isolated accepted-composition WAV rendering; automated gate passed, FL effects/render checks pending.

Do not reimplement these milestones. Read `docs/PROGRESS.md` and their evidence directories for exact test results and limitations.

## Exact M5 checkpoint

M5 added the fixed Distortion -> Chorus -> tempo-synced Delay -> Reverb -> Compressor -> Parametric EQ chain, with safe bypass defaults and bounded real-time behavior. It also added accepted-only isolated WAV rendering with overwrite confirmation and cancellation.

Recorded verification:

- UI tests: 8/8; npm audit: 0 vulnerabilities.
- Debug CTest: 8/8.
- Release CTest: 9/9, including built VST3 smoke.
- pluginval 1.0.4 strictness 5: SUCCESS.
- Release VST3 SHA-256: `5ff08476376a4d37e0224623741c3e42a5e15cb52118b6259f33705c965126ac`.
- Release Standalone SHA-256: `c151eb0486992e6dd80f98dde2644b0281f44858481e3fa127a086a2fbd213e1`.
- Evidence: `evidence/m5/verification.md`, `evidence/m5/pluginval/`, and M5 Standalone screenshots.

Outstanding M5 human checks include FL Studio effect audibility/order, bypass transitions, tempo-synced delay, automation/state recall, CPU behavior, accepted WAV render/cancel/import/playback, tail/length parity, and live-voice isolation through the FL wrapper.

## Explicit interface requirements to preserve

- Both oscillators must display their real waveform. Imported/generated wavetables must display the selected frame, respond to wavetable-position changes, and show OSC A and OSC B independently. Visualization reads bounded immutable UI snapshots, never mutable audio state.
- Keep the optional Serum-like 3D frame/morph visualization original in design, with 2D low-graphics and reduced-motion fallbacks.
- The playable piano spans more than one octave: the implemented visual keyboard is C2-B5. Computer-key audition has octave shift and must suppress macOS held-key repeat so a held key produces one sustained note, not repeated attacks.
- The product must work as both Standalone and VST3. VST3 behavior in FL Studio remains a human verification boundary.
- Add a guided sound-design walkthrough in M7: ask adaptive questions, translate answers into bounded explained parameter proposals, audition A/B reversibly, and require explicit acceptance before applying changes.
- AI composition may propose chords, melodies, bass, arpeggios, MIDI, samples, and sound changes, but provider access is opt-in and offline/manual workflows remain complete.
- Preserving or bypassing Serum 2 licensing is not a product feature. Folk Park uses its own format and legal assets.

## Active milestone: M6 presets and history

M6 contract work has started on `feat/m6-presets-history`. ADR-0007 freezes the permissive pre-M6 preset placeholder as the oldest supported schema version 1 and establishes schema version 2 as the first production contract, with a pure deterministic v1-to-v2 migration. The current macOS SDK provides `sqlite3.h` and `libsqlite3.tbd`; M6 will use that system library behind `HistoryRepository`, never from DSP or the audio callback. `LICENSES.md` records the system boundary.

The first M6 stage established `schemas/preset.schema.json`, `schemas/preset-v1.schema.json`, `docs/PRESET_FORMAT.md`, and `docs/adr/0007-m6-persistence-and-migrations.md`. The second stage added the shared exact parameter catalog and the independent `src/persistence/Preset.*` module: deterministic current encoding, pure v1-to-v2 migration, bounded pre-parse scanning with duplicate-key rejection, strict model validation, atomic overwrite-safe storage, content-addressed user-WAV validation/import, missing-asset reporting, explicit hash-matched relink, and symlink/traversal rejection. `tests/PresetTests.cpp` covers current/legacy/malformed/future/deep/oversized/non-finite/traversal/missing/wrong-hash/atomic cases. The complete Debug build succeeds and the expanded suite passes 9/9.

The third stage added versioned canonical `MusicIntent`/`CompositionBundle` payload codecs and the `HistoryRepository` interface with a system-SQLite implementation. Database versions 1 and 2 migrate under rollback-safe transactions; prepared statements cover store, lightweight bounded search, exact recall, parent lineage, favorites, canonical tags, soft deletion, retention preferences, and explicit cleanup. Tests cover fresh and oldest-schema migration, failed-migration rollback, future-schema refusal, duplicate keys, exact payload recall, search wildcard escaping, privacy-disabled prompt storage, foreign-key rollback, corruption/unavailable paths, retention, and caller-snapshot isolation. The complete Debug build succeeds and the expanded suite passes 10/10.

The next implementation stage is processor/message-thread and UI integration: native preset capture/apply/browser/relink plus automatic accepted-composition history, History search/favorite/recall/compare/retention controls, and database-unavailable status that never blocks acceptance or audio.

M6 must deliver:

- versioned deterministic `.folkparkpreset` JSON;
- current and oldest-supported preset migrations;
- author, tags, genre, emotion, description, parameter state, modulation routes, ordered effect state, asset hashes/recovery metadata, preview metadata, and migration provenance;
- validated content-addressed wavetable/sample asset handling and missing-asset recovery;
- atomic save through a sibling temporary file and rename/replace only after validation;
- strict bounds for file size, decoded asset size, JSON nesting, strings, arrays, paths, and hash values;
- rejection of executable content, absolute paths, `..`, traversal, malformed required data, oversized input, and unsupported future required state;
- transactional recall so a failed parse/apply leaves the active sound unchanged;
- a `HistoryRepository` interface backed by SQLite if the verified system boundary is acceptable;
- stable UUIDs, parent/variation lineage, timestamps, generator/schema versions, privacy-controlled prompt summary, macro snapshot, clip payload, optional preset reference, favorites, tags, soft delete, retention preference, and cleanup command;
- searchable history and correct versioned recall;
- database migrations and transactions;
- database failure isolation: history/database failure must never stop or alter active audio;
- no filesystem, JSON, or database access from the audio callback.

Required M6 fixtures/tests include current preset, oldest-supported preset, malformed JSON, missing asset, oversized data, path traversal, unknown/future fields, migration correctness, database migration, database unavailable/failure isolation, retention, lineage, search, and exact recall. Keep native preset migrations pure and deterministic. Put SQLite behind an interface so DSP never depends on it.

Suggested buildable commit sequence:

1. `Established the M6 persistence and migration contracts` — ADR, schemas, immutable models, bounds, and fixtures.
2. `Implemented versioned native presets and validated assets` — deterministic codec, migrations, atomic storage, content-addressed assets, and focused tests.
3. `Implemented transactional searchable composition history` — repository interface, SQLite migrations, lineage/search/retention/recall, and failure tests.
4. `Integrated preset and history workflows` — processor/message-thread coordination and Preset/History UI without audio-thread persistence.
5. `Hardened M6 persistence failure boundaries` — adversarial fixtures, transactional rollback, missing-asset recovery, UI bridge rejection, and failure isolation.
6. `Verified and documented the M6 checkpoint` — full Debug/Release/UI/validator/artifact gate and evidence.

This ordering is guidance, not permission to claim a stage passed without its tests.

## Likely M6 design decisions requiring confirmation in code

- `schemas/preset.schema.json` currently describes schema version 1 but is incomplete and permissive. Do not silently reinterpret a shipped version. Establish the compatibility decision in the ADR and test a real migration path.
- Use deterministic key insertion/serialization and verify it byte-for-byte. Pre-scan JSON nesting while respecting strings before using a recursive parser.
- Asset references should be relative and content-addressed; validate exact syntax and SHA-256 before use. Never trust a filename from imported content.
- The current imported wavetable lives only in bounded session memory. M6 must persist the legal source/converted asset explicitly; do not pretend parameter-only state restores it.
- Capture immutable state outside the callback, validate the entire replacement, then publish it at a safe boundary. Do not partially mutate live state during validation.
- SQLite schema versions/migrations are append-only. Repository operations run off the audio callback and failures return typed results rather than destabilizing synthesis.

## Commands and environment

The user-local CMake/CTest executables are under `/Users/hermannpr/Library/Python/3.9/bin` when not on `PATH`. Use the checked-in presets and scripts:

```sh
./scripts/bootstrap_macos.sh
/Users/hermannpr/Library/Python/3.9/bin/cmake --preset macos-x86_64-debug
/Users/hermannpr/Library/Python/3.9/bin/cmake --build --preset macos-x86_64-debug
/Users/hermannpr/Library/Python/3.9/bin/ctest --preset macos-x86_64-debug --output-on-failure
/Users/hermannpr/Library/Python/3.9/bin/cmake --preset macos-x86_64-release
/Users/hermannpr/Library/Python/3.9/bin/cmake --build --preset macos-x86_64-release
/Users/hermannpr/Library/Python/3.9/bin/ctest --preset macos-x86_64-release --output-on-failure
cd ui && npm ci --ignore-scripts && npm run build && npm test && npm run lint
./scripts/test.sh
./scripts/install_user_vst3.sh release
```

Only report commands actually run. Full milestone verification must also inspect `file` architecture, code signature, installed artifact/hash parity, the built VST3 smoke test, pluginval, and retained evidence as specified by the progress/plan documents.

## Safe resume checklist

1. Run `pwd`, read the required documents completely, and check for a more local `AGENTS.md` before editing.
2. Run `git status --short --branch`, `git log --oneline --decorate -12`, and `git remote -v`.
3. Confirm the repository is private before the next push or PR action.
4. Confirm M5 remains reproducible enough for the intended M6 change; do not rerun every expensive gate before the first documentation commit unless a dependency changed.
5. Continue on `feat/m6-presets-history`; do not recreate or rebase it unless repository evidence contradicts this handoff.
6. Maintain a plan with one in-progress stage and update this handoff when reality changes.
7. Implement one bounded stage, run its focused tests plus relevant regression tests, inspect the diff, commit exact paths, and push the checkpoint.
8. At the M6 gate, update `docs/PROGRESS.md`, this file, README current/coming-soon language if necessary, and `evidence/m6/verification.md`.
9. Open a private draft PR stacked exactly on `feat/m5-effects-preview` unless the branch topology has intentionally changed and is documented.

## Safety and truthfulness

Do not add credentials, provider keys, personal prompts, licensed plug-in binaries, Serum content/state, or third-party factory assets. Do not make network AI mandatory. Do not access unrelated personal files. Do not modify installed plug-ins or the user's FL Studio projects as part of repository development. Do not claim that a sound, plug-in, host workflow, or migration works merely because the UI exists; distinguish automated proof, local visual inspection, and the user's human listening/FL Studio result.
