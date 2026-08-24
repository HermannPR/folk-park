# Rhythm Lab product plan

Status: proposed post-0.1 milestone; implementation has not started.

## Product promise

Rhythm Lab should turn `folk park` from a pitched-note composition assistant into a complete rhythm partner. A producer can generate a coordinated drum candidate, audition real drum sounds, lock the parts that work, replace only selected lanes, edit the performance, and export either MIDI or rendered audio without losing the deterministic candidate/accepted boundary.

The drum system must produce drum timbres. It must not route General MIDI drum pitches through the wavetable synth and describe that result as a drum engine.

## Recommended product shape

### One rhythm, three layers

1. **Pattern engine** — deterministic kick, snare/clap, closed/open hat, percussion, cymbal, and texture lanes with genre-aware timing, velocity, probability, swing, fills, and humanization.
2. **Sound engine** — a hybrid voice per lane: synthesized transient/body/noise plus an optional bounded sample layer. Synthesized operation remains complete when no sample pack is installed.
3. **Break lab** — beat-synchronous loop slicing with transient markers, reorder, repeat, reverse, pitch, gate, probability, and controlled breakcore density. Distribution uses only original or clearly licensed audio; producers can import their own loops through a strict reviewed boundary.

### Producer workflow

- Generate drums together with chords, melody, bass, and arpeggio, or generate drums alone.
- Choose a genre profile and a kit independently. A techno rhythm may be auditioned with an acoustic, synthetic, or hybrid kit.
- See every lane in a dedicated step/piano-roll hybrid editor with meaningful drum names and colors.
- Solo, mute, lock, replace, and regenerate individual lanes without disturbing locked material.
- Select an individual sound for each lane, then edit synthesis/sample controls in a focused inspector.
- Ask for controlled variations such as “busier hats,” “half-time snare,” “more human,” or “breakcore fill in bar four.”
- Compare original A and candidate B, then explicitly accept the complete rhythm or selected lanes.
- Export standard drum MIDI plus isolated kick/snare/hat/percussion/break stems and a full rendered loop.

## Initial genre profiles

Recommended first set:

- House / techno
- Hip-hop / trap
- Funk / breakbeat
- Ambient / experimental
- Breakcore

Rock, Latin, drum and bass, garage, and additional regional styles should follow through data-driven profile definitions rather than parallel engine implementations.

## Amen-style break handling

The feature should be called a break slicer, not an “Amen sample library.” The original recording must not be bundled unless its distribution rights are explicitly documented. The safe default is an original break recorded or synthesized for `folk park`, plus user import and optional approved CC0/CC-BY packs whose attribution and hashes are retained.

The breakcore profile can still reproduce the musical technique: 16th/32nd-note slicing, retriggers, alternating direction, pitch jumps, ghost hits, bar-end fills, and seed-stable controlled chaos.

## Contracts and safety boundaries

- Extend composition schemas through a new version; never reinterpret existing M3 clips.
- Keep candidate and accepted rhythm bundles separate.
- Store stable kit/sample identities, not unrestricted paths.
- Decode, analyze, hash, and prepare samples away from the audio callback.
- Publish complete immutable drum-kit snapshots at block boundaries.
- Preallocate voices, slicer state, envelopes, filters, and per-lane routing.
- Cap lane count, polyphony, sample duration, transient count, event count, render duration, and import size.
- A missing or invalid sample must leave the last valid accepted kit audible and request explicit relinking.
- Jarvis may propose typed rhythm changes but cannot silently accept, download, execute, or overwrite them.

## Delivery stages

### R1 — Drum contracts and audible vertical slice

Freeze versioned rhythm/kit schemas and stable parameters. Deliver kick, snare, closed hat, a four-lane deterministic pattern, lane audition, explicit acceptance, and MIDI export. Gate on finite output, determinism, state round trip, zero callback allocations, and no stuck voices.

### R2 — Hybrid kit and editor

Add clap, open hat, percussion, cymbal, texture, sample layers, kit browser, lane locking, per-lane regeneration, solo/mute, velocity/probability, and audio-stem render. Gate on sample failure isolation, exact asset recovery, editor/state parity, and render reopen verification.

### R3 — Genre profiles and Jarvis rhythm guidance

Add the initial five genre profiles and bounded natural-language rhythm proposals. Gate on seed reproducibility, musical bounds, explanation/acceptance, stale-response rejection, and profile-independent engine behavior.

### R4 — Break Lab

Add reviewed loop import, transient slicing, break arrangement, breakcore controls, original/licensed factory content, and project recovery. Gate on rights inventory, bounded analysis, tempo parity, deterministic slicing, missing-asset recovery, and FL Studio human tests.

## Inputs useful from the producer

Implementation can begin with the defaults above. Before factory-content selection, the producer should provide:

1. The three most important genres for the first usable release.
2. A preference for synthesized, sampled, or hybrid drums; **hybrid** is the recommended default.
3. Any personally owned loops or one-shots intended for testing, along with whether they may remain private test material or can legally ship.
4. A few reference tracks for feel only. References guide timing and sound goals; they are not copied or bundled.

## Explicit non-goals for the first stage

- No unreviewed online sample downloading.
- No bundled copyrighted break recording without documented rights.
- No arbitrary agent writes to FL Studio or the filesystem.
- No replacement of the existing pitched composition engine.
- No claim that generated rhythm is musically approved until the producer listens.
