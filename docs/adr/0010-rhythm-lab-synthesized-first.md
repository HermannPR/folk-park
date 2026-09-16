# ADR-0010: Synthesized-first Rhythm Lab contracts

- Status: Accepted for R1 implementation
- Date: 2026-08-24

## Context

The producer wants drum generation for indie/rock, Eurodance, techno, funk, and jazz, but owns no sample library intended for this product. Drum notes routed through the existing pitched wavetable sound would not satisfy the requested audible workflow. Downloading or bundling unreviewed one-shots and the original Amen recording would create provenance, recovery, and distribution risk.

The verified 0.1 composition, explicit candidate/accepted boundary, host state, and real-time guarantees must remain compatible while this post-0.1 feature grows.

## Decision

- R1 is synthesized-first. It ships an authored `synth_core_v1` kit with kick, snare, closed hat, open hat, and percussion voices and requires no sample, account, network, or external library.
- The five initial genre profiles are `indie_rock`, `eurodance`, `techno`, `funk`, and `jazz`. Profiles provide bounded timing/velocity/probability weights; they never encode or claim copyrighted performances.
- `RhythmIntent`, `DrumPattern`, and `SynthDrumKit` are separate versioned contracts. Existing M3 `MusicIntent` and `GeneratedClip` schemas remain unchanged.
- Drum patterns keep stable lane names and one deterministic PPQ timeline. Candidate generation and explicit acceptance remain separate session states before MIDI/audio delivery.
- The synthesized kit owns bounded normalized/timed parameters and remains complete without samples. A future optional sample layer must be added through a new compatible contract version and content-addressed reviewed assets; R1 contains no dormant path or placeholder download.
- The audio engine preallocates all voices/state in `prepare`, performs no allocation, lock, filesystem, JSON, database, network, WebView, or formatted logging in render, and replaces non-finite output with silence.
- Break slicing is deferred until an original, clearly licensed, or producer-imported loop boundary exists. The original Amen recording is not bundled by assumption.

## Consequences and gates

- Folk Park can create recognizable tunable drum sounds from code with a clean provenance trail.
- Sample realism is intentionally deferred; synthesized profiles should express genre through timing and synthesis rather than pretending to be recorded acoustic kits.
- Contract tests reject unknown kit IDs, duplicate lanes, unsupported genres, invalid timing, non-finite values, event overflow, and implicit compatibility changes.
- Engine gates require deterministic finite output, supported sample rates/block sizes, no stuck tails after bounded decay/reset, and zero measured callback allocations.
- FL Studio discovery, listening, timing feel, automation, project reopen, and rendered-stem workflow remain human-required until the producer runs them.
