# M6 verification

Date: 2026-08-23 (America/Monterrey)

## Verified results

- Clean pinned UI install and audit: PASS; npm reported 0 vulnerabilities.
- UI protocol/interface/persistence contracts and strict TypeScript: PASS, 10/10.
- Production Vite bundle: PASS; `app.js` 806.08 kB (209.25 kB gzip), `app.css` 16.30 kB, and a 0.40 kB local index.
- Complete Debug build: PASS for native tests, Standalone, and VST3.
- Debug CTest: PASS, 10/10.
- Complete Release build: PASS for native tests, Standalone, and VST3.
- Release CTest: PASS, 11/11, including scanning, instantiating, and rendering finite stereo MIDI audio through the actual packaged VST3.
- Release Standalone and VST3 binaries are thin `x86_64` Mach-O artifacts.
- The Release VST3 local ad-hoc signature passes `codesign --verify --deep --strict`; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples.
- Installed user VST3: PASS; the installed and validated build binary hashes match exactly, the installed bundle is thin `x86_64`, its local ad-hoc signature verifies, and it independently instantiates/renders finite stereo MIDI audio.
- Source development-origin scan: project native/UI source and embedded index/stylesheet contain no localhost, loopback, WebSocket, or runtime HTTP origin. The only HTTP strings in the scanned project source are JSON Schema identifiers.
- Sensitive-token pattern scan: no matching private keys or common provider/GitHub access-token forms in tracked project material outside generated/dependency/build directories.
- Every strict JSON schema parses successfully; `git diff --check` passes.

## M6 behavior covered

- Deterministic schema-v2 `.folkparkpreset` encoding, oldest-supported v1 migration, bounded duplicate-safe parsing, explicit overwrite, and collision-safe Save As with distinct UUIDs.
- Complete capture/restore of all 102 normalized parameters, modulation routes, ordered effects, metadata, imported oscillator assets, and dirty/current-preset identity.
- Content-addressed imported-WAV localization, SHA-256/size validation, external-source independence, traversal/symlink rejection, and exact missing-asset relink.
- Bounded versioned host project state restoring the full native sound, accepted composition, imported assets, dirty status, and history lineage without an editor.
- Malformed/oversized state and missing/wrong assets leave parameters, current wavetables, and composition unchanged until a complete recovery transaction succeeds.
- Searchable SQLite history with migrations, exact recall, lineage, favorites/tags, comparison, recoverable soft deletion, retention, cleanup, and restart persistence.
- SQLite unavailability and database-path symlink rejection remain isolated from presets, composition acceptance, project state, and finite audio.
- Confirmed import retry after a deliberately busy exchange and rejection of a competing complete preset publication without partial audio-state mutation.
- Audio callback allocation instrumentation remains at zero across synth publication, direct MIDI, preview keyboard work, and the enabled effect chain.
- Real Release Standalone inspection: native bridge reached ready state; actual A/B wave views, four-octave keyboard, accepted Compose piano roll, ordered FX workspace, and the M6 preset/history workspace rendered coherently. The app closed cleanly.

## Evidence hashes

- Release/installed VST3 binary SHA-256: `9b0fb548a4844b4384742e02248682fde8ffa479a19b9066c953dabc8c6572dc`.
- Release Standalone binary SHA-256: `3e4cf0d884ad8a770100e7cc34ac6281959879acaf1495c9d03fefd79b1f810f`.
- pluginval log SHA-256: `353b859997bebf9fa5e6af9e039b1bd4e326eedfe77ccd2ba939b79e56daec92`.
- Official pluginval 1.0.4 macOS archive SHA-256 used for this gate: `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`.
- Synth screenshot SHA-256: `c9d4acc0bf021212960a25f99efecd09ae6463b6b8d4bd9898dda5039fbd8ca8`.
- Compose screenshot SHA-256: `9154a0d185d8cffe277755f7a7e32fbd2ebd4936d48e24f8a9b770c0d14ac476`.
- FX screenshot SHA-256: `86afdd725c5532b04340036615a0b3b9c20d007fa8b967e69351fe6f4742b0eb`.
- History screenshot SHA-256: `bd6e1a8462b121dc8ce1678b7dbd9a1e978968eaea03663c6309d48d4ab65cea`.

## Retained artifacts

- `pluginval/pluginval-release-strictness-5.txt`
- `standalone-m6-synth.png`
- `standalone-m6-compose.png`
- `standalone-m6-fx.png`
- `standalone-m6-history.png`
- `tests/PresetTests.cpp`
- `tests/HistoryTests.cpp`
- `tests/PluginTests.cpp`
- `tests/RealtimeTests.cpp`

## Explicit gaps and observations

- Every FL Studio discovery, insertion, listening, input/focus, automation, project/preset reopen, missing-asset chooser/relink, history, effects, MIDI drag/routing, and WAV import/playback case remains `HUMAN RUN REQUIRED`.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- The Vite build warning comes from direct `eval` in pinned JUCE `check_native_interop.js`, an Android interoperability helper bundled upstream. Folk Park source does not call `eval`, and the macOS UI uses embedded local resources.
- The linked JUCE binary includes its generic loopback-address string, but the project source/runtime-resource scan found no configured development server or remote runtime origin.
- A first Release configure/build attempt was started concurrently with the clean Vite build. Vite temporarily removed the embedded UI output directory, so Ninja correctly reported a missing generated resource. After the UI build finished, a serial CMake reconfigure/build and the complete Release gate passed. This was a gate-orchestration race, not a source/test failure.
- Standalone visual inspection proves presentation/bridge readiness, not audible quality or physical-device behavior.
- Distribution signing/notarization, JUCE distribution licensing, final identity, privacy/legal review, and asset-rights gates remain open. These are private engineering artifacts, not a distributable release.
