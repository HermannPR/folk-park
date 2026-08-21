# folk park

`folk park` is an original wavetable synthesizer and deterministic composition assistant for producers. Release 0.1 targets FL Studio on Intel macOS as a 64-bit VST3 instrument and as a Standalone application.

## Final 0.1 product

The finished 0.1 product combines a playable dual-wavetable instrument with composition help for chords, melodies, bass, arpeggios, MIDI, presets, and sound exploration. Producers can work manually or use an optional guided assistant: describe the sound, answer focused questions, audition an explained parameter proposal, refine it conversationally, and explicitly accept or reject it. The assistant never silently changes a project.

The visual direction is the original **Silicon Dreams** interface: a fast, bundled, offline-capable synth/compose workspace rather than a copy of Serum or another proprietary product.

## What works now — M2 checkpoint

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

The current build is a verified M2 engineering checkpoint, not the complete 0.1 instrument. FL Studio insertion, playability, automation, state reopen, import behavior in a real project, and MIDI routing remain explicitly marked as human tests until they are performed in FL Studio.

## Coming soon and later

| Milestone | Producer-facing result | Status |
| --- | --- | --- |
| M2 | Dual wavetable oscillators, safe user-WAV import, envelopes/LFOs, modulation matrix, multimode filter | Current; automated gates passed, FL Studio human run pending |
| M3 | Deterministic chord, melody, bass, and arpeggio generation with MIDI preview/export/drag | Coming next |
| M4 | Full responsive Silicon Dreams React interface and accessible low-graphics mode | Planned |
| M5 | Distortion, chorus, synced delay, reverb, compressor, EQ, and isolated WAV preview | Planned |
| M6 | Searchable presets, reversible history, migrations, and crash-safe persistence | Planned |
| M7 | Offline Jarvis text workflow and guided AI sound walkthrough; optional secure provider | Planned |
| M8 | FL Studio matrix, performance hardening, packaging, legal/asset audit, and release documentation | Planned |

The guided sound workflow is specified now in [docs/PRODUCT_AMENDMENTS.md](docs/PRODUCT_AMENDMENTS.md), but it is not yet implemented. M3 will define the validated musical-intent schemas it builds on; M4/M6 add the interaction and reversible preview/history foundations; M7 adds the offline conversation and optional secure provider.

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
