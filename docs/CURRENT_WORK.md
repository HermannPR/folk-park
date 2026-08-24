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
- A failed preset/history/assistant operation must not alter valid live audio state.
- The assistant may propose only typed, bounded intent/parameters and must require explicit producer acceptance before applying changes.

## Authoritative sources, in reading order

1. `AGENTS.md`
2. this file
3. `plans/RELEASE_0_1.md`
4. `docs/PROGRESS.md`
5. the milestone contract/ADR; M7 starts with `docs/PRODUCT_AMENDMENTS.md`, `docs/AI_PROVIDER_SECURITY.md`, `docs/PARAMETER_CATALOG.md`, and the assistant schemas/models
6. `/Users/hermannpr/Downloads/folk_park_Codex_Master_Build_Prompt.docx`

The DOCX is the master product and engineering contract. Read it without modifying it, for example with `textutil -convert txt -stdout`. A repository copy also exists at `docs/source/folk_park_Codex_Master_Build_Prompt.docx`; reconcile both before using the copy. If the authoritative download is missing, ask the user instead of inventing requirements.

## Repository and Git state

- Private repository: `HermannPR/folk-park`.
- Current branch: `feat/m7-guided-assistant`, stacked exactly on `feat/m6-presets-history`.
- Any M7 draft PR must use base exactly `feat/m6-presets-history` unless the documented stacked topology intentionally changes.
- M5 draft PR: <https://github.com/HermannPR/folk-park/pull/5>.
- M6 private draft PR: <https://github.com/HermannPR/folk-park/pull/6>, base exactly `feat/m5-effects-preview`, head `feat/m6-presets-history`.
- Final M6 checkpoint: `b3e9e78 Verified and documented the M6 checkpoint`.
- M6 handoff checkpoint: `282c344 Established the M7 continuation handoff`.
- No M7 PR exists at this contract checkpoint.
- Earlier M6 commits are:
  - `735fb84 Established the M6 persistence and migration contracts`
  - `a69a8bc Implemented versioned native presets and validated assets`
  - `abb25f7 Implemented transactional searchable composition history`
  - `ed2c1a7 Established the exact M6 integration handoff`
  - `5a940aa Integrated preset and history workflows`
  - `a67ec8a Hardened M6 project recovery boundaries`
  - `9f7f684 Hardened M6 persistence and Save As boundaries`

Git rules:

- Keep each meaningful stage buildable and commit it separately.
- Use concise impersonal subjects such as `Established ...`, `Implemented ...`, `Hardened ...`, or `Verified ...`; never `I established ...`.
- Review status and the exact diff before every commit.
- Stage exact paths only; never use `git add .` or `git add -A`.
- Never force-push, rewrite established history, expose the repository publicly, or add credentials/licensed assets.

## Milestone status

- M0: repository/toolchain/risk spikes; automated gate passed.
- M1: playable vertical slice; automated gate passed; human host run pending.
- M2: dual wavetable/modulation/import; automated gate passed; human host checks pending.
- M3: deterministic composition and MIDI delivery; automated gate passed; FL drag/routing checks pending.
- M4: Silicon Dreams UI, real A/B visuals, four-octave C2–B5 piano, octave shift, and held-key repeat suppression; automated gate passed; FL UI/input checks pending.
- M5: ordered Distortion → Chorus → tempo-synced Delay → Reverb → Compressor → Parametric EQ and isolated accepted-composition WAV rendering; automated gate passed; FL effects/render checks pending.
- M6: native presets/assets/migration, transactional composition history, and editor-independent project recovery; automated gate verified; every FL persistence case remains human-required.
- M7: composition text, guided Jarvis sound workflow, reversible A/B, and optional secure provider boundary; contract checkpoint in progress.
- M8: host/release hardening, packaging, legal/asset audit, and release documentation; planned.

Do not reimplement M0–M6. Preserve the oscillator displays, C2–B5 keyboard, held-key repeat behavior, effects, candidate/accepted MIDI boundary, accepted-only WAV workflow, and transactional persistence while working on M7.

## M6 implementation now established

### Native presets and assets

- `schemas/preset-v1.schema.json` freezes the oldest supported schema version 1; `schemas/preset.schema.json` defines production version 2.
- `src/persistence/Preset.*` owns deterministic encoding, pure v1→v2 migration, strict bounded validation, duplicate-key rejection, atomic explicit-overwrite storage, content-addressed assets, SHA-256/size validation, exact missing-asset relink, and traversal/symlink rejection.
- Presets capture all 102 normalized parameters, modulation routes, ordered effects, metadata, and up to two imported user-WAV asset references.
- Save As creates a new stable UUID by default. Replacing the current library preset is a separate explicit action that retains its UUID.
- External preset assets are localized into the application store and reload without their original directory.

### Transactional history

- `src/persistence/HistoryRepository.*` provides rollback-safe SQLite migrations, prepared statements, bounded search, exact recall, lineage, favorites/tags, non-mutating comparison, recoverable soft deletion, retention preferences, and explicit cleanup.
- Database availability is independent from presets and acceptance. A missing/unusable/symlink database cannot stop finite audio, native preset use, project state, or composition acceptance.
- `src/persistence/PersistenceCoordinator.*` owns all filesystem/SQLite work on non-audio threads below one configurable validated root.

### Host project recovery

- Bounded versioned `FolkParkProjectSession` state records the complete deterministic native preset payload, imported asset references, dirty status, optional accepted composition, and history lineage without depending on an editor.
- Malformed/oversized payloads are rejected before mutation.
- Missing assets leave current parameters, wavetables, and composition unchanged while exposing an explicit recovery request.
- Wrong hash/size is rejected; only an exact explicit relink completes the pending sound-and-composition transaction.
- APVTS parameter listeners perform only an atomic revision increment for dirty tracking.
- Complete preset publication prepares oscillator A, oscillator B, and modulation routes before one audio-block boundary exchange. Busy/invalid publication does not partially mutate audio.
- Confirmed WAV import is retained for retry after a deliberately busy exchange.

### Native bridge and UI

- `PluginEditor` exposes bounded functions for persistence status, preset list/save/load/import/relink/favorite, history search/recall/favorite/soft-delete/compare, retention, and cleanup.
- Native payloads expose safe metadata/filenames rather than unrestricted personal paths.
- `ui/src/protocol.ts` rejects malformed/future/non-finite/duplicate/oversize snapshots before view replacement.
- `ui/src/PersistenceView.tsx` implements the History workspace with availability/degraded state, explicit Save As/replace semantics, preset browser, recovery, history search/compare/recall/trash/retention, and confirmed cleanup.
- The recruiter-facing `README.md` explains the full product/architecture/status and embeds actual M6 Release screenshots.

## M6 verification actually completed

The following gate was recorded on 2026-08-23:

- Clean `npm ci --ignore-scripts` and `npm audit --omit=dev`: PASS, 0 vulnerabilities.
- UI build/tests/lint: PASS, 10/10. `app.js` 806.08 kB (209.25 kB gzip), `app.css` 16.30 kB, local index 0.40 kB.
- Complete Debug build and CTest: PASS, 10/10.
- Complete Release build and CTest: PASS, 11/11, including packaged VST3 scanning/instantiation/finite-stereo MIDI rendering.
- Standalone and VST3: thin Mach-O `x86_64`.
- Release/installed VST3 ad-hoc signature: deeply/strictly verified.
- pluginval 1.0.4 strictness 5: `SUCCESS` across editor lifecycle, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples.
- Installed/build VST3 hash parity and independent installed-bundle MIDI smoke: PASS.
- Release/installed VST3 SHA-256: `9b0fb548a4844b4384742e02248682fde8ffa479a19b9066c953dabc8c6572dc`.
- Release Standalone SHA-256: `3e4cf0d884ad8a770100e7cc34ac6281959879acaf1495c9d03fefd79b1f810f`.
- Source dev-origin, sensitive-token-pattern, JSON-schema, and `git diff --check` gates: PASS. JSON Schema IDs are the only scanned project-source HTTP URLs.
- Actual Release Standalone visual inspection: PASS for bridge readiness, Synth, Compose, FX, History, and clean close. This is not an audible/physical-device/FL pass.
- Evidence: `evidence/m6/verification.md`, the strictness-5 pluginval log, and four actual M6 screenshots.

Known verification observations:

- pluginval's optional separate Steinberg-validator subtest is skipped because no validator executable is installed.
- Vite reports direct `eval` only in JUCE's pinned Android compatibility helper; Folk Park source does not call it and the macOS UI uses bundled local resources.
- The private Standalone engineering artifact is unsigned. Distribution signing/notarization remains M8.
- A concurrent first CMake/Vite attempt temporarily lost the generated embedded index; the required serial UI build → CMake reconfigure/build and complete gate passed. Keep these steps serial.
- Every FL Studio test remains `HUMAN RUN REQUIRED`; see `docs/FL_STUDIO_TEST_MATRIX.md`.

## Current milestone: M7 Jarvis text and guided sound assistant

Do not treat the grey M7 UI hint or the passing contract models as a working assistant/LLM. There is no offline parser, question/proposal engine, A/B processor integration, conversation UI, credential implementation, or real provider yet.

### Contract checkpoint now implemented in the worktree

- `src/assistant/AssistantContracts.*` defines tagged composition/sound requests and responses across offline/mock/remote origins plus a non-audio asynchronous provider interface.
- Prompts are bounded at 1,024 characters; request/response UUID, target, origin, and typed payload must match; mixed variants and stale responses fail.
- Remote origin requires explicit consent on the submitted request.
- `schemas/parameter-proposal-v1.schema.json` preserves the M3 73-ID proposal format. The current `schemas/parameter-proposal.schema.json` and C++ model are v2 with a 102-change maximum.
- All proposal IDs resolve against the authoritative C++ catalog. V1 cannot address effects; v2 can address all M5 effect IDs. Unknown/duplicate IDs, non-finite/out-of-range values, missing v2 reasons/assumptions, unsupported versions, and disabled explicit acceptance fail.
- ADR-0008 freezes the offline composition, guided sound, A/B, provider-consent, credential, and no-real-adapter-yet decisions.
- The focused assistant target builds and `FolkParkAssistantSchemaModelTests` passes 1/1 after these changes.

Next M7 sequence:

1. Preserve the current contract work and commit it with an impersonal subject after exact diff/test review.
2. Implement the deterministic offline composition parser, adaptive guided-question/proposal engine, and a mock provider before processor/UI integration.
3. Add adversarial tests for sanitization, ambiguity/follow-ups, prompt bounds, determinism, catalog mapping, stale/cancelled requests, provider failure, and explicit acceptance.
4. Then integrate reversible A/B and explicit accept/reject into processor state without adding callback work.

M7 required producer workflow:

- The producer can type a sound goal and receive adaptive focused questions about timbre, motion, envelope, range, effects, and musical use.
- Offline deterministic logic must provide a useful walkthrough without any account or API key.
- Answers become a bounded explained `ParameterProposal` validated against the stable parameter catalog.
- The producer can audition original versus proposal as reversible A/B, refine it, and explicitly accept or reject it.
- No proposal silently applies, changes the DAW, executes code, or bypasses schema/catalog limits.
- Optional network/provider support is opt-in, cancellable, clearly offline when unavailable, and cannot receive secrets, unrestricted paths, full project state, raw personal files, or unbounded prompt/tool output.
- Provider credentials use the documented Keychain abstraction; never store credentials in presets, project state, logs, commits, or frontend JavaScript.
- Audio must continue if the UI, assistant, provider, network, database, or persistence root fails.

M7 should add adversarial tests for malformed/oversized proposals, unknown/duplicate parameters, non-finite values, cancellation/timeout, stale session responses, provider failure, explicit acceptance, A/B restoration, project/editor reload, and zero callback allocations.

## Exact continuation sequence

Run first:

```sh
pwd
git status --short --branch
git log --oneline --decorate -12
git diff --check
git remote -v
gh repo view HermannPR/folk-park --json visibility,nameWithOwner
gh pr list --head feat/m6-presets-history --state all
gh pr list --head feat/m7-guided-assistant --state all
```

Confirm the repository is still private, inspect every local change, and never discard work. If the M7 contract changes are uncommitted, run the focused assistant test plus full Debug suite, review exact diffs, and stage only the intended contract/schema/test/documentation paths. The expected first M7 subject is `Established M7 assistant and provider contracts`.

The M6 draft PR already exists and this branch is correctly stacked. Review/commit the M7 contract checkpoint if it is still uncommitted, then continue with the offline engine checkpoint. Prefer meaningful impersonal commits at every passing stage.

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
cd ui && npm ci --ignore-scripts && npm audit --omit=dev && npm run build && npm test && npm run lint
./scripts/test.sh
./scripts/install_user_vst3.sh release
```

Only claim commands actually run. Keep UI generation and CMake configure/build serial because Vite intentionally replaces `resources/ui` during its production build.

## Final safety and truthfulness checklist

- Never add credentials, private prompts, licensed plug-in binaries, Serum content/state, or third-party factory assets.
- Never access or modify unrelated personal files, installed plug-ins, or the user's FL Studio projects while developing this repository.
- Never silently overwrite a preset or permanently delete history without explicit bounded user action.
- Never report FL Studio, audible quality, provider security, distribution signing, licensing, or release readiness as passed without its exact required evidence.
- The repository must remain private until the user explicitly decides otherwise and all distribution gates are closed.
