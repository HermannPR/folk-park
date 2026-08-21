# ADR-0004: Pure deterministic composition and shared MIDI delivery

- Status: Accepted for M3 implementation
- Date: 2026-08-20

## Context

M3 must generate chords, melody, bass, and arpeggios offline without a provider, expose visual preview data, and deliver the same accepted notes through SMF drag/export and bounded direct MIDI. Generation must remain reproducible and cannot place files, UI objects, mutable vectors, or model/provider work on the audio callback.

## Decision

- `MusicIntent` is the only composition input. It has typed enums, bounded macros, time/key/scale context, explicit requested parts, arpeggiator settings, note/polyphony/event constraints, and a 32-bit seed. Unknown enum values and structural contradictions are rejected; numeric macro candidates may be clamped only through an explicit normalization function.
- `GeneratedClip` is the only musical output. It owns sorted host-independent note events, optional chord labels, scale/time context, seed and generator version, deterministic ID, parent lineage, and creation metadata. A clip validates before preview or delivery.
- Version 1 uses PPQ 960 internally. All generators are pure bounded functions of normalized intent, generator version, part, and variation seed. They share one generated harmonic plan so chord-aware parts agree.
- Chords use weighted functional transitions, deterministic cadence, triad/seventh selection, inversions, and a bounded voice-leading cost. Melody uses phrase contour, chord-tone weighting, scale passing tones, motif repetition/variation, rests, and leap limits. Bass is monophonic and uses root/octave/approach choices. Arpeggios render up/down/up-down/random-seeded/chord-order clips; no real-time latch engine is added in M3.
- Humanization is applied after musical generation with a separate deterministic stream. Timing and velocity offsets are bounded, then events are sorted and monophonic parts are repaired so no duration becomes non-positive or overlaps the next onset.
- “More Like This” derives a new seed while preserving key, scale, time, requested parts, macro snapshot, and parent clip IDs. “Surprise Me” derives bounded macro/genre/emotion changes and a new seed but still returns a reviewable `MusicIntent`; neither path writes to the DAW.
- Piano-roll preview is a lossy presentation projection of validated clips. It is never the source for export.
- SMF export, temporary drag files, and direct output consume the same validated clips. Export uses tempo/time-signature metadata and explicit note-offs, and its validator reopens the file and compares musical events.
- Direct MIDI converts an accepted bundle to two fixed schedule slots off audio. The callback activates a complete schedule at a block boundary, emits at most 128 generated events per block, and uses the 2048-byte MIDI capacity preallocated by JUCE's VST3 wrapper. Direct Route is explicit and adds the accepted schedule to the processor MIDI buffer so the internal synth and host output receive it; Stop emits bounded note-offs for tracked generated notes.
- Candidate and accepted bundles are session-memory coordinator state protected only on non-audio threads. M6 history/persistence will retain accepted clips across restart.

## Assistant schema foundation

M3 also defines strict version 1 schemas and typed validation surfaces for `SoundIntent` and `ParameterProposal`. They do not implement an assistant. M7 must additionally resolve every proposed parameter ID against the authoritative C++ parameter catalog and must preserve preview/A-B/explicit acceptance.

## Deferred work

- Offline WAV preview through an isolated synth context is M5 by contract.
- The complete responsive COMPOSE view, piano-roll interaction, accessibility, and bridge recovery are M4. M3 adds a condensed engineering view and native drag strip.
- Real-time live-input arpeggiation is deferred until dedicated note-lifecycle and host-transport tests exist.
- Accepted composition persistence, lineage history, cleanup, and restart recovery are M6.

## Consequences and gates

- Property tests cover keys/scales, chord construction/inversions, determinism, ranges, ordering, non-overlap, event caps, humanization, lineage, and bounded variation.
- Every exported fixture must reopen and match its source events at multiple PPQ/time/tempo/range settings.
- Direct scheduling must prove correct block offsets, cancellation note-offs, fixed publication, and zero measured audio allocations with a pre-sized host buffer.
- FL Studio drag and wrapper routing remain explicit human tests until run on the target host.
