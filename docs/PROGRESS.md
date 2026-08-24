# Progress

## Current checkpoint

- Milestone: M7 — offline Jarvis text, guided sound proposals, and secure provider boundary
- Status: M7 versioned request/response/catalog contract checkpoint passes focused tests; offline engines, A/B integration, UI, provider/Keychain implementation, full gates, and all FL Studio human runs remain required
- Date: 2026-08-23 (America/Monterrey)
- Branch: `feat/m7-guided-assistant`, stacked exactly on `feat/m6-presets-history`

## M7 contract checkpoint

- Added versioned typed `AssistantRequest`/`AssistantResponse` variants for composition versus sound and offline/mock/remote processing origins.
- Bounded prompts at 1,024 characters, required matching UUID/target/origin/typed context, rejected mixed variants and stale responses, and made per-request consent mandatory for remote origin.
- Added one asynchronous non-audio `AssistantProvider` interface with cancellation and at-most-once completion semantics; no real provider or network dependency is selected yet.
- Preserved the original 73-ID proposal contract as `parameter-proposal-v1.schema.json` and migrated the current proposal schema/model to v2 for all 102 host parameters.
- Resolved every proposed ID against `src/common/ParameterIds.h`; v1 rejects effect IDs, v2 accepts the complete catalog, and unknown/duplicate IDs, invalid values, missing reasons, and implicit acceptance are rejected.
- Recorded ADR-0008 for offline-first composition text, guided sound A/B, provider consent, credential boundaries, and the open real-provider decision.
- Focused assistant model/contract build and CTest: PASS, 1/1.

## M7 work still required

- Implement bounded deterministic offline composition text parsing and adaptive guided sound questions/proposals.
- Implement a deterministic mock provider plus cancellation/timeout/retry state tests.
- Integrate immutable original/proposal A/B snapshots and explicit accept/reject with processor/project state without audio-thread work.
- Add the native bridge and complete typed Jarvis conversation/settings UI.
- Implement and test the macOS Keychain credential abstraction. Do not add a real adapter until the product-owner decision and privacy UX exist.
- Run complete UI, Debug, Release, validator, artifact, visual, security, and evidence gates. Every FL Studio case remains HUMAN RUN REQUIRED.

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
- User-drawn LFOs and oversampling are optional M2 items and were deliberately deferred. Native preset/history integration is in M6 hardening; the M7 guided assistant is not implemented yet.
- The CPU number is a reproducible local baseline, not a guarantee for every host/audio-device configuration. FL Studio profiling is still required.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- Accepted compositions and their delivery state now round-trip in the bounded M6 project payload and remain searchable in local history. Actual FL Studio project reopen is not yet human-verified.
- Direct MIDI begins at the next audio block using clip tempo. Host transport synchronization, reposition, and loop semantics are not claimed in M3.
- M5 preview is an exported, validated WAV rather than an internal transport. Audition/import behavior in FL Studio remains a human workflow check.
- The Vite build reports direct `eval` in JUCE's pinned `check_native_interop.js` Android compatibility helper. The macOS bundle loads only embedded local resources; no project source or runtime URL uses a remote origin.
- JUCE distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain unresolved; builds are private local engineering artifacts only.

## Next smallest verifiable task

Commit the passing M7 contract checkpoint, then implement the deterministic offline composition-text parser, adaptive guided-question engine, bounded catalog proposal mapper, and mock provider on the same typed boundary. Add adversarial fixtures before processor or UI integration, and keep every unexecuted FL Studio case marked HUMAN RUN REQUIRED.
