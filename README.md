# folk park

`folk park` is an original wavetable synthesizer and deterministic composition assistant for producers. Release 0.1 targets FL Studio on Intel macOS as a 64-bit VST3 instrument and as a Standalone application.

The project is in milestone M0. It is not yet a playable release. Current work establishes a reproducible build, host declarations, state skeleton, bundled WebView proof, MIDI drag proof, tests, and validation evidence.

## Product principles

- Playable synth first, deterministic composition second, explicit producer acceptance always.
- Offline operation is the default; optional model providers may only produce validated musical intent.
- Audio continuity does not depend on the UI, network, database, or filesystem.
- No proprietary Serum code, UI, presets, wavetables, or private state format is copied or claimed compatible.

## Target

- Product: folk park 0.1.0
- Architecture: x86_64
- Host under test: FL Studio 26.1.4.5356 on macOS 15.7.9
- Formats: Standalone and VST3 instrument
- Toolchain observed: Apple clang 17.0.0, macOS SDK 15.5, Node 24.18.1, npm 11.16.0

See [plans/RELEASE_0_1.md](plans/RELEASE_0_1.md) for gates and [docs/PROGRESS.md](docs/PROGRESS.md) for verified status.
