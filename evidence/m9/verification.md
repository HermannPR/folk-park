# M9 audio-stability verification

Date: 2026-08-24, America/Monterrey

## Reproduced deadline defect

The pre-optimization Release matrix rendered a normal one-bar four-part composition at every supported sample-rate/block-size pair:

- 44.1 kHz, 32–1024 samples: `0.531228x`–`0.541636x` real time.
- 48 kHz, 32–1024 samples: `0.575731x`–`0.587598x` real time.
- 96 kHz, 32–1024 samples: `1.14778x`–`1.16321x` real time.

Every 96 kHz case exceeded its audio deadline despite a finite peak near `0.372` and no MIDI overflow. This classifies a real CPU deadline failure rather than clipping or non-finite DSP in that fixture.

## Repair

The renderer now avoids processing unison lanes whose smoothed contribution is exactly inaudible and caches oscillator frequency/mip results until the exact pitch bit pattern changes. Live pitch changes invalidate the cache. No allocation, lock, clock, log, or file access was added to the callback.

## Post-repair measurements

- Normal four-part 48 kHz/512 composition: `0.127443x` real time versus `0.566012x` immediately before the repair; peak remains exactly `0.428440`.
- 44.1 kHz, 32–1024 samples: `0.119538x`–`0.135525x`.
- 48 kHz, 32–1024 samples: `0.126384x`–`0.135008x`.
- 96 kHz, 32–1024 samples: `0.252856x`–`0.262954x`.
- Heavy 96 kHz/64 case with two 8-lane oscillators, driven filter, and all six effect stages enabled: `0.669342x`, peak `0.262349`, zero over-unity/non-finite samples, 16 maximum voices, and 9 voice steals.
- Existing 16-voice × two-oscillator × eight-unison M2 benchmark: `0.398095x` after the repair.

The current default macOS output is the built-in MacBook Pro speakers at 48 kHz. The optimized Release Standalone was restarted after the build so the running process uses the repaired binary.

## Automated gates

- Complete Debug build and CTest: PASS, 18/18.
- Complete Release build and CTest: PASS, 19/19, including the packaged VST3 MIDI smoke test and the new Release matrix deadline assertions.
- Real-time allocation suite: PASS, zero allocations across its measured callback workload.
- pluginval 1.0.4 strictness 5: `SUCCESS`, including editor, processing, state, automation, buses, and 44.1/48/96 kHz across its supported block matrix.
- Installed/build VST3 executable parity and independent installed-bundle finite-stereo MIDI render: PASS.
- Installed VST3 executable SHA-256: `5377f6dcb0af792cacf0733415b972469e09978b8faa1f2b786b733517b89250`.
- Rollback bundle: `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T154551Z`.

## Remaining boundaries

The automated measurements prove large sequential render headroom and finite delivery on this Intel Core i9. They do not constitute an audible or FL Studio pass. The producer must listen to the restarted Release Standalone and a newly loaded FL Studio instance. M9 output-ceiling/gain work also remains: a deliberate supported extreme-gain fixture still demonstrates finite output above unity.
