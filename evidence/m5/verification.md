# M5 verification

Date: 2026-08-21 (America/Monterrey)

## Verified results

- Clean pinned UI install and audit: PASS; npm reported 0 vulnerabilities.
- UI interface/snapshot/effects contracts and strict TypeScript: PASS, 8/8.
- Visual analysis benchmark: 10,000 iterations over 16 × 96 samples in 1093.936 ms (`109.394 µs` per analysis) on this Intel Mac.
- Production Vite bundle: PASS; `app.js` 787.76 kB (205.58 kB gzip), `app.css` 13.51 kB, and a 0.40 kB local index.
- Debug CTest: PASS, 8/8.
- Release CTest: PASS, 9/9, including scanning, instantiating, and rendering finite stereo audio through the actual built VST3.
- Release Standalone and VST3 binaries are thin `x86_64` Mach-O artifacts.
- The Release VST3 local ad-hoc signature passes `codesign --verify --deep --strict`; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed and validated build binary hashes match exactly, the installed bundle is thin `x86_64`, its signature verifies, and it independently instantiates/renders finite stereo MIDI audio.
- Source/runtime-origin scan: no remote HTTP, WebSocket, localhost, or loopback origin in project native/UI source, embedded index, or embedded stylesheet.
- All strict JSON schemas parse successfully; `git diff --check` passes.

## M5 behavior covered

- Fixed Distortion → Chorus → tempo-synced Delay → Reverb → Compressor → Parametric EQ order with independent bypass and 29 append-only host parameters.
- Bit-transparent safe defaults, audible isolated stages, bounded serial output, supported sample rates/block sizes, deterministic reset, malformed/non-finite containment, and 10 ms bypass continuity.
- Exact 1/16 delay arrival at 3,000 samples for 240 BPM and 6,000 samples for 120 BPM at 48 kHz.
- Zero measured callback allocations across 32 blocks with all six effects enabled alongside synth swaps, direct MIDI, and preview keyboard work.
- Separate offline synth/effect instances using immutable parameter, route, master, and current A/B wavetable snapshots.
- Accepted-only transactional stereo 24-bit/48 kHz WAV output, deterministic musical length plus 12-second tail, readable/non-silent samples, explicit overwrite authorization, cancellation cleanup, and a 15-minute bound.
- Offline/live isolation proof: rendering a WAV between two blocks left an active live synth bit-identical to its untouched control engine and preserved its voice count.
- Complete responsive FX workspace with host-aware toggles/sliders/choice, render status/destination/duration, explicit accepted-WAV action, cancellation, and truthful M6/M7 placeholders.
- Real Release Standalone inspection: Synth and FX views loaded, all six ordered sections and the lower WAV panel were reachable, safe defaults were visible, and the app closed cleanly.

## Evidence hashes

- Release/installed VST3 binary SHA-256: `5ff08476376a4d37e0224623741c3e42a5e15cb52118b6259f33705c965126ac`.
- Release Standalone binary SHA-256: `c151eb0486992e6dd80f98dde2644b0281f44858481e3fa127a086a2fbd213e1`.
- pluginval log SHA-256: `9fd1416e70c04a446b7d53be9afc028b8b33667406ea7e7c890071aa48db2566`.
- Synth screenshot SHA-256: `673016cc371070e1ea55ff2cda380630f6f21319fff0db2a39d56adde5be3cf5`.
- FX screenshot SHA-256: `db26426311bac3dc5a1c447cbb10a827f7e24502d633c569e946e13df37e096f`.
- FX render-panel screenshot SHA-256: `dc2357cb56cb422bf2017fd7b8d33da72fc3d4b341b548c970fcff11e596108a`.

## Retained artifacts

- `pluginval/pluginval-release-strictness-5.txt`
- `standalone-m5-synth.png`
- `standalone-m5-fx.png`
- `standalone-m5-fx-render.png`
- `tests/EffectsTests.cpp`
- `tests/OfflinePreviewTests.cpp`
- `tests/RealtimeTests.cpp`

## Explicit gaps

- FL Studio discovery/insertion, audible effect quality/order, host-tempo changes, effect automation/state, WAV chooser/render/import/playback, current-sound parity, and cancellation inside its wrapper remain `HUMAN RUN REQUIRED`.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- The Vite build warning comes from direct `eval` in pinned JUCE `check_native_interop.js`, an Android interoperability helper bundled upstream. Project code does not call `eval`, and the macOS UI uses only embedded local resources.
- Accepted compositions and imported wavetables remain session memory until M6; a reopened project cannot recreate an earlier accepted audio preview or imported table yet.
- Distribution signing/notarization, JUCE distribution licensing, final identity, privacy/legal review, and asset-rights gates remain open. These are private engineering artifacts, not a distributable release.
