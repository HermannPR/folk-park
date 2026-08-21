# Assumptions

## Confirmed environment

- Development machine is Intel x86_64 running macOS 15.7.9 (24G830).
- Apple clang 17.0.0 and macOS SDK 15.5 are available through Command Line Tools.
- Full Xcode is not currently selected; `/Library/Developer/CommandLineTools` is active.
- FL Studio 26.1.4.5356 is installed and contains an x86_64 host binary.
- Node 24.18.1 and npm 11.16.0 are installed.
- CMake, Ninja, JUCE, and GitHub CLI were absent at initial inspection.

## Product defaults used for M0

- User-facing name is `folk park`; code/CMake name is `FolkPark`; version is 0.1.0.
- Bundle identifier remains the provisional `com.folkpark.audio.folkpark`.
- Manufacturer code `FlPk` and plug-in code `FkP1` are provisional until release audit.
- macOS deployment target is 12.0 unless the pinned SDK/framework proves incompatible.
- VST3 and Standalone are x86_64-only in 0.1.
- The first slice uses legal project-created waveforms and no Serum importer.
- The assistant operates offline and deterministically; no API key or remote model is needed for M0-M3.
- FL Studio modifications always require explicit user action. Drag-and-drop MIDI is the primary delivery path.
