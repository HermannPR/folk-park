# folk park

`folk park` is an original wavetable synthesizer and deterministic composition assistant for producers. Release 0.1 targets FL Studio on Intel macOS as a 64-bit VST3 instrument and as a Standalone application.

## Final 0.1 product

The finished 0.1 product combines a playable dual-wavetable instrument with composition help for chords, melodies, bass, arpeggios, MIDI, presets, and sound exploration. Producers can work manually or use an optional guided assistant: describe the sound, answer focused questions, audition an explained parameter proposal, refine it conversationally, and explicitly accept or reject it. The assistant never silently changes a project.

The visual direction is the original **Silicon Dreams** interface: a fast, bundled, offline-capable synth/compose workspace rather than a copy of Serum or another proprietary product.

The intended final workflow is one connected instrument:

1. Build and play a sound manually with the complete synth, modulation, effects, and preset controls, with live original 2D/3D wavetable and spectrum feedback for both oscillators.
2. Generate seeded chords, melody, bass, or arpeggios; inspect them in a piano roll; request a related or surprising variation; then explicitly accept the result.
3. Drag or export standards-compliant MIDI to FL Studio, or route accepted MIDI to another instrument.
4. Optionally ask the guided assistant for help. It asks focused sound-design questions, returns bounded parameter changes with reasons and uncertainty, supports reversible A/B audition, and waits for explicit acceptance.
5. Save projects with searchable presets, history, migrations, and missing-asset recovery before the signed Release 0.1 package is called complete.

## What works now — M3 checkpoint

- Intel `x86_64` Debug and Release builds for Standalone and VST3.
- A deterministic 16-voice engine with released/quietest-then-oldest voice stealing.
- Two independent legal project-generated wavetable oscillators with position, coarse/fine tuning, phase/random/reset behavior, level/pan, and up to eight unison lanes with detune/spread/blend.
- Eleven band-limited mip levels per wavetable frame, a sine/triangle sub, deterministic white/pink noise, three envelopes, four free/synced/retriggerable LFOs, and a stable low/high/band-pass filter with resonance, drive, key tracking, and envelope depth.
- A central modulation registry with ten sources, thirteen destinations, three curves, and at most 32 validated routes. The engineering UI edits one reviewed route; complete matrices already serialize safely in plug-in state.
- Deterministic user-WAV conversion away from audio with strict bounds, SHA-256 metadata, a preview state, explicit confirmation, atomic publication, and a 128-sample table crossfade. Confirmed imported tables are currently session-memory assets and are not yet embedded in project state.
- Ten-millisecond smoothing and fixed per-lane fades for live wavetable position, pitch, level, pan, detune/spread/blend, and unison-count changes.
- Stable automatable host parameters and versioned state round trips.
- Bundled WebView controls using host-aware parameter attachments, safe WAV review/confirm actions, modulation actions, panic, and live native status; audio behavior is tested with the editor open and closed.
- Native synth/processor/import/state tests, zero-allocation audio instrumentation, spectral and CPU evidence, plus a Release smoke test that loads the built VST3 as an external host and renders finite stereo audio from MIDI.
- pluginval strictness 5 validation across editor, state, automation, buses, and the required sample-rate/block-size matrix.
- Strict version 1 `MusicIntent` and `GeneratedClip` schemas and typed models with key, scale, tempo, meter, bars, requested parts, genre/emotion, six composition macros, arpeggiator settings, note range, polyphony, event caps, seed, generator version, and parent lineage.
- Deterministic offline chord progressions with triads/sevenths, functional movement, V-I cadence, inversions, and bounded voice leading; chord-aware melodies with contour, motifs, rests, passing tones, and leap limits; bounded bass; and five seeded arpeggio orders.
- `More Like This` preserves musical context and parent IDs while producing a controlled difference. `Surprise Me` produces a separate bounded candidate. Neither replaces accepted material.
- A condensed Compose UI with seed/key/scale/BPM/bars, density/rhythm/tension/humanization/repetition/variation, part selection, colored piano-roll preview, candidate status, and an explicit Accept action.
- One accepted `GeneratedClip` bundle feeds all MIDI delivery paths: multitrack SMF with tempo/time-signature metadata and explicit note-offs, reopen verification at multiple PPQ resolutions, temporary-file drag, save chooser export, and fixed-schedule direct MIDI with correct block offsets and tracked Stop note-offs.
- Direct accepted MIDI also drives the internal synth. Publication is atomic at an audio-block boundary, and measured composition scheduling adds zero allocations to the callback with a pre-sized host MIDI buffer.
- Strict `SoundIntent` and `ParameterProposal` schemas plus typed validators are present as the foundation for the requested guided sound walkthrough. The conversational assistant, provider integration, A/B audition, and parameter application are not implemented yet.

The current build is a verified M3 engineering checkpoint, not the complete 0.1 instrument. Debug passed 6/6 automated suites; Release passed 7/7 including loading and rendering through the actual built VST3; pluginval 1.0.4 strictness 5 passed. FL Studio insertion, playability, automation, state reopen, MIDI drag, and direct routing remain explicitly marked as human tests until they are performed in FL Studio.

Accepted compositions currently live in session memory and are deliberately not written into plug-in state until M6 history/persistence exists. Direct MIDI starts on the next audio block at the accepted clip tempo; transport-synchronized start/reposition behavior remains a host-level limitation to verify and refine. The M3 preview is MIDI-only; isolated WAV audition is M5.

## Coming soon and later

| Milestone | Producer-facing result | Status |
| --- | --- | --- |
| M2 | Dual wavetable oscillators, safe user-WAV import, envelopes/LFOs, modulation matrix, multimode filter | Implemented foundation; FL Studio human run pending |
| M3 | Deterministic chord, melody, bass, and arpeggio generation with candidate preview, acceptance, MIDI export/drag/direct route | Current; automated gates passed, FL Studio human drag/route run pending |
| M4 | Full responsive Silicon Dreams React interface, original live 2D/3D wavetable and spectrum views, interactive composition editing, accessible low-graphics mode | Coming next |
| M5 | Distortion, chorus, synced delay, reverb, compressor, EQ, and isolated WAV preview | Planned |
| M6 | Searchable presets, reversible history, migrations, and crash-safe persistence | Planned |
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
