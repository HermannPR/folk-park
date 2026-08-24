# folk park

`folk park` is an original wavetable synthesizer and deterministic composition assistant for producers. Release 0.1 targets FL Studio on Intel macOS as a 64-bit VST3 instrument and as a Standalone application.

## Final 0.1 product

The finished 0.1 product combines a playable dual-wavetable instrument with composition help for chords, melodies, bass, arpeggios, MIDI, presets, and sound exploration. Producers can work manually or use an optional guided assistant: describe the sound, answer focused questions, audition an explained parameter proposal, refine it conversationally, and explicitly accept or reject it. The assistant never silently changes a project.

The visual direction is the original **Silicon Dreams** interface: a fast, bundled, offline-capable synth/compose workspace rather than a copy of Serum or another proprietary product.

The intended final workflow is one connected instrument:

1. Build and play a sound manually with the complete synth, modulation, effects, preset controls, and a four-octave C2–B5 touch/mouse piano with an octave-shiftable computer-key zone, with live original 2D/3D wavetable and spectrum feedback for both oscillators.
2. Generate seeded chords, melody, bass, or arpeggios; inspect them in a piano roll; request a related or surprising variation; then explicitly accept the result.
3. Drag or export standards-compliant MIDI to FL Studio, or route accepted MIDI to another instrument.
4. Optionally ask the guided assistant for help. It asks focused sound-design questions, returns bounded parameter changes with reasons and uncertainty, supports reversible A/B audition, and waits for explicit acceptance.
5. Save projects with searchable presets, history, migrations, and missing-asset recovery before the signed Release 0.1 package is called complete.

## What works now — M6 hardening checkpoint

- A bundled React/TypeScript Silicon Dreams interface with responsive Synth, Compose, FX, History, and Settings navigation. History now contains the native preset and composition-history workspace; M7 assistant/provider controls remain clearly unfinished.
- Two live OSC A/B views render the actual bounded wavetable frames, current morph position, and derived spectrum. Three.js is capped at 30 FPS; Low Graphics uses a 12 FPS 2D waveform/spectrum fallback, Reduced Motion renders on state changes, and hidden views stop animation.
- A four-octave C2–B5 touch/mouse piano plus an octave-shiftable A–P computer-key zone. Preview notes cross a fixed native queue, release on pointer cancel/blur/hide/editor close/panic, and held macOS keys sustain once without repeat retriggering.
- Complete host-aware control surfaces for both oscillators, mixer, filter, three envelopes, four LFOs, and a reviewed 32-route modulation matrix. Header Undo/Redo synchronize pending APVTS state before acting.
- Interactive candidate-note selection and bounded pitch, timing, duration, and velocity editing. Editing never mutates the previously accepted bundle and requires a fresh explicit Accept before delivery.
- A strict complete native UI snapshot restores parameters, actual wavetable tables, route state, composition state, version/status, and active voices after UI reload. Malformed, future, non-finite, duplicate, or oversize payloads cannot replace the last valid view.

- Intel `x86_64` Debug and Release builds for Standalone and VST3.
- A deterministic 16-voice engine with released/quietest-then-oldest voice stealing.
- Two independent legal project-generated wavetable oscillators with position, coarse/fine tuning, phase/random/reset behavior, level/pan, and up to eight unison lanes with detune/spread/blend.
- Eleven band-limited mip levels per wavetable frame, a sine/triangle sub, deterministic white/pink noise, three envelopes, four free/synced/retriggerable LFOs, and a stable low/high/band-pass filter with resonance, drive, key tracking, and envelope depth.
- A central modulation registry with ten sources, thirteen destinations, three curves, and at most 32 validated routes. The M4 matrix edits, reviews, and transactionally publishes all 32 bounded slots.
- Deterministic user-WAV conversion away from audio with strict bounds, SHA-256 metadata, a preview state, explicit confirmation, content-addressed source retention, atomic publication, and a 128-sample table crossfade. Project state records validated asset references and exposes exact hash-and-size recovery if the local source is missing.
- Ten-millisecond smoothing and fixed per-lane fades for live wavetable position, pitch, level, pan, detune/spread/blend, and unison-count changes.
- Stable automatable host parameters and versioned state round trips.
- Bundled WebView controls using host-aware parameter attachments, safe WAV review/confirm actions, modulation actions, preview MIDI, panic, and live native status; audio behavior is tested with the editor open and closed.
- Native synth/processor/import/state tests, zero-allocation audio instrumentation, spectral and CPU evidence, plus a Release smoke test that loads the built VST3 as an external host and renders finite stereo audio from MIDI.
- pluginval strictness 5 validation across editor, state, automation, buses, and the required sample-rate/block-size matrix.
- Strict version 1 `MusicIntent` and `GeneratedClip` schemas and typed models with key, scale, tempo, meter, bars, requested parts, genre/emotion, six composition macros, arpeggiator settings, note range, polyphony, event caps, seed, generator version, and parent lineage.
- Deterministic offline chord progressions with triads/sevenths, functional movement, V-I cadence, inversions, and bounded voice leading; chord-aware melodies with contour, motifs, rests, passing tones, and leap limits; bounded bass; and five seeded arpeggio orders.
- `More Like This` preserves musical context and parent IDs while producing a controlled difference. `Surprise Me` produces a separate bounded candidate. Neither replaces accepted material.
- A Compose UI with seed/key/scale/BPM/bars, density/rhythm/tension/humanization/repetition/variation, part selection, an editable colored piano roll, candidate status, and an explicit Accept action.
- One accepted `GeneratedClip` bundle feeds all MIDI delivery paths: multitrack SMF with tempo/time-signature metadata and explicit note-offs, reopen verification at multiple PPQ resolutions, temporary-file drag, save chooser export, and fixed-schedule direct MIDI with correct block offsets and tracked Stop note-offs.
- Direct accepted MIDI also drives the internal synth. Publication is atomic at an audio-block boundary, and measured composition scheduling adds zero allocations to the callback with a pre-sized host MIDI buffer.
- Strict `SoundIntent` and `ParameterProposal` schemas plus typed validators are present as the foundation for the requested guided sound walkthrough. The conversational assistant, provider integration, A/B audition, and parameter application are not implemented yet.
- A fixed, independently bypassable serial chain: Distortion → Chorus → tempo-synced Delay → Reverb → Compressor → Parametric EQ. All 29 effect parameters are stable host automation/state surfaces, new instances default to gain-safe bypass, and each transition uses a 10 ms crossfade.
- A complete host-aware FX workspace exposes every implemented effect control. Values are bounded at both the parameter and DSP boundaries, and malformed/non-finite samples cannot escape the chain.
- Accepted compositions can be rendered to stereo 24-bit/48 kHz WAV on a worker thread. Rendering copies immutable synth, effect, modulation, and A/B wavetable snapshots into a separate engine, writes transactionally, validates the result before replacement, supports cancellation, and never resets or seeks live voices.
- Versioned deterministic `.folkparkpreset` files capture all 102 normalized parameters, modulation routes, ordered effects, metadata, and up to two content-addressed user WAVs. The native workspace supports explicit Save As/overwrite, import, favorites, missing-asset relink, and transactional recall.
- Accepted compositions are stored in a searchable transactional SQLite history with stable IDs, lineage, tags, favorites, recoverable soft deletion, comparison, retention, and exact recall. Database failure is isolated from acceptance and audio.
- Versioned bounded host project state now restores the complete native sound snapshot, imported wavetable references, dirty status, accepted composition, and history lineage without requiring the editor. Missing or malformed assets/payloads leave the complete live state unchanged until explicit recovery succeeds.

The current build is an M6 hardening checkpoint, not the complete 0.1 instrument. The integrated UI build/tests/lint passed 10/10 at the preceding checkpoint, and the current complete Debug build and 10/10 native suites pass with project-state/asset recovery coverage. The last fully gated Release/pluginval artifact remains M5; the complete M6 Release, pluginval, artifact, visual, and FL Studio gates have not yet run. FL Studio insertion, piano focus/repeat behavior, project/preset reopen, missing-asset recovery, effect automation/tempo sync, MIDI drag/routing, and WAV import/playback remain human tests.

Direct MIDI starts on the next audio block at the accepted clip tempo; transport-synchronized start/reposition behavior remains a host-level limitation to verify and refine. M5 WAV rendering requires the explicitly accepted bundle and writes only to the producer-selected destination. Native state tests prove project recovery, but actual FL Studio save-close-reopen and listening remain required.

## Coming soon and later

| Milestone | Producer-facing result | Status |
| --- | --- | --- |
| M2 | Dual wavetable oscillators, safe user-WAV import, envelopes/LFOs, modulation matrix, multimode filter | Implemented foundation; FL Studio human run pending |
| M3 | Deterministic chord, melody, bass, and arpeggio generation with candidate preview, acceptance, MIDI export/drag/direct route | Implemented foundation; FL Studio human drag/route run pending |
| M4 | Full responsive Silicon Dreams React interface, actual live 2D/3D wavetable and spectrum views, safe four-octave touch/computer piano audition, interactive composition editing, accessible low-graphics mode | Implemented foundation; FL Studio human UI run pending |
| M5 | Distortion, chorus, synced delay, reverb, compressor, EQ, and isolated WAV preview | Automated gates passed; FL Studio human effects/WAV run pending |
| M6 | Searchable native presets, reversible history, migrations, content-addressed assets, and crash-safe project recovery | Current hardening checkpoint; Debug passes, full Release/pluginval/FL gate pending |
| M7 | Offline Jarvis text workflow and guided AI sound walkthrough; optional secure provider | Planned |
| M8 | FL Studio matrix, performance hardening, packaging, legal/asset audit, and release documentation | Planned |

The guided sound workflow is specified in [docs/PRODUCT_AMENDMENTS.md](docs/PRODUCT_AMENDMENTS.md). M3 now provides its strict intent/proposal schema foundation; M4/M6 add the production interaction and reversible preview/history foundations; M7 adds the offline conversation and optional secure provider.

## Product principles

- Playable synth first, deterministic composition second, explicit producer acceptance always.
- Offline operation is the default; optional model providers may only produce validated musical intent.
- The optional sound walkthrough asks focused production questions, previews a bounded parameter proposal, explains it, and requires explicit acceptance before applying it.
- Audio continuity does not depend on the UI, network, database, or filesystem.
- No proprietary Serum code, UI, presets, wavetables, or private state format is copied or claimed compatible.

## Build and evidence

Run `./scripts/bootstrap_macos.sh` once, then `./scripts/build_x86_64.sh`. Tests use the Debug and Release CMake presets documented in [plans/RELEASE_0_1.md](plans/RELEASE_0_1.md). Verified checkpoint details, exact limitations, and evidence paths live in [docs/PROGRESS.md](docs/PROGRESS.md).

## Target

- Product: folk park 0.1.0
- Architecture: x86_64
- Host under test: FL Studio 26.1.4.5356 on macOS 15.7.9
- Formats: Standalone and VST3 instrument
- Toolchain observed: Apple clang 17.0.0, macOS SDK 15.5, Node 24.18.1, npm 11.16.0

Private engineering can continue, but distributing binaries remains blocked until the JUCE license, signing/notarization, product identity, and asset-rights decisions in [docs/OPEN_DECISIONS.md](docs/OPEN_DECISIONS.md) are resolved.
