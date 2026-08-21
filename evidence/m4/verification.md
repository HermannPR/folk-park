# M4 verification

Date: 2026-08-21 (America/Monterrey)

## Verified results

- Clean pinned UI install, TypeScript check, and production build: PASS. npm reported 0 vulnerabilities.
- UI interface/snapshot/animation/keyboard contracts: PASS, 7/7.
- Visual analysis benchmark: 10,000 iterations over 16 × 96 samples in 829.857 ms (`82.986 µs` per analysis) on this Intel Mac.
- Debug CTest: PASS, 6/6.
- Release CTest: PASS, 7/7, including scanning, instantiating, and rendering finite stereo audio through the actual built VST3.
- Release Standalone and VST3 binaries are thin `x86_64` Mach-O artifacts.
- The Release VST3 local ad-hoc signature passes `codesign --verify --deep --strict`. The private Release Standalone engineering artifact is unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples.
- The installed user VST3 is thin `x86_64`, its local signature verifies, and its binary hash exactly matches the validated Release build.
- Source/runtime-origin scan: no remote HTTP, WebSocket, localhost, or loopback origin in project/native/UI source, embedded index, or embedded stylesheet. The UI uses JUCE's local resource-provider root.
- All strict JSON schemas parse successfully; `git diff --check` passes.

## M4 behavior covered

- Complete version 1 native snapshot recovery for all parameters, real fixed A/B table copies, routes, composition state, version/status, and active voices.
- Rejection of future schemas, non-finite values, duplicate parameters, oversize visual arrays, and malformed bridge calls without partial view/state replacement.
- Actual wavetable-frame/morph and spectrum presentation with bounded 30 FPS Three.js, 12 FPS 2D Low Graphics, Reduced Motion, resize, and hidden-view suspension policies.
- Four visible touch/mouse octaves C2–B5 plus the octave-shiftable A–P computer zone.
- Fixed native preview-MIDI queue, audio-block consumption, zero-allocation measurement, pointer/focus/hide/editor-close/Panic release, overflow fallback, and duplicate note-on/off suppression.
- Real Release-window held-key test: twelve injected `A` keydowns with no keyup kept exactly one highlighted C3 and one active voice; keyup returned the UI to zero voices and no highlighted key.
- Shared host-aware Undo/Redo, including synchronization of a just-finished APVTS gesture before immediate Undo.
- Full bounded 32-route review/apply/discard state and transactional native publication.
- Bounded candidate-note pitch/start/duration/velocity editing, stable ordering, immutable prior acceptance, and mandatory re-acceptance before delivery.
- Audio continuity with the WebView editor open, destroyed while a note is active, and closed; preview release is emitted on close.

## Evidence hashes

- Release Standalone binary SHA-256: `63f82b852c9707f7bb85a45e57386aba5d19a08610ceb5835adcfd7797b7514c`.
- Release/installed VST3 binary SHA-256: `58169e6dcfda3ee50298e6994ac6b9441cbc5c887d2b5fa0e0bae26932848188`.
- pluginval log SHA-256: `e5477105f285ba87ad3e436935571b9bc3c1dad96f64fc6bca781080023c8624`.
- Four-octave Release screenshot SHA-256: `a7ae9e0b3d6ab3617695e871ad1aba0de0fbda2cfd506167d4b0d09d354b6a62`.
- Held-key Release screenshot SHA-256: `b22f580608dbc75b21b3b921a746f71c1c146c77afcd7010668c66272bf2b944`.

## Retained artifacts

- `pluginval/pluginval-release-strictness-5.txt`
- `standalone-m4-four-octaves.png`
- `standalone-m4-held-key.png`

## Explicit gaps

- FL Studio discovery/insertion, keyboard-focus ownership, four-octave pointer play, macOS repeat behavior inside its wrapper, resize/scroll, host automation, state reopen, MIDI drag, and direct routing remain `HUMAN RUN REQUIRED`.
- The optional separate Steinberg validator executable is not installed; pluginval records that subtest as skipped.
- The Vite build warning comes from direct `eval` in pinned JUCE `check_native_interop.js`, an Android interoperability helper bundled upstream. Project code does not call `eval`, and the macOS UI uses only embedded local resources.
- Effects and isolated WAV preview are M5. Preset/history and imported-table persistence are M6. Guided Jarvis/provider integration is M7.
- Distribution signing/notarization, JUCE distribution licensing, final identity, privacy/legal review, and asset-rights gates remain open. These are private engineering artifacts, not a distributable release.
