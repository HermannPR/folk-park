# M2 verification record

Date: 2026-08-20 (America/Monterrey)

## Automated proof

- Debug CTest: 3/3 passed (native synthesis/import, real-time allocation, processor/state/UI).
- Release CTest: 4/4 passed (the same three plus an external-host smoke against the actual built VST3).
- Built Release Standalone and VST3 are thin `x86_64` Mach-O artifacts. Both local ad-hoc signatures pass `codesign --verify --deep --strict`.
- Installed user VST3 hash matches the validated build; it is thin `x86_64` and its local ad-hoc signature verifies.
- VST3 binary SHA-256: `5ceeddbffd812f2a535253e2548a6d27576ac0b2d1bccadc7c1389edb303266b`.
- pluginval 1.0.4 strictness 5: `SUCCESS`; log SHA-256 `fd382d5174291094541138f97c12587bd894c1705be6b7aad96fab25a657e0ea`.
- Visual M2 Standalone inspection: passed; screenshot retained alongside this file.

## Audio and import behavior covered

- Dual independent oscillator B audibility, stereo pan/unison spread, and finite rendering.
- Eleven-level FFT mip construction and frequency-dependent selection. At 48 kHz/8 kHz, mip 9 retained two harmonics and measured rejected-to-retained spectral energy of `1.73254e-14`.
- Low/high/band-pass stability at bounded resonance, drive, cutoff, key tracking, envelope depth, and high note extremes.
- Deterministic white noise, distinct deterministic pink noise, three-envelope release without stuck voices, and all four LFO shapes/sources reaching a registered destination.
- Ten modulation sources, thirteen destinations, three curves, finite normalized amounts, 32-route bound, invalid-enum rejection, and atomic complete-snapshot activation.
- Valid deterministic stereo WAV conversion and repeat equality; metadata/hash/preview/continuity proof; explicit confirmation before publication; corrupt/missing/silent/non-finite/undersized/oversized/excess-channel rejection.
- A validated table swap crossfades over 128 samples and retires the previous fixed slot only after the fade.
- Aggressive simultaneous oscillator position/pitch/pan/unison/detune/spread/blend automation remains under the tested block-boundary discontinuity threshold.
- Measured rendering allocated zero times for 32 audio blocks including atomic table/matrix activation and table crossfade.

## Performance observation

Release test on this Mac at 48 kHz with 16 voices, two oscillators, eight unison lanes each, maximum resonance/drive, and 40 blocks of 512 samples:

- Audio represented: 426.667 ms
- Rendering elapsed: 327.983 ms
- Realtime ratio: `0.768709×`

This is a local regression baseline, not a universal performance guarantee.

## Human or later-milestone work

- Physical Standalone MIDI/audio-device behavior was not run.
- FL Studio discovery, insertion, playback, control/automation behavior, state reopen, MIDI routing, and drag/drop were not run.
- The real file chooser and WAV review/confirm/cancel journey were not run manually in Standalone or FL Studio.
- Confirmed imported tables are session-memory assets and are not restored from project state until M6 asset persistence exists.
- The full 32-route UI, composition assistant, effects, preset/history system, and guided AI sound walkthrough are later milestones.
- The optional external Steinberg validator was unavailable; pluginval reports that subtest as skipped.
- Distribution signing/notarization and licensing/legal release gates remain open.
