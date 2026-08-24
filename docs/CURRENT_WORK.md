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
- Current branch: `feat/m8-release-hardening`, created at M7 handoff `eee67cf` and stacked exactly on `feat/m7-guided-assistant`.
- Any M8 draft PR must use base exactly `feat/m7-guided-assistant` unless the documented stacked topology intentionally changes.
- Any M7 draft PR must use base exactly `feat/m6-presets-history` unless the documented stacked topology intentionally changes.
- M5 draft PR: <https://github.com/HermannPR/folk-park/pull/5>.
- M6 private draft PR: <https://github.com/HermannPR/folk-park/pull/6>, base exactly `feat/m5-effects-preview`, head `feat/m6-presets-history`.
- Final M6 checkpoint: `b3e9e78 Verified and documented the M6 checkpoint`.
- M6 handoff checkpoint: `282c344 Established the M7 continuation handoff`.
- First M7 checkpoint: `74d6e43 Established M7 assistant and provider contracts`.
- Offline workflow checkpoint: `639a749 Implemented deterministic offline Jarvis workflows`.
- A/B audition checkpoint: `63191e8 Integrated reversible Jarvis proposal audition`.
- Connected Jarvis UI checkpoint: `de6db59 Implemented the offline Jarvis workspace`.
- Secure provider-settings checkpoint: `1415f70 Established secure Jarvis provider settings`.
- Final M7 checkpoint: `5711496 Verified and documented the M7 checkpoint`.
- M7 private draft PR: <https://github.com/HermannPR/folk-park/pull/7>, base exactly `feat/m6-presets-history`, head `feat/m7-guided-assistant`.
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
- M7: composition text, guided Jarvis sound workflow, reversible A/B, optional secure provider boundary, and full automated/Release evidence; automated checkpoint verified, with every FL Studio case still human-required.
- M8: host/release hardening, packaging, legal/asset audit, and release documentation; current continuation milestone, not yet implemented.

Do not reimplement M0–M7. Preserve the oscillator displays, C2–B5 keyboard, held-key repeat behavior, effects, candidate/accepted MIDI boundary, accepted-only WAV workflow, transactional persistence, and explicit Jarvis A/B boundary while working on M8.

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
- The recruiter-facing `README.md` explains the full product/architecture/status and embeds actual M6 and M7 Release screenshots. Never substitute concept art for product evidence.

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

## Completed milestone: M7 Jarvis text and guided sound assistant

The integrated Jarvis workspace is a working deterministic offline production helper, not a general-purpose LLM. It can interpret bounded composition text, guide sound intent, create explained proposals, and drive explicit A/B review. A native macOS Keychain abstraction now exists for a future opt-in provider, but no real remote provider or credential is selected/configured, and the UI states that boundary directly.

### Contracts and deterministic offline workflow now implemented

- `src/assistant/AssistantContracts.*` defines tagged composition/sound requests and responses across offline/mock/remote origins plus a non-audio asynchronous provider interface.
- Prompts are bounded at 1,024 characters; request/response UUID, target, origin, and typed payload must match; mixed variants and stale responses fail.
- Remote origin requires explicit consent on the submitted request.
- `schemas/parameter-proposal-v1.schema.json` preserves the M3 73-ID proposal format. The current `schemas/parameter-proposal.schema.json` and C++ model are v2 with a 102-change maximum.
- All proposal IDs resolve against the authoritative C++ catalog. V1 cannot address effects; v2 can address all M5 effect IDs. Unknown/duplicate IDs, non-finite/out-of-range values, missing v2 reasons/assumptions, unsupported versions, and disabled explicit acceptance fail.
- ADR-0008 freezes the offline composition, guided sound, A/B, provider-consent, credential, and no-real-adapter-yet decisions.
- Commit `74d6e43` freezes and pushes this first M7 contract checkpoint.
- `src/assistant/OfflineAssistant.*` implements deterministic composition-text parsing, stable two-at-a-time guided questions, current-to-proposed catalog mapping, explained assumptions/confidence, and a controlled mock provider.
- Guided intensity is optional until the producer answers it, preventing the previous neutral default from falsely completing that question.
- `tests/OfflineAssistantTests.cpp` covers determinism, natural-language bounds, real catalog mapping, current-value parity, describe/guided/manual modes, invalid snapshots, origin isolation, cancellation, collision, and at-most-once completion.
- Complete Debug build and CTest: PASS, 11/11. Focused assistant suites: PASS, 2/2. `git diff --check`: PASS.
- `src/assistant/AssistantAudition.*` owns immutable original/proposal comparison, strict stale/no-op rejection, reversible switching, and explicit accepted/rejected outcomes.
- `PluginProcessor` canonicalizes proposal targets to real APVTS legal steps, suppresses temporary-preview dirty tracking, invalidates on external host edits, and exposes non-audio begin/switch/accept/reject APIs.
- Host project session version 2 stores only an active bounded A/B comparison and restores it on its audible side without an editor; version 1 remains supported. Malformed assistant state rejects before live mutation.
- Focused assistant/processor recovery CTest: PASS, 2/2. Complete Debug build and CTest: PASS, 11/11, including real-time allocation coverage.

Implemented after the A/B checkpoint:

- Seven strict native operations expose assistant state, guided questions, sound-proposal creation, A/B switching, explicit decisions, and composition candidates.
- The `JARVIS` workspace provides a shared typed prompt, message transcript, describe/walkthrough modes, at-most-two-question steps, explained change review, reversible A/B, explicit accept/reject, and separate piano-roll composition review.
- UI protocol parsing rejects malformed/future/non-finite/duplicate/oversized responses before publication.
- UI lint/tests pass (14/14), the production bundle builds, Debug Standalone/VST3 build, and complete Debug CTest passes (11/11).
- Actual Debug Standalone visual inspection confirms the integrated tab and safe empty state. This is not a complete interaction, audible, Release, provider, or FL Studio pass.

Implemented after the connected UI checkpoint:

- `src/platform/CredentialStore.*` owns a bounded move-only credential and an exact macOS generic-password Keychain store with strict identifiers, device-only accessibility, exact update/read/remove behavior, and fail-closed Core Foundation request construction.
- `tests/CredentialStoreTests.cpp` performs a real temporary-service Keychain round trip and removes the exact test item. UI/native status exposes no credential value or secret-bearing JavaScript operation.
- Settings reports the complete current truth: deterministic offline mode active, remote provider not selected, no credential configured, Keychain supported, and no Jarvis data leaving the Mac.
- Frontend UUID validation now matches the native deterministic opaque-ID format, and stable kebab-case question IDs map explicitly to React answer fields.
- UI tests/lint/build: PASS, 15/15; complete Debug Standalone/VST3 and CTest: PASS, 12/12.
- Actual Debug interaction confirms guided progress and input persistence, restored proposal review, A/B selection/rejection, and provider/privacy status. This is not an audible, Release, remote-provider, or FL Studio pass.

### Final M7 automated gate

- Clean UI install/audit and UI tests/lint/build: PASS, 0 vulnerabilities and 15/15 tests.
- Complete Debug Standalone/VST3 build and CTest: PASS, 12/12.
- Complete Release Standalone/VST3 build and CTest: PASS, 13/13, including packaged VST3 finite-audio smoke and native Keychain coverage.
- pluginval 1.0.4 strictness 5: `SUCCESS`; Release/installed VST3 architecture, deep/strict local signature, hash parity, and independent installed-bundle render: PASS.
- Release/installed VST3 SHA-256: `b17c88bab2c1356c7b01980b96f918a28acbdd337f7ee2e437f9c63a7d7119ca`.
- Release Standalone SHA-256: `4523ffa815cfcdd7fb4d666644f75dde82869f6ebf673f9707f31314c8d3b1da`.
- Source origin/eval/token, JSON-schema, and diff checks: PASS.
- Real Release interaction: PASS for provider/privacy status, explained proposal creation, A/B selection, rejection restoring A, and guided progress to the next question pair. This is not an audible, provider-network, physical-device, or FL Studio pass.
- Evidence: `evidence/m7/verification.md`, pluginval log, and actual Release screenshots. Every FL Studio case remains `HUMAN RUN REQUIRED`.

M7 closure sequence:

1. Final M7 evidence/README checkpoint `5711496` is committed and pushed.
2. Private draft PR #7 is open with base exactly `feat/m6-presets-history`.
3. Begin M8 release-candidate hardening from this verified boundary; keep every FL Studio check HUMAN RUN REQUIRED until the user runs it.

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

Confirm the repository is still private, inspect every local change, and never discard work. Final M7 checkpoint `5711496` and draft PR #7 are pushed; only this M8 continuation handoff should be uncommitted before its own checkpoint. Clean UI/audit passes with 15/15 tests, Debug CTest passes 12/12, Release CTest passes 13/13, pluginval strictness 5 succeeds, installed/build VST3 parity and independent render pass, and real Release screenshots are retained. Review exact diffs, stage only the intended handoff paths, and use the impersonal subject `Established the M8 continuation handoff`.

The M6 and M7 draft PRs already exist and the branches are correctly stacked. Preserve meaningful impersonal commits at every passing stage. The next boundary is M8 release hardening; do not enable a real remote adapter by assumption.

## Current milestone: M8 release-candidate hardening

M8 starts from the verified M7 artifacts; it must not redesign or reimplement M0–M7. Its job is to turn the private engineering build into a supportable release candidate while keeping unresolved legal/distribution and physical-host claims explicit.

### Exact M8 start

1. Re-run the read-only repository/private/PR checks at the top of this file and confirm a clean worktree at `5711496` plus this handoff commit.
2. Create `feat/m8-release-hardening` from `feat/m7-guided-assistant`. Any stacked M8 PR must use base exactly `feat/m7-guided-assistant` unless topology is deliberately documented.
3. Freeze an M8 verification/checklist document before changing runtime code. Reuse the M7 gate as the regression baseline: UI 15/15, Debug 12/12, Release 13/13, pluginval strictness 5, installed VST3 parity/render, and source/security/schema scans.
4. Audit release-candidate behavior: x86_64 CPU/memory baselines, long-run/stuck-note/finite-output behavior, UI open-close/reload/focus recovery, preset/history/database/provider failure isolation, corrupted-state diagnostics, installation/repair/uninstall instructions, and actionable non-secret diagnostics.
5. Prepare the human FL Studio run from `docs/FL_STUDIO_TEST_MATRIX.md`. An agent may improve instructions or instrumentation, but only Hermann can convert a row from `HUMAN RUN REQUIRED` to passed by actually running it and recording the result.
6. Audit `LICENSES.md`, original/user-owned assets, bundle identity, privacy statements, and documentation. Stop for product-owner decisions in `docs/OPEN_DECISIONS.md`; do not choose a JUCE distribution license, legal identity, signing certificate, notarization credentials, installer/update channel, asset rights, or remote provider by assumption.
7. Add packaging/signing/notarization automation only after the corresponding owner decisions and credentials exist. Never commit certificates, passwords, API keys, notarization profiles, or provider secrets.
8. Finish installation, routing, recovery, troubleshooting, known-limitations, privacy, and release notes. Run the complete clean Release gate again, retain new M8 evidence, and keep the repository private until every distribution gate is explicitly resolved.

### M8 non-negotiable boundaries

- Preserve the current parameter IDs, schemas, preset/project migrations, candidate/accepted MIDI boundary, reversible assistant A/B state, and exact missing-asset recovery.
- Keep Standalone and VST3 `x86_64`; do not claim Apple Silicon or another plug-in format.
- Do not add a remote provider because Keychain exists. Provider selection remains open decision 5 and requires a separate privacy/consent review.
- Do not claim signing/notarization, audible quality, FL compatibility, physical MIDI/audio-device behavior, or distribution readiness without exact evidence.
- Keep the recruiter README factual and update screenshots only with real Release artifacts.

### M8 checklist checkpoint in progress

- `docs/M8_RELEASE_CHECKLIST.md` freezes defect severity, deliverables/evidence, diagnostics privacy, exact automated gates, and owner/human stop conditions.
- ADR-0009 fixes a deterministic 4 KiB diagnostics preview, fixed sanitized codes/counters, callback-safe atomics, exact preview-before-copy enforcement, and the engineering-candidate label.
- No runtime code, distribution setting, owner decision, or FL Studio status changed in this checklist stage.
- Next implementation boundary: the typed native diagnostics model, processor fault counters, strict bridge/UI preview-copy workflow, and adversarial tests.

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
