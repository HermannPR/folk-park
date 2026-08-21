# ADR-0003: Fixed-capacity immutable wavetable and modulation exchange

- Status: Accepted for M2 implementation
- Date: 2026-08-20

## Context

M2 must load user-owned WAV material, build a band-limited representation, and replace oscillator tables without allocating, locking, parsing, or destroying ownership on the audio thread. It also needs a bounded modulation graph whose routes can be edited and restored without exposing mutable UI state to a voice render.

## Decision

### Wavetable representation

- Engine frame length is fixed at 2048 samples for Release 0.1.
- A bank contains 1-16 normalized frames and 11 mip levels per frame, plus explicit frame count and non-audio metadata.
- Frame and phase morphing use deterministic linear interpolation for the M2 baseline.
- Each mip level removes another octave of upper harmonics. The voice selects the least-filtered level whose highest retained harmonic remains below Nyquist for the current fundamental.
- Built-in tables and imported tables use the same representation and render path.

### Import and conversion

- Only RIFF/WAVE input is accepted in M2. Decode, validation, conversion, hashing, and mip construction happen on a non-audio worker.
- Input is bounded by channel count, decoded sample count, cycle length, and output frame count before large work proceeds.
- Channels are averaged to mono, non-finite samples are rejected, DC is removed, cycle endpoints receive deterministic linear continuity correction, and the result is peak-normalized only when a safe nonzero peak exists.
- Frames are linearly resampled to 2048 samples. If more than 16 source cycles exist, 16 cycles are selected at deterministic evenly spaced positions.
- The result includes provenance metadata and a fixed preview of the converted first frame. Import remains a producer-confirmed file selection; there is no network or automatic library mutation.

### Real-time publication

- Each oscillator owns three preallocated bank slots: current, previous/crossfading, and free or pending.
- A non-audio producer may write only a slot atomically marked free, then publish its index with release semantics. At most one pending bank is accepted.
- The audio callback consumes a pending index only at a block boundary and crossfades old to new for 128 samples. The old slot becomes free only after the crossfade finishes.
- The callback exchanges indices and reads immutable samples; it never allocates, deletes, locks, waits, copies a bank, or touches a file.

The fixed storage cost is approximately 1.38 MiB per slot, 4.13 MiB per oscillator exchange, and 8.25 MiB for oscillators A and B before object metadata. This is accepted for deterministic ownership on the target Mac.

### Modulation and filter

- A central registry defines supported sources, destinations, normalization, units, range, smoothing policy, and whether a destination is per-voice or global-derived.
- Release 0.1 accepts at most 32 routes. A complete candidate array is validated off-thread and published through fixed double-buffered snapshots. Unknown, recursive, non-finite, or out-of-range routes are rejected transactionally.
- Amp, filter, and auxiliary envelopes are per voice. LFO retrigger mode is per voice; free mode reads one engine-global phase. Velocity/note and supported MIDI controllers are per-voice/cache inputs.
- The multimode filter uses a stable topology-preserving state-variable form with low-pass, high-pass, and band-pass outputs. Cutoff, resonance, drive, key tracking, and envelope amount are bounded, with audible continuous targets smoothed inside each voice.

## Deferred choices

- User-drawn breakpoint LFO shapes remain deferred until sine, triangle, saw, and square shapes pass deterministic and lifecycle tests; the contract marks this shape optional.
- Oversampling is disabled until the M2 CPU and spectral baseline exists.
- Imported-bank embedding/content-addressed persistence is completed with the M6 preset/asset schema. M2 project state records parameters and validated routes but does not claim missing-asset recovery yet.

## Consequences and gates

- Import tests must cover empty, non-WAV, oversized, non-finite, invalid cycle, and valid mono/stereo fixtures.
- An allocation-counted render must consume and crossfade a pending bank with zero allocations in the callback.
- Route validation and transactional restore tests must cover the 32-route bound and unsupported endpoints.
- M2 evidence must record high-note spectral behavior and worst-case x86_64 render cost without describing the baseline oscillator as production quality.
