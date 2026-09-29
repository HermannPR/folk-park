# ADR-0006: Ordered effects and isolated offline preview

- Status: Accepted for M5 implementation
- Date: 2026-08-21

## Context

M5 adds a production effect chain and WAV preview to an instrument whose live synth already has strict real-time ownership. Host automation, older project recall, bypass changes, tempo synchronization, and rendering must not introduce allocations, clicks, invalid output, or a second mutable owner of live voices.

## Decision

- The fixed serial order is Distortion, Chorus, tempo-synced Delay, Reverb, Compressor, then three-band Parametric EQ. The order is not user-rearrangeable in Release 0.1.
- Every processor has an append-only host parameter surface, independent bypass, bounded settings, and an internal wet/dry or equivalent blend. New parameters default to bypassed to preserve the pre-M5 sound and safely load state written before M5.
- A preallocated dry copy and a 10 ms per-stage crossfade make bypass transitions click-safe. Delay and modulation memory, reverb state, compressor envelope, filters, and scratch storage are allocated or initialized in `prepare`, never in `process`.
- Tempo-synced delay reads a finite, bounded block-level tempo snapshot. Release 0.1 supports 1 bar, 1/2, 1/4, 1/8, and 1/16 divisions with feedback capped below unity.
- The effect boundary substitutes catalog defaults for non-finite parameter values and bounds non-finite or runaway samples before they leave the chain.
- WAV preview will render only an explicitly accepted composition. It will copy an immutable synth/effect/composition snapshot into a separate synth and effect-chain instance on a non-audio thread. It will never seek, reset, reuse, or mutate live voices.
- Preview output is a bounded PCM WAV with an explicit sample rate and deterministic length. Saving or replacing an existing destination requires an explicit user action; rendering a candidate never silently overwrites a file.

## Consequences

- State and host parameter tests must cover the 29 appended IDs, effect bypass defaults, and round trips.
- DSP tests must cover each stage, serial-chain finite output, reset determinism, supported rates and block sizes, tempo timing, bypass continuity, and zero measured callback allocations.
- Renderer tests must cover WAV header/rate/length and prove that preview work does not change the live processor's active voices or subsequent audio.
- FL Studio loading, automation, host tempo changes, WAV import/playback, and DAW scan remain explicit human validation rows until they are actually run.
