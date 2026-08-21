# Progress

## Current checkpoint

- Milestone: M4 — Silicon Dreams UI, live wavetable visualization, and safe audition
- Status: UI, Debug, Release, built-VST3, architecture, signature, offline, visual, held-key, and pluginval gates passed; FL Studio human insertion/focus/resize/drag/routing runs remain required
- Date: 2026-08-21 (America/Monterrey)
- Branch: `feat/m4-silicon-dreams-ui`, stacked on M3 draft PR #3

## Implemented M4 checkpoint

- Added pinned React/React DOM/TypeScript/Vite/Three.js dependencies and a reproducible offline production bundle embedded as three JUCE resources. No development server, CDN, remote font, tracker, or runtime network origin is used.
- Replaced the condensed page with the responsive Silicon Dreams Synth/Compose/FX/History/Settings information architecture. M5–M7 views remain explicit placeholders.
- Added real OSC A/B wavetable visualization from fixed native table copies: 30 FPS bounded Three.js frame/morph scene, position marker, derived spectrum, 12 FPS 2D Low Graphics fallback, state-change Reduced Motion rendering, resize handling, and hidden-view suspension.
- Added a four-octave C2–B5 touch/mouse piano and octave-shiftable A–P computer zone. The fixed SPSC preview queue becomes MIDI only inside `processBlock`; pointer cancel, blur, hide, editor close, Panic, overflow, and queue reset release notes safely.
- Made held keys idempotent in both layers. React ignores repeat/already-held sources, and native duplicate note-on/off commands emit nothing. Twelve injected held-`A` keydowns produced exactly one active C3 voice; release returned to zero.
- Added host-aware controls for both oscillator positions/levels, mixer sources, filter, all envelopes, and four LFO shapes/rates. Immediate Undo flushes pending APVTS state before using the shared undo manager, and Redo restores the gesture.
- Added a complete 32-route modulation review workspace with strict bounded native parsing and atomic publication.
- Added bounded composition-note pitch/start/duration/velocity editing. Candidate edits are validated, stable-sorted, and published transactionally; the accepted bundle remains immutable and editing requires a new explicit acceptance.
- Expanded the version 1 complete UI snapshot to include actual A/B table frames, all parameters, routes, composition state, status, architecture, and voices. Malformed/future/non-finite/duplicate/oversize snapshots are rejected before view replacement.

## M4 commands and exact results

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

## M4 evidence

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

## Human-required status

- Standalone physical keyboard/audio-device playability: PARTIAL. Automated real-window C3 hold/repeat/release passed; listening through the user's selected audio device remains human.
- FL Studio discovery/insertion, dual-oscillator playability, every exposed control, automation write/read, save-close-reopen state, panic/no-stuck-note, editor reopen/resize/scroll, and MIDI routing/drag: HUMAN RUN REQUIRED.
- M4 four-octave pointer play, A–P focus ownership, macOS hold/repeat behavior, Oct−/Oct+ mapping, compact horizontal keyboard scroll, low-graphics/reduced-motion behavior, and editor close while holding notes inside FL Studio: HUMAN RUN REQUIRED.
- M4 actual A/B visual response to host automation/import, full matrix review/apply/discard, immediate Undo/Redo, candidate note editing, and accepted-versus-candidate delivery inside FL Studio: HUMAN RUN REQUIRED.
- WAV import through the real macOS chooser, preview/confirm/cancel, table audition, FL project save/reopen limitation, and failure recovery: HUMAN RUN REQUIRED.
- M3 candidate generation/preview/accept interaction in the physical Standalone: HUMAN RUN REQUIRED.
- M3 `.mid` drag into the FL Studio piano roll/channel workflow and musical parity after import: HUMAN RUN REQUIRED.
- M3 Wrapper output-port routing into a second FL instrument, timing/note-off/Stop/panic behavior, and save-close-reopen session limitation: HUMAN RUN REQUIRED.
- M1's outstanding FL Studio human matrix remains outstanding; M2 automation does not turn it into a pass.

## Risks and limitations

- A confirmed imported wavetable lives in bounded session memory only. Source audio or converted table persistence is intentionally deferred to M6; reopening a project currently restores parameters/routes but uses the built-in table. The UI states the review boundary but cannot yet warn on project reopen.
- The M4 interface edits all 32 route slots, but host automation of route structure is not supported; route changes are reviewed native transactions stored in plug-in state.
- Three.js is a presentation dependency only. The measured local analysis cost and frame caps do not replace FL Studio CPU/GPU profiling on the target machine.
- User-drawn LFOs and oversampling are optional M2 items and were deliberately deferred. Composition, effects, preset library/history, and AI assistance are not implemented yet.
- The CPU number is a reproducible local baseline, not a guarantee for every host/audio-device configuration. FL Studio profiling is still required.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- Accepted compositions are session memory only until M6; project reopen intentionally restores synth parameters/routes but not M3 candidate/accepted clips.
- Direct MIDI begins at the next audio block using clip tempo. Host transport synchronization, reposition, and loop semantics are not claimed in M3.
- The M4 piano roll supports bounded note editing, but isolated audio/WAV preview still belongs to M5.
- The Vite build reports direct `eval` in JUCE's pinned `check_native_interop.js` Android compatibility helper. The macOS bundle loads only embedded local resources; no project source or runtime URL uses a remote origin.
- JUCE distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain unresolved; builds are private local engineering artifacts only.

## Next smallest verifiable task

Run the installed M4 VST3 through the explicit FL Studio M1–M4 human matrix and record every result. Then begin M5 on a new stacked branch with ordered bypassable effects and isolated WAV preview while keeping the M4 UI/audio boundaries intact.
