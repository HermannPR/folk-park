# Progress

## Current checkpoint

- Milestone: M3 — Deterministic composition and MIDI delivery
- Status: automated composition, schema, export parity, direct scheduling, real-time, processor, built-VST3, architecture, visual, and validator gates passed; physical Standalone interaction and FL Studio human drag/routing runs remain required
- Date: 2026-08-20 (America/Monterrey)
- Branch: `feat/m3-composition-midi`, stacked on the still-unmerged M2 branch

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

## Human-required status

- Standalone physical keyboard/audio-device playability: NOT RUN.
- FL Studio discovery/insertion, dual-oscillator playability, every exposed control, automation write/read, save-close-reopen state, panic/no-stuck-note, editor reopen/resize/scroll, and MIDI routing/drag: HUMAN RUN REQUIRED.
- WAV import through the real macOS chooser, preview/confirm/cancel, table audition, FL project save/reopen limitation, and failure recovery: HUMAN RUN REQUIRED.
- M3 candidate generation/preview/accept interaction in the physical Standalone: HUMAN RUN REQUIRED.
- M3 `.mid` drag into the FL Studio piano roll/channel workflow and musical parity after import: HUMAN RUN REQUIRED.
- M3 Wrapper output-port routing into a second FL instrument, timing/note-off/Stop/panic behavior, and save-close-reopen session limitation: HUMAN RUN REQUIRED.
- M1's outstanding FL Studio human matrix remains outstanding; M2 automation does not turn it into a pass.

## Risks and limitations

- A confirmed imported wavetable lives in bounded session memory only. Source audio or converted table persistence is intentionally deferred to M6; reopening a project currently restores parameters/routes but uses the built-in table. The UI states the review boundary but cannot yet warn on project reopen.
- The engine and serialized state support 32 routes, but the condensed M2 engineering UI edits one route at a time and its Apply action replaces the matrix with that reviewed route. Full 32-slot editing belongs to the M4 interface.
- The condensed UI exposes the primary M2 controls; the remaining stable parameters are available to a host and state but await the full M4 interface.
- User-drawn LFOs and oversampling are optional M2 items and were deliberately deferred. Composition, effects, preset library/history, and AI assistance are not implemented yet.
- The CPU number is a reproducible local baseline, not a guarantee for every host/audio-device configuration. FL Studio profiling is still required.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- Accepted compositions are session memory only until M6; project reopen intentionally restores synth parameters/routes but not M3 candidate/accepted clips.
- Direct MIDI begins at the next audio block using clip tempo. Host transport synchronization, reposition, and loop semantics are not claimed in M3.
- The M3 piano roll is a bounded view-only projection. Interactive editing belongs to M4, and isolated audio/WAV preview belongs to M5.
- JUCE distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain unresolved; builds are private local engineering artifacts only.

## Next smallest verifiable task

Install the verified M3 VST3, run the explicit Standalone and FL Studio M1-M3 human matrix, and record every result. Then begin M4 on a new stacked branch with the full responsive Silicon Dreams interface, interactive composition presentation, accessibility, and snapshot recovery.
