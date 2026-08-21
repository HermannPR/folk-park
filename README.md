# folk park

`folk park` is an original wavetable synthesizer and deterministic composition assistant for producers. Release 0.1 targets FL Studio on Intel macOS as a 64-bit VST3 instrument and as a Standalone application.

## Product vision

The finished 0.1 product combines a playable dual-wavetable instrument with composition help for chords, melodies, bass, arpeggios, MIDI, presets, and sound exploration. Producers can work manually or use an optional guided assistant: describe the sound, answer focused questions, audition an explained parameter proposal, refine it conversationally, and explicitly accept or reject it. The assistant never silently changes a project.

The visual direction is the original **Silicon Dreams** interface: a fast, bundled, offline-capable synth/compose workspace rather than a copy of Serum or another proprietary product.

## What works now — M1 checkpoint

- Intel `x86_64` Debug and Release builds for Standalone and VST3.
- A deterministic 16-voice engine with released/quietest-then-oldest voice stealing.
- One legal project-generated wavetable oscillator with sine and triangle choices, a sine sub oscillator, amp ADSR, one-pole low-pass filter, master gain, sustain pedal, pitch bend, all-notes-off, and panic.
- Stable automatable host parameters and versioned state round trips.
- Bundled WebView controls using host-aware parameter attachments; audio behavior is tested with the editor open and closed.
- Native synth/processor tests plus a Release smoke test that loads the built VST3 as an external host and renders finite stereo audio from MIDI.
- pluginval strictness 5 validation across editor, state, automation, buses, and the required sample-rate/block-size matrix.

The current build is a verified playable engineering slice, not the complete instrument. FL Studio insertion, playability, project reopen, automation, and MIDI routing remain explicitly marked as human tests until they are performed in FL Studio.

## Coming next

| Milestone | Producer-facing result | Status |
| --- | --- | --- |
| M2 | Dual wavetable oscillators, safe user-WAV import, envelopes/LFOs, modulation matrix, multimode filter | Next |
| M3 | Deterministic chord, melody, bass, and arpeggio generation with MIDI preview/export/drag | Planned |
| M4 | Full responsive Silicon Dreams React interface and accessible low-graphics mode | Planned |
| M5 | Distortion, chorus, synced delay, reverb, compressor, EQ, and isolated WAV preview | Planned |
| M6 | Searchable presets, reversible history, migrations, and crash-safe persistence | Planned |
| M7 | Offline Jarvis text workflow and guided AI sound walkthrough; optional secure provider | Planned |
| M8 | FL Studio matrix, performance hardening, packaging, legal/asset audit, and release documentation | Planned |

The guided sound workflow is specified now in [docs/PRODUCT_AMENDMENTS.md](docs/PRODUCT_AMENDMENTS.md), but it is not yet implemented.

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
