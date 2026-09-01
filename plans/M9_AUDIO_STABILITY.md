# M9 Audio Stability and Performance plan

Status: active on `feat/m9-audio-stability`, stacked on the verified Rhythm Lab R1 native foundation and persistent-keyboard checkpoint.

## Problem statement

The producer reports intermittent saturation, crackling, and complete distortion while auditioning a generated composition. This is a release-blocking audio defect. M9 must determine whether each failure is caused by internal over-level audio, unstable DSP/feedback, non-finite containment, excessive voice pressure, or a missed host audio deadline. A transparent safety stage may be useful, but it must not be used to conceal the original cause.

## Evidence at milestone start

- The callback contains non-finite output samples and counts those incidents.
- Direct and preview MIDI queues count bounded overflows and panic safely.
- The effect chain contains intermediate values only to `[-32, 32]`; this is a finite-safety bound, not a 0 dBFS output guarantee.
- Multiple synth voices are summed before effects. The current diagnostics expose active voices but do not retain peak, over-unity, voice-pressure, or callback-budget measurements.
- Current automated tests prove finite output and zero callback allocations. They do not prove gain safety or deadline headroom for dense generated compositions with costly synth/effect settings.
- Release CTest and pluginval passed immediately before M9, so the reported audible defect is not yet reproduced by the existing validators.

## Classification model

Every reproduced failure must be classified before repair:

1. **Level overload** — finite samples exceed the intended output ceiling or downstream device/host headroom.
2. **DSP instability** — filter, delay, reverb, compressor, modulation, or parameter transition creates runaway, discontinuous, or non-finite output.
3. **Voice pressure** — dense overlapping composition events, long releases, unison, or repeated note ownership creates excessive summing or steals voices audibly.
4. **Deadline miss** — callback execution consumes too much of the available block duration and the host/device underruns.
5. **Delivery fault** — duplicate/stuck MIDI, queue overflow, transport restart, or timing discontinuity creates unintended note density.

## M9 stages

### S1 — Reproduction and telemetry

- Add deterministic dense-composition fixtures that exercise simultaneous chords, melody, bass, arpeggio, preview notes, long releases, both oscillators, maximum unison, filter extremes, and every effect stage.
- Measure per-case sample peak, RMS, over-unity sample count, non-finite count, maximum active voices, steals, MIDI overflow, and render-time-to-audio-time ratio.
- Run the matrix at 44.1, 48, and 96 kHz and block sizes 32, 64, 128, 256, 512, and 1024.
- Extend bounded diagnostics with atomic numeric counters only. No logging, formatting, allocation, locks, clock calls, or UI work may enter the callback.
- Retain a short exact reproduction preset/seed and expected metrics without storing user project names, paths, or audio.

Gate: at least one current-path stress fixture records whether the failure is level, stability, voice, deadline, or delivery related. Baselines must be reproducible, finite, and allocation-free.

### S2 — Gain staging and DSP containment

- Establish explicit internal headroom at voice, synth-bus, effect-stage, and master boundaries.
- Test chords and full arrangements rather than isolated notes.
- Smooth any new gain compensation so note density changes cannot pump or click.
- Bound feedback state at the source and reset invalid effect state deterministically.
- Add a final low-distortion safety ceiling only after source gain and feedback behavior are corrected; document latency and transfer behavior if look-ahead is used.
- Preserve parameter IDs and old-project state semantics. Any new host parameter must be append-only and migration-tested.

Gate: all stress cases remain finite, bounded, click-safe, and measurably below the chosen ceiling without unacceptable loudness pumping or timbre collapse.

### S3 — CPU and scheduling optimization

- Profile before optimizing. Separate synth, modulation/filter, effects, MIDI delivery, and WebView presentation cost.
- Remove redundant per-sample work where mathematically equivalent block/control-rate work is safe.
- Preserve wavetable spectral behavior and parameter smoothing while optimizing.
- Keep low-graphics/reduced-motion presentation independent of audio correctness.
- Define a conservative Release budget for the Intel Core i9 target, including a dense real-product composition rather than only a synthetic oscillator benchmark.

Gate: the worst supported automated case has documented callback headroom, zero callback allocations, no new locks, and no regression in deterministic rendered audio outside intentionally changed gain staging.

### S4 — Product diagnostics and recovery

- Present peak/overload, maximum voice pressure, MIDI faults, and non-finite containment in bounded diagnostics using understandable labels.
- Provide one-click Panic and a safe temporary-effects-bypass comparison for diagnosis; neither action may alter stored presets without explicit acceptance.
- Document host buffer-size guidance as troubleshooting, not as a substitute for fixing internal overload.

Gate: a producer can distinguish clipping-like overload from an overloaded audio callback without exposing project content or credentials.

### S5 — Release and listening gate

- Run clean UI, Debug, Release, complete CTest, pluginval strictness 5, installed/build parity, and independent installed-VST3 MIDI render.
- Run prolonged Standalone listening with generated composition start/stop/restart, held notes, page navigation, and effects changes.
- Run the dedicated FL Studio matrix at practical and small buffer sizes. Record the producer's actual result; do not convert automation into a human listening pass.

Gate: no reproduced crack, runaway distortion, non-finite output, stuck delivery, or unexplained deadline failure. Any remaining device/host limitation is explicit.

## Non-goals

- Do not reduce oscillator quality blindly or disable features globally.
- Do not solve overload solely by lowering the master default.
- Do not hard-clip every sample and call the issue fixed.
- Do not add logging, heap allocation, filesystem access, WebView work, or locks to the callback.
- Do not claim FL Studio or audible success without the producer listening.

## Producer input needed later

Implementation can begin from deterministic stress fixtures. For exact real-world reproduction, retain these details the next time the failure occurs: Standalone or FL Studio, audio buffer size, sample rate, selected preset, enabled effects, whether direct composition playback was active, and whether Panic immediately restored clean sound. No project file needs to be shared unless the producer chooses to do so.
