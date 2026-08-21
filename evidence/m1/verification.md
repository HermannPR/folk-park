# M1 verification record

Date: 2026-08-20 (America/Monterrey)

## Automated proof

- Debug Standalone/VST3 and both Debug test executables built successfully.
- Debug CTest: 2/2 passed.
- Release Standalone/VST3 and all three Release test executables built successfully.
- Release CTest: 3/3 passed.
- The Release external-host smoke loaded the built VST3 bundle and rendered audible finite centered stereo output from MIDI note-on.
- Release Standalone: Mach-O 64-bit executable `x86_64`.
- Release VST3: Mach-O 64-bit bundle `x86_64`; local ad-hoc signature verifies.
- Installed VST3: Mach-O 64-bit bundle `x86_64`; local ad-hoc signature verifies.
- pluginval 1.0.4 strictness 5: `SUCCESS`; detailed log retained alongside this file.
- Visual M1 WebView/native bridge and controls: inspected; screenshot retained alongside this file.

## Automated behaviors covered

- Silence before note-on and finite audible audio after note-on.
- Centered stereo rendering.
- Fixed 16-voice maximum and deterministic oldest-active stealing.
- Panic and release return to silence without stuck voices.
- Deterministic renders for identical state and MIDI.
- Stable parameter presence and state serialization/restoration.
- Editor construction and identical rendering with the editor open or closed.
- Atomic message-thread panic handoff.
- Built VST3 scanning, instantiation, bus layout, reported release tail, MIDI input, and output.

## Not automated or not yet run

- Physical MIDI/audio device behavior in Standalone.
- FL Studio discovery, insertion, play, automation, state reopen, MIDI routing, and drag/drop.
- The optional Steinberg validator executable was not available to pluginval.
- Distribution signing, notarization, installer, and license audit.
