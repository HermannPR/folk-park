# Current work and mandatory agent handoff

Last updated: 2026-08-23, America/Monterrey

## Read this first

This is the canonical continuation file for `folk park`. Every coding agent, including Codex in a new session, must read this file completely before changing code. Then reconcile it with `git status`, `git log`, `docs/PROGRESS.md`, `plans/RELEASE_0_1.md`, and the master DOCX. Do not restart the project or repeat completed milestones.

The repository may contain valuable uncommitted work. Never discard, reset, clean, rebase, or overwrite it. Inspect it first.

## Product objective and non-negotiable boundaries

Build `folk park` Release 0.1 as an original Intel macOS `x86_64` wavetable instrument in both Standalone and VST3 formats. It combines a playable dual-wavetable synth, ordered effects, deterministic MIDI composition, native presets/history, and an optional guided AI sound-design workflow.

- It is not a Serum copy and must not import proprietary Serum state, bypass licensing, or ship unlicensed factory assets.
- Standalone and VST3 are both release requirements.
- FL Studio behavior is a human-verification boundary. Never report an FL check as passed unless the user actually ran it and confirmed it.
- Offline/manual workflows must remain complete. Provider AI is opt-in and must not require embedded user keys.
- No filesystem, JSON, SQLite, WebView, allocation, or new lock may enter the audio callback.
- A failed preset/history operation must not alter valid live audio state.

## Authoritative sources, in reading order

1. `AGENTS.md`
2. this file
3. `plans/RELEASE_0_1.md`
4. `docs/PROGRESS.md`
5. the milestone-specific contract, currently `docs/PRESET_FORMAT.md` and ADR-0007
6. `/Users/hermannpr/Downloads/folk_park_Codex_Master_Build_Prompt.docx`

The DOCX is the master product and engineering contract. Read it without modifying it, for example with `textutil -convert txt -stdout`. If it is missing, ask the user for its location instead of inventing requirements.

## Repository and Git state

- Private repository: `HermannPR/folk-park`.
- Working branch: `feat/m6-presets-history`.
- M6 is stacked on `feat/m5-effects-preview`; any M6 PR must use that exact base unless the documented branch topology intentionally changes.
- M5 draft PR: <https://github.com/HermannPR/folk-park/pull/5>.
- No M6 PR was open when this handoff was written.
- Integration checkpoint: `5a940aa Integrated preset and history workflows`.
- The intended checkpoint subject for the current hardening work is `Hardened M6 project recovery boundaries`. Resolve its final hash with `git log`; do not rely on a copied hash.
- A following bounded checkpoint uses the subject `Hardened M6 persistence and Save As boundaries`; resolve its final hash with `git log` after it is committed.
- Earlier M6 commits are:
  - `735fb84 Established the M6 persistence and migration contracts`
  - `a69a8bc Implemented versioned native presets and validated assets`
  - `abb25f7 Implemented transactional searchable composition history`
  - `ed2c1a7 Established the exact M6 integration handoff`

Git rules:

- Keep each meaningful stage buildable and commit it separately.
- Use concise impersonal subjects such as `Established ...`, `Implemented ...`, `Hardened ...`, or `Verified ...`; never `I established ...`.
- Review status and the exact diff before every commit.
- Stage exact paths only; never use `git add .` or `git add -A`.
- Never force-push, rewrite established history, expose the repository publicly, or add credentials/licensed assets.

## Completed milestones

- M0: repository/toolchain/risk spikes; automated gate passed.
- M1: playable vertical slice; automated gate passed; human host run pending.
- M2: dual wavetable/modulation/import; automated gate passed; human host checks pending.
- M3: deterministic composition and MIDI delivery; automated gate passed; FL drag/routing checks pending.
- M4: Silicon Dreams UI; automated gate passed. Real OSC A/B waveform visualization, optional frame/morph surface, spectrum feedback, four-octave C2-B5 piano, octave shift, and held-key repeat suppression are implemented.
- M5: ordered Distortion -> Chorus -> tempo-synced Delay -> Reverb -> Compressor -> Parametric EQ and isolated accepted-composition WAV rendering; automated gate passed; FL effects/render checks pending.

M5 recorded gate: UI 8/8, Debug 8/8, Release 9/9, pluginval 1.0.4 strictness 5 SUCCESS. VST3 SHA-256 `5ff08476376a4d37e0224623741c3e42a5e15cb52118b6259f33705c965126ac`; Standalone SHA-256 `c151eb0486992e6dd80f98dde2644b0281f44858481e3fa127a086a2fbd213e1`. See `evidence/m5/verification.md`.

Do not reimplement M0-M5. Preserve the oscillator displays, C2-B5 keyboard, key-repeat behavior, effects, MIDI, and accepted-only WAV workflow while working on M6.

## Current milestone: M6 native presets and composition history

M6 is integrated and its first project-recovery hardening stage passes Debug, but it is **not complete**. Foundation modules, processor coordination, native bridge, the React workspace, bounded host project state, imported-asset recovery, accepted-composition restore, dirty tracking, and explicit Save As semantics exist. The next agent must finish the remaining adversarial/UI review, run the complete Release gate, and document evidence before claiming M6 complete.

### Established foundation

- `schemas/preset-v1.schema.json` freezes the oldest supported schema version 1.
- `schemas/preset.schema.json` defines production schema version 2.
- `src/persistence/Preset.*` implements deterministic encoding, pure v1-to-v2 migration, strict bounded validation, duplicate-key rejection, atomic overwrite-safe storage, content-addressed assets, SHA-256 verification, missing-asset reporting, explicit relink, and traversal/symlink rejection.
- `src/persistence/CompositionJson.*` implements canonical versioned composition payloads.
- `src/persistence/HistoryRepository.*` implements a SQLite-backed repository with rollback-safe v1/v2 migrations, prepared statements, bounded search, exact recall, lineage, favorites, tags, soft deletion, retention preferences, and explicit cleanup.
- System SQLite is the documented dependency boundary; DSP does not depend on SQLite.

These are compatibility surfaces. Integrate or harden them; do not create a second preset codec or a second history database.

### Integration now implemented in the worktree/checkpoint

Build integration:

- The main Folk Park target and plugin integration tests compile `CompositionJson`, `HistoryRepository`, and `PersistenceCoordinator` and link `SQLite3::SQLite3`.

Persistence coordination:

- New `src/persistence/PersistenceCoordinator.h/.cpp` owns message-thread filesystem/SQLite work below one configurable root.
- Production defaults to the Folk Park macOS application-support location; tests can disable persistence or use a temporary root.
- Preset availability and history availability are separate. A failed SQLite database leaves native presets usable.
- The coordinator provides bounded library listing, save/load/import, content-addressed localization, missing-asset relink, favorites, current-preset status, history store/search/recall/inspect, soft deletion, retention, and cleanup.

Transactional audio publication and import:

- `SynthEngine` now has a producer-side `publishPresetSnapshot` for prepared oscillator A, oscillator B, and modulation routes at one audio-block boundary.
- Producer/consumer atomic handshakes avoid locks and waiting in `processBlock`.
- Existing single-wavetable and modulation publication remain compatible.
- `WavetableImportService` retains the selected source through review, passes it to the confirmed publisher, and keeps the pending import for retry when publication fails.

Processor/session integration:

- `PluginProcessor` has an explicit persistence configuration and lazy initialization.
- It captures the exact synth/mod/effect state, routes, metadata, and oscillator asset references into native presets.
- Preset loading prepares/validates both wavetables and routes before live publication; only successful publication updates APVTS/UI/current-preset state.
- Confirmed user WAV imports are stored as validated content-addressed assets before their references are recorded.
- Accepted compositions remain accepted even if history storage fails.
- Accepted history stores exact intent/macro/composition state, optional current preset, tags, timestamps, versions, and variation lineage where available.
- `CompositionSession::restoreAccepted` validates recalled compositions before replacing candidate/accepted state.
- History recall loads an associated preset first and does not mutate composition if that preset cannot be restored transactionally.

Native bridge and UI:

- `PluginEditor` exposes bounded, type-checked functions for status, preset list/save/load/import/relink/favorite, history search/recall/favorite/soft-delete/compare, retention, and cleanup.
- External preset and relink paths use explicit JUCE file choosers.
- Native payloads expose safe metadata/filenames rather than unrestricted personal paths.
- `ui/src/protocol.ts` strictly parses the append-only persistence snapshot and bounded workspace/comparison payloads while accepting older schema-v1 snapshots without the optional persistence field.
- `ui/src/PersistenceView.tsx` replaces the History placeholder with native preset metadata/save/import/load/favorite/relink, searchable composition history, recoverable trash, non-mutating comparison, recall, retention, and confirmed cleanup.
- `ui/src/App.tsx` routes the header preset control to this workspace and displays active/dirty preset status.

Tests added or extended:

- Restart integration with temporary persistence root: preset save/list/load and composition history survive a new processor instance; linked preset and accepted composition recall are checked.
- Database-unavailable isolation: a deliberately unusable database does not block acceptance, native presets, or finite audio.
- Real-time test queues a complete preset snapshot under allocation tracking and verifies malformed follow-up state is rejected without changing the active route.
- Import tests verify confirmed metadata and source retention.
- UI protocol and interface-contract tests cover the M6 bridge, recovery, compare, trash, confirmation, bounds, and lack of remote URLs.

## Verification actually completed for the M6 working checkpoint

The following results were recorded on 2026-08-23 on the integrated worktree:

- `git diff --check`: passed.
- UI production build: passed. Generated bundle was approximately 805.38 kB JS (209.03 kB gzip), 16.30 kB CSS.
- UI tests: 10/10 passed.
- UI lint: passed.
- Debug build: passed after integrating the new sources/tests.
- Full Debug CTest: 10/10 passed. The last command was `/Users/hermannpr/Library/Python/3.9/bin/ctest --preset macos-x86_64-debug --output-on-failure`.
- Real-time allocation test passed as part of the 10-test suite.
- The current C++ hardening stage completed another full Debug Standalone/VST3 build and 10/10 CTest pass after adding project-state recovery.
- Focused processor tests now use a real generated WAV to prove editor-independent parameter/wavetable/accepted-composition restore, bounded malformed/oversized rejection, no partial mutation on a missing asset, wrong-hash rejection, exact relink completion, dirty tracking, distinct Save As UUIDs, and explicit overwrite.
- Current UI build/test/lint passes 10/10 with an explicit Save-as-new versus replace-current workflow; generated JS is 806.08 kB (209.25 kB gzip) and CSS is 16.30 kB.
- Additional native hardening proves confirmed-import retry after a deliberately busy exchange, external preset asset localization and reload without its source directory, SQLite path symlink rejection with presets still available, and competing complete preset publication rejection. The subsequent full Debug build and CTest suite pass 10/10.

A first version of the restart integration test compared a requested normalized float against JUCE's quantized stored value and failed. The test was corrected to capture the actual parameter value before save; the targeted test and the subsequent full 10/10 suite passed. This was a test expectation issue, not a persistence failure.

Do not overstate this checkpoint:

- The integrated work has not yet received the complete M6 Release build/CTest/pluginval/artifact gate.
- No new M6 Standalone screenshots or `evidence/m6/verification.md` exist yet.
- No M6 FL Studio human checks are passed.
- The known JUCE Android compatibility helper warning still appears during direct UI bundle evaluation; it is not emitted by Folk Park source, but should remain documented.

## Known gaps and risks to examine before M6 completion

The first three integration risks were addressed: host state now contains a bounded versioned preset/composition payload and asset references; parameter callbacks use atomic-only revision tracking; and non-overwrite Save As creates a new UUID. README, progress, and the FL matrix now describe the M6 working state truthfully.

Remaining explicit review targets, not necessarily confirmed defects:

1. Atomic preset publication: invalid and competing-busy complete snapshots are covered, along with zero allocations. Finish any concurrency review needed for Release evidence; do not add an audio-thread mutex or wait.
2. Asset transactions: project missing/wrong/exact relink, external localization/independent reload, and confirmed-import busy retry are covered. Review only remaining multiple-asset failure ordering and orphan-safe behavior.
3. History boundaries: expand adversarial processor/bridge tests for malformed UUIDs, excessive strings/tags/results, unavailable/corrupt/future database, recall failure, retention/favorite preservation, and non-mutating compare. Repository-level coverage already exists for many of these; verify the exposed native boundary too.
4. Symlinks and root safety: preset root/assets and the SQLite file reject symbolic links; database failure leaves presets available. Review remaining root-as-file/unwritable/concurrent-change behavior.
5. UI behavior: source contracts and build pass, and Save As is now explicit. Visually verify loading/error/degraded states, active preset naming, overwrite confirmation, missing-asset recovery, soft-delete recovery, comparison, cleanup confirmation, keyboard accessibility, and narrow layout in the built Standalone.
6. State size/fallback: the custom project payload is bounded at 8 MiB and rejects malformed/oversized input transactionally. Confirm the largest valid composition plus preset remains below the serialized XML bound during the Release gate.

## Exact continuation sequence for the next coding agent

### 1. Reconcile; do not discard

Run:

```sh
pwd
git status --short --branch
git log --oneline --decorate -12
git diff --check
git remote -v
```

Confirm the branch is `feat/m6-presets-history`, inspect whether the `Integrated preset and history workflows` commit is present, and preserve any later work. Read all authoritative sources listed above. Check for more local `AGENTS.md` files before editing.

If the integration changes are unexpectedly uncommitted, inspect them rather than recreating them. The expected paths are:

- `CMakeLists.txt`
- `src/persistence/PersistenceCoordinator.h/.cpp`
- `src/midi/CompositionSession.h/.cpp`
- `src/plugin/PluginProcessor.h/.cpp`
- `src/plugin/PluginEditor.h/.cpp`
- `src/synth/SynthEngine.h/.cpp`
- `src/synth/WavetableImportService.h/.cpp`
- `tests/M0Tests.cpp`, `tests/PluginTests.cpp`, `tests/RealtimeTests.cpp`
- `ui/src/App.tsx`, `ui/src/PersistenceView.tsx`, `ui/src/protocol.ts`
- `ui/src/interface-contract.test.ts`, `ui/src/protocol.test.ts`, `ui/src/styles.css`
- generated `resources/ui/app.js`, `resources/ui/app.css`, and `resources/ui/index.html` when changed by the UI build

### 2. Finish the remaining M6 hardening review

Host project state, Save As, dirty tracking, and project missing-asset recovery are implemented and Debug-tested. Use the remaining known-gaps list as the checklist. Keep filesystem and database work off `processBlock`; add focused tests for any repaired boundary and retain exact error reporting.

The project-recovery checkpoint is `a67ec8a Hardened M6 project recovery boundaries`. The next bounded checkpoint is `Hardened M6 persistence and Save As boundaries`. If additional material hardening is needed, commit it separately with an accurate impersonal subject.

### 3. Run the complete M6 gate

Run UI build/tests/lint, complete Debug and Release builds/tests, architecture inspection, built VST3 smoke, signatures, installed artifact/hash parity, and pluginval at the required strictness. Inspect Standalone visually and retain evidence. Do not infer FL Studio success from pluginval.

Create/update:

- `evidence/m6/verification.md`
- bounded M6 evidence artifacts required by `plans/RELEASE_0_1.md`
- `docs/PROGRESS.md`
- `README.md` current/coming-soon language
- this handoff

Suggested commit subject: `Verified and documented the M6 checkpoint`.

Then push `feat/m6-presets-history` and open a private draft PR with base exactly `feat/m5-effects-preview`. Confirm the repository remains private before the push/PR action.

### 4. Do not begin M7 until M6 is truthfully gated

M7 is the guided sound-design assistant. It must ask adaptive questions, translate answers into bounded and explained proposals, provide reversible A/B audition, and require explicit user acceptance before applying changes. It may later propose chords, melody, bass, arpeggios, MIDI, samples, and sound parameters, but network/provider access remains opt-in and offline workflows remain complete.

## Commands and local environment

The user-local CMake tools may not be on `PATH`:

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

Only claim commands actually run. The final gate must follow the exact checks in the plan/progress documents, including `file` architecture, signature inspection, VST3 smoke, pluginval, installed artifact/hash parity, and retained evidence.

## Final safety and truthfulness checklist

- Never add credentials, private prompts, licensed plug-in binaries, Serum content/state, or third-party factory assets.
- Never access or modify unrelated personal files, installed plug-ins, or the user's FL Studio projects while developing this repository.
- Never silently overwrite a preset or permanently delete history without explicit bounded user action.
- Never expose raw SQL, unrestricted paths, or executable content through the native bridge.
- Never call a milestone complete because its UI renders.
- Distinguish automated proof, local visual inspection, and human listening/FL Studio verification in every report.
