# Progress

## Current checkpoint

- Milestone: M8 — release-candidate hardening and distribution audit
- Status: M8 complete automated checkpoint passes; real diagnostics screenshot, all FL Studio human runs, and owner distribution decisions remain required
- Date: 2026-08-24 (America/Monterrey)
- Branch: `feat/m8-release-hardening`, stacked exactly on `feat/m7-guided-assistant`

## Post-M8 Compose control repair

- Reproduced the reported black WebView after changing Compose macro controls as a React event-lifetime failure: the six macro handlers read `event.currentTarget` from inside deferred functional state updaters, after React was permitted to clear the event target.
- Captured each primitive range/checkbox value synchronously before scheduling state updates. Applied the same correction to all six Compose macros, all four part selectors, and both Settings presentation toggles that shared the unsafe pattern.
- Added an interface regression contract that rejects `event.currentTarget` access inside the affected deferred updaters.
- UI tests/lint/build: PASS, 18/18. Reconfigured and rebuilt the embedded Release UI, Standalone, and VST3; complete Release CTest: PASS, 16/16, including packaged VST3 scan/instantiate/finite-audio rendering.
- pluginval 1.0.4 strictness 5: `SUCCESS`, including editor lifecycle, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples. Retained log: `evidence/m8/pluginval/pluginval-compose-repair-strictness-5.txt`.
- The exact repaired VST3 was installed with executable SHA-256 `823bf765a1744b7de6e8232ef17ad4d93d209628e01dfd6f716b2dff14b0131d`; architecture, deep/strict local signature, build/install parity, and an independent installed-bundle finite-stereo MIDI render pass.
- The former installed bundle is preserved at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T140808Z`. No presets, songs, history, user wavetables, exports, Application Support data, or other plug-ins were modified.
- Producer interaction in the real Standalone and FL Studio remains human-required; automated validation does not convert either control-interaction row to PASS.

## M8 checklist checkpoint

- Froze the M8 release-candidate deliverables, evidence requirements, Critical/High defect gate, and the exact distinction between automated, owner-decision, and FL Studio human evidence.
- Defined a deterministic 4 KiB opt-in diagnostic preview containing bounded configuration, sanitized subsystem codes, and lock-free fault counters only.
- Prohibited prompts, credentials, paths, filenames, project/preset identity, audio/MIDI content, database rows, clipboard contents, persistence, and provider transmission from diagnostics.
- ADR-0009 requires native enforcement of exact preview-before-copy and preserves message-thread-only formatting/clipboard work.
- No signing, licensing, provider, distribution, or FL Studio decision is inferred by this checkpoint.

## M8 bounded diagnostics checkpoint

- Added a deterministic native report below 4 KiB with build type/version, x86_64 architecture, wrapper/host when safely supplied, sample rate, block size, active voices, fixed subsystem codes, and typed fault counters.
- Host/build text is length-bounded and stops before line/path separators. Reports exclude paths, filenames, project/preset identity, prompts, audio/MIDI content, persistence rows/messages, provider data, credentials, and arbitrary workflow errors.
- Converted active audio setup to atomic publication and added callback-safe relaxed counters for final non-finite containment, direct/preview MIDI queue overflow, and rejected host project state. Callback work remains bounded and does not format diagnostics.
- Added an opaque preview session: only the exact current preview ID can retrieve the exact previewed text for native clipboard copying. React rejects malformed/oversized previews and disables Copy until Preview succeeds.
- UI contracts/build: PASS, 16/16. Complete Debug Standalone and VST3 build: PASS. Complete Debug CTest: PASS, 13/13. Focused diagnostics and processor integration: PASS. `git diff --check`: PASS.
- This is a Debug implementation checkpoint, not a final M8 Release gate. The M7 Release/pluginval/install evidence remains current; every FL Studio row remains `HUMAN RUN REQUIRED`.

## M8 runtime-hardening Debug checkpoint

- Added a deterministic runtime suite with a 12-second routine mode and bounded `FOLK_PARK_RUNTIME_SECONDS=120` evidence mode.
- The suite checks every output sample for finiteness across repeated note cycles and all six effects, executes panic, waits for zero active voices, proves preview held-key idempotence and full-queue recovery, and proves direct-MIDI Stop emits the tracked note-off and clears pending/playing state.
- Reconstructed and destroyed the bundled editor three times while a host-held note continued through the processor callback; audio and voice ownership remained independent from WebView lifetime.
- Final extended Debug run: PASS — 11,250 blocks, 120 simulated seconds, 48 kHz/512, four notes, 2×2 unison, all effects, one panic, 87,280.9 ms internal elapsed, `0.727341×` realtime.
- Complete Debug Standalone/VST3 build: PASS. Complete Debug CTest: PASS, 14/14. Retained evidence: `evidence/m8/runtime-hardening-debug.md`.
- Removed the legacy arbitrary `<4×` CPU failure threshold while retaining finite output, positive measurement, zero callback allocations, and printed performance evidence. An owner-approved CPU budget remains unresolved.
- No FL Studio, listening, physical device, Release parity, distribution, provider, or legal status changed.

## M8 supportability and provenance checkpoint

- Added read-only VST3 verification for exact bundle shape, thin `x86_64`, deep/strict signature, executable SHA-256, and size.
- Made first install refuse an existing destination by default. Explicit `--replace` preserves a timestamped rollback, verifies copy hash parity, retains a failed candidate, and restores the previous bundle on verification failure.
- Added dry-run-by-default uninstall that moves only the exact user VST3 to a unique Trash item on `--execute`; it never removes Application Support or producer work.
- Added a full support playbook for backup, build, verify, install, FL rescan, repair, rollback, uninstall, diagnostics, no-sound/stuck-note/WebView/persistence/project-state recovery, and human evidence fields.
- Added explicit privacy and private-packaging documents plus consolidated runtime notices and project asset inventory.
- Added a read-only audit for the pinned JUCE/VST3 license files, exact bundled UI runtime versions/licenses, unexpected tracked runtime media/fonts, and external UI origins. Audit and shell contract tests: PASS.
- Made concurrent drag MIDI paths unique and added guarded editor cleanup for only its exact app-generated temp file. UI contracts: PASS, 17/17. Focused MIDI/support/processor CTest: PASS, 3/3.
- Installer dry-run verified the existing retained M7 Release VST3: thin `x86_64`, deep/strict signature verification, executable SHA-256 `b17c88bab2c1356c7b01980b96f918a28acbdd337f7ee2e437f9c63a7d7119ca`; no install or replacement was executed.
- Complete Debug Standalone/VST3 build: PASS. Complete Debug CTest: PASS, 15/15 in 16.23 seconds. UI lint/contracts: PASS, 17/17.
- JUCE license route, legal identity, public privacy notice, signing/notarization, installer/update channel, asset approval, provider, CPU budget, and every FL result remain unresolved owner/human gates.

## M8 final automated verification checkpoint

- Clean pinned UI install and production audit: PASS, 34 packages and 0 vulnerabilities. UI tests/lint/build: PASS, 17/17.
- Complete Release Standalone/VST3 build and CTest: PASS, 16/16 in 7.83 seconds, including the packaged VST3 scan/instantiate/finite-stereo MIDI render.
- Extended Release recovery run: PASS, 11,250 blocks / 120 simulated seconds at 48 kHz/512 in 18,367 ms (`0.153058×` realtime) with repeated notes, 2×2 unison, all effects, panic/release, preview overflow recovery, direct-MIDI Stop, and editor reconstruction.
- Release VST3 and Standalone are thin Mach-O `x86_64`; the VST3 local ad-hoc signature verifies deeply/strictly. The unsigned Standalone remains a private engineering artifact.
- pluginval 1.0.4 strictness 5: `SUCCESS`; log retained at `evidence/m8/pluginval/pluginval-release-strictness-5.txt`.
- Installed user VST3: PASS; installed/build hash parity, architecture/signature verification, and independent MIDI rendering pass. The previous bundle remains as timestamped rollback; no Application Support or producer data was touched.
- Release/installed VST3 SHA-256: `9295e582e705837020f72f657105d5efd2213d5e8904dee628d7e55e52a82a84`.
- Release Standalone SHA-256: `bb61054c5acf8f9fb3711acd49220dc6ddcf6508d4ea4bc5513d6e82c1778386`.
- Release-material, development-origin, authored-source `eval`, sensitive-token-form, seven-schema, private-repository, and diff gates pass. The one embedded `eval` belongs to JUCE's pinned Android user-script compatibility branch and is not invoked by the Intel macOS target.
- Full result, methods, limitations, and hashes: `evidence/m8/verification.md`. The real Release diagnostics screenshot awaits the producer-visible Preview action because macOS denied accessibility automation; no mock visual or clipboard bypass is accepted.
- Every FL Studio row remains `HUMAN RUN REQUIRED`. Owner decisions for CPU budget, JUCE distribution licensing, signing/notarization, identity, distribution, privacy notice, asset approval, and remote provider remain unresolved.

## M8 owner decision-readiness checkpoint

- Added `docs/OWNER_RELEASE_DECISIONS.md` as a non-authorizing worksheet for private/closed-beta/public/commercial intent; JUCE 8 licensee/route/eligibility/seats; stable product/legal identity; Apple Developer ID/notarization; ZIP/DMG/PKG and updates; public privacy; asset/name approval; FL-informed CPU budget; provider scope; and the no-Serum-state compatibility statement.
- Mapped the exact current facts: `folk park` 0.1.0, `com.folkpark.audio.folkpark`, `FlPk/FkP1`, JUCE 8.0.13 at the pinned commit, ad-hoc VST3, unsigned Standalone, no notarization/installer/updater/network/provider, and private repository status.
- Verified the controlling JUCE 8 EULA currently publishes Starter/Indie/Pro/Educational thresholds and terms, makes the Product Owner responsible for licensing, and defines Framework User seats. Agent-assisted development is not inferred; written JUCE clarification is required if relevant.
- Verified Apple currently requires program membership, Developer ID signing, hardened-runtime/notarization preparation, and current notarization tooling for a normal outside-App-Store distribution path. No certificate, account, credential, purchase, signing, submission, or publication was accessed or performed.
- Recommended—but did not authorize—the low-risk order: private review and FL smoke → owner/license/identity decisions → signed/notarized manual ZIP beta → PKG only after install/rollback evidence; offline-only/manual-update for 0.1.

## M7 contract checkpoint

- Added versioned typed `AssistantRequest`/`AssistantResponse` variants for composition versus sound and offline/mock/remote processing origins.
- Bounded prompts at 1,024 characters, required matching UUID/target/origin/typed context, rejected mixed variants and stale responses, and made per-request consent mandatory for remote origin.
- Added one asynchronous non-audio `AssistantProvider` interface with cancellation and at-most-once completion semantics; no real provider or network dependency is selected yet.
- Preserved the original 73-ID proposal contract as `parameter-proposal-v1.schema.json` and migrated the current proposal schema/model to v2 for all 102 host parameters.
- Resolved every proposed ID against `src/common/ParameterIds.h`; v1 rejects effect IDs, v2 accepts the complete catalog, and unknown/duplicate IDs, invalid values, missing reasons, and implicit acceptance are rejected.
- Recorded ADR-0008 for offline-first composition text, guided sound A/B, provider consent, credential boundaries, and the open real-provider decision.
- Focused assistant model/contract build and CTest: PASS, 1/1.

## M7 deterministic offline workflow checkpoint

- Added a deterministic, bounded composition-text mapper for key, scale, bars, BPM, requested parts, genre, emotion, density, rhythm complexity, repetition, and variation. It normalizes through the existing `MusicIntent` validator and produces a candidate only; it does not deliver into the DAW.
- Added stable guided sound topics with no more than two focused questions per step. The seven required answers are tracked independently so a missing intensity is not confused with a neutral default.
- Added an offline proposal mapper that compares captured current host values against bounded proposed values, uses only real parameter-catalog IDs, explains every change, records assumptions/confidence, and preserves explicit acceptance.
- Added current-parameter snapshot rejection for empty/oversized, unknown, duplicate, non-finite, and out-of-range values.
- Added a deterministic mock provider with controlled pending completion, origin validation, cancellation, and at-most-once callback behavior. The offline engine rejects remote-origin execution.
- Focused assistant contract/workflow CTest: PASS, 2/2.
- Complete Debug build: PASS for native tests, Standalone, and VST3.
- Complete Debug CTest: PASS, 11/11.
- `git diff --check`: PASS.

## M7 reversible A/B and project recovery checkpoint

- Added an assistant audition state machine with immutable proposal data, original/proposal sides, exact relevant-live-value validation, explicit accept/reject outcomes, and safe invalidation when a host edit makes the comparison stale.
- Canonicalized proposed normalized values through each real APVTS parameter's legal range/step before audition, covering logarithmic, integer, choice, and boolean host controls without false stale detection.
- Temporary A/B switches notify the host but do not mark the native sound dirty. Rejection restores the exact captured A values and original dirty boundary; explicit acceptance finishes on B and marks the sound dirty once.
- Native preset saving is blocked while an A/B decision is pending. A successful preset load/reset supersedes the comparison only after the new preset applies.
- Migrated the bounded host project session from version 1 to version 2 while retaining version-1 restore. An active A/B proposal, status, audible side, assumptions, explanations, and exact values survive editor-independent project save/reopen.
- Strict project parsing rejects unsupported status/side/schema, unknown properties/children, malformed text/flags, duplicate/unknown catalog IDs, non-finite/out-of-range values, no-op/stale proposals, and oversized collections before live mutation.
- Focused offline-assistant and processor-state CTest: PASS, 2/2.
- Complete Debug build: PASS for native tests, Standalone, and VST3.
- Complete Debug CTest: PASS, 11/11, including the unchanged zero-allocation callback suite.
- `git diff --check`: PASS.

## M7 native bridge and conversation workspace checkpoint

- Added seven bounded native operations for assistant state, stable guided questions, sound-proposal creation, A/B switching, explicit acceptance/rejection, and composition-candidate creation.
- Kept all offline assistant execution and parameter snapshotting outside the audio callback. Sound creation begins on original A; composition text creates a candidate only.
- Replaced the disabled header hint with a functional `JARVIS` workspace and shared quick-entry field.
- Added a producer-readable message form, sound/composition modes, describe/guided entry, no-more-than-two-question steps, deterministic seed, proposal explanation/confidence/assumptions/change review, reversible A/B controls, and explicit decisions.
- Made the offline boundary visible in-product: no account, API key, network request, hidden edit, or general-purpose LLM is claimed at this checkpoint.
- Added strict UI parsing for bounded guided progress, UUID-linked proposals, unique finite parameter changes, explicit acceptance, known status/side values, and candidate-only composition results.
- Added processor orchestration and adversarial UI/interface tests. UI TypeScript/lint: PASS; UI tests: PASS, 14/14.
- Production UI bundle: PASS; `app.js` 824.71 kB (213.20 kB gzip), `app.css` 21.11 kB, and local index 0.40 kB. The known direct-eval warning remains confined to JUCE's pinned Android compatibility helper.
- Debug Standalone and VST3 build: PASS. Complete Debug CTest: PASS, 11/11, including the unchanged zero-allocation callback suite.
- Actual Debug Standalone visual inspection: PASS for the connected Jarvis tab, shared typed prompt, offline disclosure, describe/guided choice, and empty safe proposal state. Complete interaction evidence and Release screenshots remain part of the final M7 gate.
- `git diff --check`: PASS.

## M7 secure provider-settings checkpoint

- Added a move-only bounded native credential owner plus `MacKeychainCredentialStore` using exact generic-password service/account pairs, a 16 KiB maximum, strict ASCII identifiers, device-only accessibility, and idempotent exact removal.
- Hardened Keychain request construction to fail closed if any Core Foundation object cannot be created. No under-specified lookup, update, or removal query can be issued.
- Added a real temporary-service Keychain test covering absent/store/read/update/read, empty/oversized/malformed identifier rejection, removal, and confirmed absence after cleanup.
- Added one read-only native provider-status operation and a Settings surface that reports offline mode, no selected provider, no configured credential, Keychain availability, and the current no-network privacy boundary. No frontend credential field or secret-bearing bridge operation exists.
- Corrected frontend UUID validation to match the authoritative native opaque 128-bit deterministic-ID contract and mapped stable kebab-case guided-question IDs to typed React answer fields.
- Actual Debug Standalone interaction: PASS for two-at-a-time guided progress, typed-answer persistence, restored proposal display, A/B selection, rejection/restoration, and native provider/privacy status. This is not an audible, Release, provider-network, or FL Studio pass.
- UI tests: PASS, 15/15; strict TypeScript: PASS; production build: PASS. `app.js` 829.27 kB (214.23 kB gzip), `app.css` 22.29 kB (5.72 kB gzip), local index 0.40 kB.
- Debug Standalone/VST3 build: PASS. Complete Debug CTest: PASS, 12/12, including the native Keychain round trip and unchanged real-time allocation coverage.
- The known Vite direct-eval warning remains confined to JUCE's pinned Android compatibility helper.
- `git diff --check`: PASS.

## M7 final verification checkpoint

- Clean `npm ci --ignore-scripts` and production-dependency audit: PASS, 0 vulnerabilities. UI tests/lint/build: PASS, 15/15.
- Complete Debug Standalone/VST3 build and CTest: PASS, 12/12.
- Complete Release Standalone/VST3 build and CTest: PASS, 13/13, including packaged VST3 scan/instantiate/finite-stereo rendering and native Keychain coverage.
- Release Standalone/VST3: thin Mach-O `x86_64`; VST3 local ad-hoc signature verifies deeply/strictly.
- pluginval 1.0.4 strictness 5: `SUCCESS` across editor lifecycle, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed/build hashes match, architecture/signature verify, and the installed bundle independently renders finite stereo MIDI audio.
- Release/installed VST3 SHA-256: `b17c88bab2c1356c7b01980b96f918a28acbdd337f7ee2e437f9c63a7d7119ca`.
- Release Standalone SHA-256: `4523ffa815cfcdd7fb4d666644f75dde82869f6ebf673f9707f31314c8d3b1da`.
- Development-origin, project-source `eval`, sensitive-token pattern, JSON-schema, and `git diff --check` gates: PASS. All 14 project HTTP strings are JSON Schema identifiers.
- Actual Release Standalone interaction: PASS for native offline/provider/Keychain truth, explained proposal creation, A→B selection, rejection restoring A, guided answers persisting and advancing to the next focused pair, and application quit. This is not an audible, physical-device, provider-network, or FL Studio pass.
- Evidence: `evidence/m7/verification.md`, strictness-5 pluginval log, and five actual Release screenshots.
- Every M7 FL Studio case remains `HUMAN RUN REQUIRED` in `docs/FL_STUDIO_TEST_MATRIX.md`.

## Previous M6 checkpoint

## Implemented M6 checkpoint

- Added deterministic version-2 `.folkparkpreset` documents plus pure oldest-supported migration, complete 102-parameter/effect/route capture, bounded parsing, atomic explicit-overwrite saves, content-addressed WAV assets, traversal/symlink rejection, and exact missing-asset relink.
- Added a SQLite repository behind `HistoryRepository` with transactional migrations, bounded search, exact recall, lineage, favorites/tags, soft deletion, retention, cleanup, and database-failure isolation.
- Integrated a non-audio `PersistenceCoordinator`, processor workflows, strict native bridge payloads, and a React preset/history workspace. No filesystem, JSON, or SQLite work is reachable from `processBlock`.
- Added one audio-block-boundary preset publication for prepared oscillator A/B banks and modulation routes. Failed validation/busy publication leaves active state unchanged.
- Added versioned bounded host project state containing a complete native preset payload, imported asset references, dirty status, accepted composition, and history lineage. Restoration does not require an editor.
- Missing project assets leave parameters, wavetables, and composition unchanged while exposing recovery metadata. Wrong hash/size is rejected; exact relink completes the pending sound-and-composition transaction.
- Added atomic-only parameter revision tracking for reliable dirty state without locking the audio callback.
- Corrected Save As so non-overwrite creates a new UUID while updates require explicit overwrite and retain the active UUID.

## M6 commands and exact results

- Clean `npm ci --ignore-scripts` and `npm audit --omit=dev`: PASS; 0 vulnerabilities.
- UI production build: PASS; `app.js` 806.08 kB (209.25 kB gzip), `app.css` 16.30 kB; UI tests 10/10; lint PASS. The known direct-eval warning remains confined to JUCE's pinned Android compatibility helper.
- Complete Debug build: PASS for native tests, Standalone, and VST3.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 10/10.
- Complete Release build: PASS for native tests, Standalone, and VST3.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 11/11, including the packaged VST3 scan/instantiate/finite-stereo MIDI render.
- Release Standalone and VST3 are thin `x86_64` Mach-O artifacts. The VST3 local ad-hoc signature verifies deeply/strictly; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` across editor lifecycle, processing, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed/build hashes match, installed architecture/signature verify, and an independent smoke invocation renders finite stereo MIDI audio.
- Release/installed VST3 SHA-256: `9b0fb548a4844b4384742e02248682fde8ffa479a19b9066c953dabc8c6572dc`.
- Release Standalone SHA-256: `3e4cf0d884ad8a770100e7cc34ac6281959879acaf1495c9d03fefd79b1f810f`.
- Source development-origin, sensitive-token-pattern, JSON-schema, and `git diff --check` gates: PASS; only JSON Schema identifier URLs are present in scanned project source.
- Release Standalone visual gate: PASS for ready native bridge, Synth A/B displays and four-octave keyboard, accepted Compose piano roll, ordered FX workspace, preset/history availability, and clean close. Four screenshots are retained.
- Focused processor recovery covers real user-WAV conversion/retention, retry after busy audio publication, external preset asset localization/independent reload, editor-independent host-state restore, accepted-composition restore, malformed/oversized payload rejection, missing-asset rollback, wrong-hash rejection, exact relink, Save As identity, explicit overwrite, dirty tracking, SQLite symlink rejection, database isolation, and persistence restart.
- `git diff --check`: PASS at the working checkpoint review.

## M6 evidence

- `evidence/m6/verification.md`
- `evidence/m6/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m6/standalone-m6-synth.png`
- `evidence/m6/standalone-m6-compose.png`
- `evidence/m6/standalone-m6-fx.png`
- `evidence/m6/standalone-m6-history.png`
- Every M6 FL Studio case remains HUMAN RUN REQUIRED.

## Previous M5 checkpoint

## Implemented M5 checkpoint

- Added the fixed Distortion → Chorus → tempo-synced Delay → Reverb → Compressor → Parametric EQ chain. Every stage has independent bypass, bounded settings, a wet/dry or equivalent blend, and a 10 ms click-safe transition.
- Appended 29 stable host parameters after the 73 existing IDs. All six effects default to bypass, state round-trips all new values, and older/new default sounds remain gain-safe.
- Added finite-value/default substitution at the DSP boundary, feedback caps, bounded output, deterministic reset, supported-rate/block-size coverage, isolated-stage effectiveness, exact tempo-delay timing, and serial-chain tests.
- Included all six enabled effects in the callback allocation probe; 32 measured blocks still allocate zero times.
- Added accepted-only offline rendering with separate synth/effect/MIDI instances, immutable current A/B banks, current parameter/route/master snapshots, a bounded 12-second tail, and a 15-minute maximum output.
- Streamed stereo 24-bit/48 kHz WAV data to a temporary sibling, reopened and validated header/rate/depth/length before destination replacement, required explicit overwrite authorization, and removed temporary work on cancellation/failure.
- Added a live-isolation regression: an active synth remained bit-identical to an untouched control engine across a complete offline WAV render.
- Replaced the FX placeholder with the complete host-aware effect workspace plus accepted-WAV destination chooser, live status/duration/path, and cancellation.

## M5 commands and exact results

- Clean `npm ci --ignore-scripts`: PASS; audit reports 0 vulnerabilities.
- UI TypeScript and interface/effects contracts: PASS, 8/8.
- Visual analysis benchmark: 10,000 iterations over 16 × 96 samples in 1093.936 ms, or 109.394 microseconds per analysis on this Intel Mac.
- Production Vite bundle: PASS; `app.js` 787.76 kB (205.58 kB gzip), `app.css` 13.51 kB, and a 0.40 kB local index.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 8/8.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 9/9, including external-host load and finite stereo render through the actual built VST3.
- Release Standalone and VST3: thin `x86_64` Mach-O. The VST3 local ad-hoc signature verifies; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor lifecycle, processing, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed and validated build binary SHA-256 both equal `5ff08476376a4d37e0224623741c3e42a5e15cb52118b6259f33705c965126ac` and the installed bundle independently renders finite stereo audio.
- Release Standalone inspection: PASS for bundled M5 header, six ordered FX sections, safe bypass defaults, scroll access to the WAV panel, and clean close.

## M5 evidence

- `evidence/m5/verification.md`
- `evidence/m5/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m5/standalone-m5-synth.png`
- `evidence/m5/standalone-m5-fx.png`
- `evidence/m5/standalone-m5-fx-render.png`
- `tests/EffectsTests.cpp`
- `tests/OfflinePreviewTests.cpp`
- `tests/RealtimeTests.cpp`

## Previous M4 checkpoint

### Implemented M4 checkpoint

- Added pinned React/React DOM/TypeScript/Vite/Three.js dependencies and a reproducible offline production bundle embedded as three JUCE resources. No development server, CDN, remote font, tracker, or runtime network origin is used.
- Replaced the condensed page with the responsive Silicon Dreams Synth/Compose/FX/History/Settings information architecture. M5–M7 views remain explicit placeholders.
- Added real OSC A/B wavetable visualization from fixed native table copies: 30 FPS bounded Three.js frame/morph scene, position marker, derived spectrum, 12 FPS 2D Low Graphics fallback, state-change Reduced Motion rendering, resize handling, and hidden-view suspension.
- Added a four-octave C2–B5 touch/mouse piano and octave-shiftable A–P computer zone. The fixed SPSC preview queue becomes MIDI only inside `processBlock`; pointer cancel, blur, hide, editor close, Panic, overflow, and queue reset release notes safely.
- Made held keys idempotent in both layers. React ignores repeat/already-held sources, and native duplicate note-on/off commands emit nothing. Twelve injected held-`A` keydowns produced exactly one active C3 voice; release returned to zero.
- Added host-aware controls for both oscillator positions/levels, mixer sources, filter, all envelopes, and four LFO shapes/rates. Immediate Undo flushes pending APVTS state before using the shared undo manager, and Redo restores the gesture.
- Added a complete 32-route modulation review workspace with strict bounded native parsing and atomic publication.
- Added bounded composition-note pitch/start/duration/velocity editing. Candidate edits are validated, stable-sorted, and published transactionally; the accepted bundle remains immutable and editing requires a new explicit acceptance.
- Expanded the version 1 complete UI snapshot to include actual A/B table frames, all parameters, routes, composition state, status, architecture, and voices. Malformed/future/non-finite/duplicate/oversize snapshots are rejected before view replacement.

### M4 commands and exact results

- Clean `npm ci --ignore-scripts`: PASS; audit reports 0 vulnerabilities.
- UI TypeScript check and interface contracts: PASS, 7/7.
- Visual analysis benchmark: 10,000 iterations over 16 × 96 samples in 829.857 ms, or 82.986 microseconds per analysis on this Intel Mac.
- Production Vite bundle: PASS; `app.js` 779.27 kB (204.47 kB gzip), `app.css` 12.66 kB, and a 0.40 kB local index.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 6/6.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 7/7, including external-host load and finite stereo render through the actual built VST3.
- Release Standalone and VST3: thin `x86_64` Mach-O. The VST3 local ad-hoc signature verifies; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor lifecycle, processing, state, automation, buses, and 44.1/48/96 kHz × 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed and validated build binary SHA-256 both equal `58169e6dcfda3ee50298e6994ac6b9441cbc5c887d2b5fa0e0bae26932848188`.
- Release visual/interaction inspection: PASS for actual A/B visuals, C2–B5 layout, computer mapping, held-key single voice, release-to-zero, responsive compact scrolling, route publication, candidate generation, and bounded note editing.

### M4 evidence

- `evidence/m4/verification.md`
- `evidence/m4/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m4/standalone-m4-four-octaves.png`
- `evidence/m4/standalone-m4-held-key.png`
- `ui/src/interface-contract.test.ts`
- `tests/MidiDeliveryTests.cpp`
- `tests/RealtimeTests.cpp`
- `tests/PluginTests.cpp`

## Previous M3 checkpoint

## Implemented M3 checkpoint

- Added strict version 1 `MusicIntent` and `GeneratedClip` schemas and C++ models with UUIDs, seed/version metadata, key/scale/tempo/meter/bars, requested parts, genre/emotion, normalized macros, arpeggiator configuration, note/polyphony/event bounds, and parent lineage.
- Added one deterministic shared harmonic plan and bounded chord, melody, bass, and arpeggio generators. Chords include functional weighting, inversions, seventh/tension selection, voice-leading cost, and V-I cadence. Melody includes contour, chord tones, passing tones, rests, motifs, variation, and leap limits. Bass and five arp order modes remain monophonic with complete note lifecycles.
- Added deterministic bounded humanization plus `More Like This` lineage and `Surprise Me` candidate generation. Supported-key/scale, odd-meter, tight-range, maximum-bar/event-cap, density, ordering, overlap, and malformed-fixture properties are tested.
- Added a non-real-time composition session with distinct candidate and accepted state. Acceptance is required before drag, save export, or direct output; later candidates cannot silently replace the accepted bundle.
- Added one shared SMF delivery path with tempo/time-signature metadata, per-part tracks, explicit note-offs, canonical low-PPQ quantization, reopen comparison at PPQ 96/480/960/1920, verified temporary drag files, and save export.
- Added a fixed double-buffered direct-MIDI schedule with atomic next-block activation, correct sample offsets, tracked Stop note-offs, internal-synth playback, and host MIDI output. The measured callback remains allocation-free with a pre-sized host buffer.
- Added the condensed M3 Compose UI with seed/key/scale/BPM/bars, six macros, part selection, colored piano-roll projection, generation/variation/accept controls, export, direct route, Stop, live status, and accepted-only native drag strip.
- Added strict `SoundIntent` and `ParameterProposal` schemas and typed validators for the requested guided sound walkthrough. They require bounded text/values, unique parameter IDs, explanation/confidence, and explicit acceptance; the assistant itself remains M7.

## M3 commands and exact results

- Debug configure/build for Standalone, VST3, native, composition-property, MIDI-delivery, assistant-model, real-time-allocation, and processor/UI tests: PASS with no project compiler warnings.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 6/6.
- Release configure/build for the same targets plus built-bundle host smoke: PASS with no project compiler warnings.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 7/7.
- Release built-VST3 smoke: PASS; the actual VST3 scanned, instantiated, and rendered finite stereo audio from MIDI.
- Audio allocation instrumentation: PASS; 32 measured blocks including synth publication/crossfade and direct MIDI scheduling allocated zero times.
- pluginval 1.0.4 strictness 5: `SUCCESS`; editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples passed.
- Release Standalone visual inspection: PASS for the M3 header, composition entry, explicit accepted-only drag state, and retained screenshot. Complete interactive generation remains a human check.

## M3 evidence

- `evidence/m3/verification.md`
- `evidence/m3/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m3/standalone-m3.png`
- `tests/CompositionTests.cpp`
- `tests/MidiDeliveryTests.cpp`
- `tests/AssistantModelsTests.cpp`
- `tests/RealtimeTests.cpp`
- `tests/PluginTests.cpp`

## Previous M2 checkpoint

## Implemented M2 checkpoint

- Added two immutable wavetable banks. Each supports up to 16 normalized 2048-sample frames and 11 FFT-built band-limited mip levels; the renderer interpolates phase and adjacent frames and selects a mip from oscillator frequency.
- Added independent A/B position, coarse/fine tuning, phase/random/reset, level/pan, and up to eight unison lanes with detune/spread/blend. Audible continuous changes and unison lane activation use fixed 10 ms smoothing/fades.
- Added a sine/triangle sub, deterministic white/pink noise, three per-voice envelopes, and four sine/triangle/saw/square LFOs with free/synced rate, phase, retrigger, and global free-run behavior.
- Replaced the M1 filter with a bounded topology-preserving state-variable low/high/band-pass filter with cutoff, resonance, drive, key tracking, and filter-envelope amount.
- Added a central modulation registry with ten sources, thirteen destinations, three curves, validated normalized amounts, fixed 32-route snapshots, and atomic block-boundary publication.
- Added strict background WAV conversion with channel averaging, sample/channel/cycle bounds, non-finite and silence rejection, DC removal, deterministic resampling, endpoint continuity, normalization, SHA-256 metadata, and a fixed preview.
- Added an explicit import review state: file selection queues conversion away from audio; Confirm is required before immutable publication; Cancel and failure leave the active table unchanged; the audio path crossfades for 128 samples.
- Added host parameters append-only after the M0/M1 IDs, route state serialization with transactional validation, safe import processor APIs, host-tempo reading, and a condensed M2 WebView editor.
- Added spectral rejection, dual-oscillator/stereo, multimode extremes, deterministic noise, every LFO source/shape, three-envelope release, aggressive automation, CPU, imported-WAV, state, and allocation coverage.

## M2 commands and exact results

- Debug configure/build for Standalone, VST3, native, real-time-allocation, and processor tests: PASS with no project compiler warnings.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 3/3.
- Release configure/build for Standalone, VST3, native, real-time-allocation, processor, and built-bundle host smoke tests: PASS.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 4/4.
- Release built-VST3 smoke: PASS; the actual VST3 scanned, instantiated, exposed zero-input/stereo-output buses, and rendered finite stereo audio from MIDI.
- Audio allocation instrumentation: PASS; 32 measured blocks, including atomic wavetable/matrix activation and the 128-sample crossfade, allocated zero times.
- Spectral baseline at 48 kHz/8 kHz: selected mip 9 with maximum harmonic 2; rejected-to-retained energy ratio `1.73254e-14`.
- Release CPU baseline on this Mac: 16 voices × 2 oscillators × 8 unison, 40 × 512-sample blocks at 48 kHz; 327.983 ms to render 426.667 ms audio, ratio `0.768709×` realtime.
- Release Standalone and VST3: thin `x86_64` Mach-O; both local ad-hoc signatures verify deeply and strictly.
- `./scripts/install_user_vst3.sh release`: PASS; installed user VST3 hash matches the validated build, is thin `x86_64`, and its local ad-hoc signature verifies.
- pluginval strictness 5: `SUCCESS`; editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples passed.
- Release Standalone visual inspection: PASS; M2 controls, reviewed import, route editor, and current-versus-coming AI language are visible in retained evidence.

## Evidence

- `evidence/m2/verification.md`
- `evidence/m2/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m2/standalone-m2.png`
- `tests/M0Tests.cpp`
- `tests/RealtimeTests.cpp`
- `tests/PluginTests.cpp`
- `tests/Vst3SmokeTests.cpp`

## Post-M8 Orbital Habitat visual checkpoint

- Established one original retro-CGI design language across Synth, Compose, Jarvis, FX, History, Settings, navigation, status, and overlays. No third-party interface asset, external runtime image/font, or network URL was added.
- Added shared visual tokens for palette, gradients, physical materials, specular highlights, depth, glows, radii, spacing, typography, and motion, including low-graphics and reduced-motion fallbacks.
- Added reusable Button, IconButton, Panel, Sidebar, Navbar, Tabs, Slider, Knob, Toggle, Dropdown, Modal, Tooltip, TextInput, NumericInput, TextArea, ProgressBar, Meter, ContextMenu, Notification, and StatusIndicator components.
- Refactored host-aware sliders, knobs, selects, and toggles without changing JUCE parameter gesture ownership. Compose and Jarvis now reuse the same physical controls while retaining the deferred-event repair.
- UI contracts/lint/build: PASS, 19/19. Production bundle: local 0.40 kB index, 54.93 kB CSS (13.62 kB gzip), and 835.66 kB JavaScript (215.78 kB gzip).
- Complete Release CTest: PASS, 16/16. pluginval 1.0.4 strictness 5: `SUCCESS` across editor, processing, state, automation, buses, and its supported sample-rate/block matrix.
- Installed/build VST3 exact executable hash parity and independent finite-audio MIDI render: PASS. VST3 SHA-256: `8b93eda7c06c28849fe825d99062fff31363bc2cba1c53725d1f2a3151354d71`; Standalone SHA-256: `c34ff559eb46eeb347a6067d694473392f6f712529c0e20de4334e85f85175c9`.
- The previous user VST3 remains recoverable at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T143548Z`. No project, preset, sample, history, export, or FL Studio data was touched.
- Actual Release Standalone screenshots and visual/navigation inspection are retained for Synth, Compose, Jarvis, FX, and History under `evidence/m8/visual/`. This is not an audible or FL Studio pass.
- Established `plans/RHYTHM_LAB.md` as the separate future drum-product plan. It requires audible hybrid drum voices, deterministic patterns, per-lane control, stems, and a licensed/original/user-import break slicer; implementation has not started.

## Rhythm Lab R1 contract checkpoint

- Recorded the producer direction: indie/rock, Eurodance, techno, funk, and jazz; synthesized-first because no reviewed sample library is available.
- Added ADR-0010 and strict version-1 native/JSON contracts for `RhythmIntent`, `DrumPattern`, and the authored `synth_core_v1` kit while preserving every M3 composition contract.
- Added stable kick, snare, closed/open hat, and percussion lane identities plus bounded synthesis parameters ready for an allocation-free engine.
- Focused Debug configure/build and contract test: PASS, 1/1. Ten JSON schemas parse successfully and `git diff --check` passes.
- This checkpoint contains no audible drum engine, generated pattern, sample asset, application UI, or FL Studio evidence yet.

## Rhythm Lab synthesized engine checkpoint

- Added a deterministic preallocated synthesized drum engine for kick, snare, closed/open hat, and percussion with no external sample dependency.
- Added pitch/envelope shaping, deterministic noise, hat choking, bounded voice stealing/tails, soft drive, stereo placement, finite containment, and immediate reset.
- Added tests across 44.1/48/96 kHz and 32–1024-sample blocks plus deterministic parity, finite/nonzero output, tail completion, reset silence, and invalid-kit isolation.
- Extended the measured callback-allocation probe to render active synthesized drums; zero allocations remain measured across its complete 32-block workload.
- Focused Debug engine and real-time tests: PASS, 2/2. Product pattern playback, UI, FL Studio behavior, and listening remain pending.

## Rhythm Lab generation and acceptance checkpoint

- Added deterministic bounded patterns for indie/rock, Eurodance, techno, funk, and jazz through data-driven probability, accent, ghost-note, swing, humanization, and fill behavior.
- Added separate candidate/accepted rhythm session state plus explicit acceptance and lineage-preserving `More Like This` generation.
- Added standards-compliant channel-10 drum MIDI serialization and reopen parity at 96, 480, 960, and 1920 PPQ.
- Verified that all five genre profiles produce distinct stable timing fingerprints and long dense patterns respect their event cap.
- Focused contract, engine, generator/MIDI, and real-time suites: PASS, 4/4 without project compiler warnings. Product UI/playback and FL Studio remain pending.
- Complete Debug Standalone/VST3 build and CTest: PASS, 18/18 in 21.57 seconds after the contract, engine, and generation checkpoints.

## Persistent audition keyboard checkpoint

- Moved the single preview-note owner outside the workspace switch so the C2–B5 piano and A–P computer-key zone remain mounted across Synth, Compose, Jarvis, FX, History, and Settings.
- Preserved normal typing in input, select, textarea, and contenteditable targets while allowing audition after navigation or another non-text button receives focus.
- Consumed mapped keydown and keyup events, ignored macOS key repeat, and blurred pointer-played piano buttons after release to prevent focused-control key leakage and duplicate UI sounds.
- UI tests/lint/build: PASS, 19/19. Final Debug CTest: PASS, 18/18. Release CTest: PASS, 19/19, including packaged-VST3 finite-stereo MIDI rendering. pluginval 1.0.4 strictness 5: `SUCCESS`.
- Real Debug Standalone automation on the FX workspace held `A` as C3 with `1 voices`, then released to `0 voices` without a stuck note. Audible absence of the reported interface sound and FL Studio behavior remain human listening checks.
- The verified thin `x86_64` Release VST3 was installed with executable SHA-256 `e5aaf6757ee54dd5d92a7639ec424d33374d4f88f25b9e11a269ed409bece5dd`; installed/build parity and independent installed-bundle finite-stereo MIDI rendering pass. The previous bundle remains recoverable at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T152511Z`.

## M9 Audio Stability contract checkpoint

- Recorded the producer-reported intermittent saturation/crack/complete distortion during generated-composition audition as a release-blocking defect.
- Established `plans/M9_AUDIO_STABILITY.md` with separate classifications for level overload, DSP instability, voice pressure, audio deadline misses, and MIDI delivery faults.
- Confirmed the existing architecture counts non-finite output and MIDI queue overflows but has no explicit output-ceiling, over-unity, peak, voice-pressure, or deadline-headroom proof. The effect chain's `[-32, 32]` finite bound is not a 0 dBFS guarantee.
- Ordered the work as deterministic reproduction/telemetry, source gain and feedback correction, profiled CPU optimization, bounded product diagnostics, then Release and producer listening gates. No limiter, global feature disable, or blind quality reduction is authorized as a substitute for diagnosis.

## M9 audio-pressure telemetry checkpoint

- Added cumulative lock-free peak, over-unity, maximum-active-voice, and voice-steal telemetry to the existing bounded diagnostics path. The callback performs only local arithmetic and relaxed atomic publication; no clock, string, log, allocation, filesystem access, or new lock was added.
- Normal generated-composition fixture, seed 7007, defaults, 48 kHz/512: maximum output `0.428440`, zero over-unity samples, zero non-finite samples, 16 maximum voices, and 13 voice steals.
- The same normal fixture in the optimized Release processor test rendered at `0.571528x` real time on this Intel Core i9. This is a local baseline with roughly 43% sequential render headroom, not proof against host/device deadline misses.
- Deliberate supported extreme-gain fixture: pre-master peak `8.460820`, output peak `16.881600`, 17,456 over-unity samples, and zero non-finite samples. This reliably classifies a finite level overload and proves the current output is not ceiling-safe.
- Focused diagnostics/plugin/allocation suites: PASS, 3/3. Complete Debug Standalone/VST3 build and CTest: PASS, 18/18. Focused Release processor test: PASS. A new Release artifact was not installed at this telemetry-only checkpoint; the verified keyboard build remains installed.

## M9 render-headroom checkpoint

- Reproduced an actual Release deadline failure before optimization: every normal-composition 96 kHz case measured `1.14778x`–`1.16321x` real time while remaining finite and below unity. The 48 kHz cases used roughly 58% of their sequential audio budget.
- Removed wasted oscillator work for exactly silent unison lanes and cached stable per-lane frequency/mip calculations with exact pitch invalidation. No product feature, oscillator quality setting, parameter ID, or state meaning changed.
- Post-repair default matrix: 44.1 kHz `0.119538x`–`0.135525x`, 48 kHz `0.126384x`–`0.135008x`, and 96 kHz `0.252856x`–`0.262954x` across 32–1024 samples. The normal 48 kHz/512 four-part case fell from `0.566012x` to `0.127443x` with the same exact `0.428440` peak.
- Heavy 96 kHz/64, two oscillators × eight unison plus driven filter and all effects: `0.669342x`, peak `0.262349`, zero over-unity/non-finite samples. The gate requires at least 15% sequential render headroom and observed roughly 33%.
- Complete Debug CTest: PASS, 18/18. Complete Release CTest: PASS, 19/19. pluginval strictness 5: `SUCCESS`. Installed/build parity and independent finite-stereo MIDI rendering: PASS.
- Installed optimized VST3 SHA-256: `5377f6dcb0af792cacf0733415b972469e09978b8faa1f2b786b733517b89250`; rollback bundle retained at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T154551Z`. The Release Standalone was restarted and the current default built-in output is 48 kHz.
- Automated evidence is retained in `evidence/m9/verification.md`. Audible Standalone and FL Studio confirmation remain human-required; the separate extreme-gain output-ceiling defect remains open.

## Human-required status

- Standalone physical keyboard/audio-device playability: PARTIAL. Automated real-window C3 hold/repeat/release passed; listening through the user's selected audio device remains human.
- FL Studio discovery/insertion, dual-oscillator playability, every exposed control, automation write/read, save-close-reopen state, panic/no-stuck-note, editor reopen/resize/scroll, and MIDI routing/drag: HUMAN RUN REQUIRED.
- M4 four-octave pointer play, A–P focus ownership, macOS hold/repeat behavior, Oct−/Oct+ mapping, compact horizontal keyboard scroll, low-graphics/reduced-motion behavior, and editor close while holding notes inside FL Studio: HUMAN RUN REQUIRED.
- M4 actual A/B visual response to host automation/import, full matrix review/apply/discard, immediate Undo/Redo, candidate note editing, and accepted-versus-candidate delivery inside FL Studio: HUMAN RUN REQUIRED.
- M5 audible ordered effects, independent bypass/click behavior, host-tempo delay changes, automation write/read, save-close-reopen, and CPU use inside FL Studio: HUMAN RUN REQUIRED.
- M5 accepted WAV chooser/render/cancel/import/playback, expected length/tail, current-sound parity, and live-voice isolation inside the FL wrapper: HUMAN RUN REQUIRED.
- M6 native preset Save As/explicit overwrite/load, content-addressed imported-table project reopen, wrong-file rejection/exact relink, accepted composition restore, history search/compare/recall/trash/retention, and database-unavailable behavior inside FL Studio: HUMAN RUN REQUIRED.
- WAV import through the real macOS chooser, preview/confirm/cancel, table audition, FL project save/reopen limitation, and failure recovery: HUMAN RUN REQUIRED.
- M3 candidate generation/preview/accept interaction in the physical Standalone: HUMAN RUN REQUIRED.
- M3 `.mid` drag into the FL Studio piano roll/channel workflow and musical parity after import: HUMAN RUN REQUIRED.
- M3 Wrapper output-port routing into a second FL instrument, timing/note-off/Stop/panic behavior, and save-close-reopen session limitation: HUMAN RUN REQUIRED.
- M1's outstanding FL Studio human matrix remains outstanding; M2 automation does not turn it into a pass.

## Risks and limitations

- Imported wavetable sources are retained in bounded content-addressed local storage and referenced by versioned project state. Automated missing-asset recovery is covered, but FL Studio save-close-reopen, chooser recovery, and audible parity remain human checks.
- The M4 interface edits all 32 route slots, but host automation of route structure is not supported; route changes are reviewed native transactions stored in plug-in state.
- Three.js is a presentation dependency only. The measured local analysis cost and frame caps do not replace FL Studio CPU/GPU profiling on the target machine.
- User-drawn LFOs and oversampling are optional M2 items and were deliberately deferred. The M7 offline assistant, producer-facing workspace, secure optional-provider boundary, and final Release evidence are verified; FL Studio checks remain human-required.
- The CPU number is a reproducible local baseline, not a guarantee for every host/audio-device configuration. FL Studio profiling is still required.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- Accepted compositions and their delivery state now round-trip in the bounded M6 project payload and remain searchable in local history. Actual FL Studio project reopen is not yet human-verified.
- Direct MIDI begins at the next audio block using clip tempo. Host transport synchronization, reposition, and loop semantics are not claimed in M3.
- M5 preview is an exported, validated WAV rather than an internal transport. Audition/import behavior in FL Studio remains a human workflow check.
- The Vite build reports direct `eval` in JUCE's pinned `check_native_interop.js` Android compatibility helper. The macOS bundle loads only embedded local resources; no project source or runtime URL uses a remote origin.
- JUCE distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain unresolved; builds are private local engineering artifacts only.

## Next smallest verifiable task

Capture the real Release diagnostics panel after Hermann deliberately clicks Preview, then run the safe first FL Studio session in `docs/FL_STUDIO_TEST_MATRIX.md` using a new disposable project. Record only Hermann's observed results; keep every unexecuted case `HUMAN RUN REQUIRED`. Do not begin rollback/uninstall, asset/database failure, signing, licensing, provider, or distribution work without its separate explicit owner boundary.
