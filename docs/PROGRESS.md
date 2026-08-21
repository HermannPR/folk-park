# Progress

## Current checkpoint

- Milestone: M1 — Playable vertical slice
- Status: automated engine, processor, built-VST3, architecture, and validator gates passed; Standalone hardware-MIDI and FL Studio human runs remain required
- Date: 2026-08-20 (America/Monterrey)
- Branch: `feat/m1-playable-synth`

## Implemented M1 checkpoint

- Added a fixed-capacity 16-voice engine with project-generated sine/triangle wavetables, a sine sub, linear amp ADSR, one-pole low-pass filter, sustain pedal, two-semitone pitch bend, all-notes-off, and panic.
- Implemented deterministic stealing: inactive voice first, then the quietest released voice with oldest tie-break, then the oldest active voice.
- Kept the audio callback free of locks, allocations, filesystem, UI, database, and network work; parameters are read atomically and panic crosses threads through an atomic request.
- Added stable host IDs for waveform, oscillator/sub levels, filter cutoff, ADSR, and the existing master gain. State remains versioned and round-trips through APVTS.
- Added host-aware WebView attachments for waveform, master, cutoff, attack, and release, plus native panic and active-voice status. The native fallback remains available.
- Added native engine invariants, processor state/UI-independence tests, and a Release-only external host smoke test that scans and instantiates the built VST3 before sending it MIDI.
- Corrected the plug-in tail report to the maximum 10-second amp release so hosts do not truncate released notes.
- Installed the verified Release VST3 to `~/Library/Audio/Plug-Ins/VST3/folk park.vst3` without opening or changing an FL Studio project.
- Recorded the product-owner guided AI sound walkthrough in `docs/PRODUCT_AMENDMENTS.md` and separated implemented versus roadmap behavior in the README.

## Commands and exact results

- Debug configure/build for Standalone, VST3, native tests, and processor tests: PASS.
- `ctest --preset macos-x86_64-debug --output-on-failure`: PASS, 2/2.
- Release configure/build for Standalone, VST3, native tests, processor tests, and built-bundle host smoke test: PASS.
- `ctest --preset macos-x86_64-release --output-on-failure`: PASS, 3/3.
- Release built-VST3 smoke: PASS; one VST3 type scanned, instantiated, zero-input/stereo-output bus verified, 10-second tail verified, and MIDI note-on rendered finite centered stereo audio.
- `file` and `lipo -archs` on Release Standalone and VST3: PASS; both are thin `x86_64` Mach-O binaries.
- `codesign --verify --deep --strict` on the built and installed VST3: PASS with the local ad-hoc signature.
- pluginval 1.0.4 at strictness 5: SUCCESS; editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples passed. Its optional separate Steinberg-validator subtest was skipped because no validator path is installed.
- Release Standalone visual inspection: PASS; final M1 controls and both native bridge directions are visible in the retained screenshot.
- `./scripts/install_user_vst3.sh release`: PASS; installed bundle is thin `x86_64` and its ad-hoc signature verifies.

## Evidence

- `evidence/m1/verification.md`
- `evidence/m1/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m1/standalone-playable-m1.png`
- `tests/M0Tests.cpp`
- `tests/PluginTests.cpp`
- `tests/Vst3SmokeTests.cpp`

## Human-required status

- Standalone physical keyboard/audio-device playability: NOT RUN.
- FL Studio discovery/insertion, MIDI playability, automation write/read, save-close-reopen state, panic/no-stuck-note, editor reopen/resize, MIDI routing, and drag/drop: HUMAN RUN REQUIRED.
- These cases are not represented as passed by pluginval or the JUCE host smoke test.

## Risks and limitations

- JUCE distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain unresolved; the installed VST3 is for private local engineering only.
- Continuous oscillator level, sub level, and cutoff changes are not smoothed in M1 and may click under aggressive automation; this is visible in the parameter catalog and must be addressed before release hardening.
- The triangle baseline is intentionally not band-limited; wavetable interpolation/band-limiting and CPU/spectral baselines belong to M2.
- The Release external-host smoke is not enabled for Debug because loading a Debug JUCE plug-in into a Debug executable containing the same static JUCE symbols caused allocator symbol interposition. Debug engine/processor suites and the shipping Release bundle are independently covered.
- The Release Standalone is locally runnable but not distribution-signed or notarized.
- Full Xcode is absent; Command Line Tools are sufficient for the verified local builds.

## Next smallest verifiable task

Run the explicit Standalone and FL Studio M1 human matrix with the installed VST3 and record each result. Then start M2 with an ADR and tests for band-limited dual-wavetable rendering and click-safe immutable table swaps.
