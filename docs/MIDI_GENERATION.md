# MIDI generation contract

M0 contains one deterministic proof clip only: middle C, MIDI channel 1, velocity 100, starting at tick 0 and ending at tick 960, with PPQ 960, 4/4 time signature, and 120 BPM tempo metadata. The same in-memory MIDI data is written, reopened, and checked for complete note-on/note-off lifecycle by `FolkParkM0Tests`.

The producer starts external drag explicitly from the native strip. The receiving application may copy but is not permitted to move the source file. The generated temporary file remains available for the duration of the drag; no audio-thread file access occurs.

M3 replaces this proof with shared `MusicIntent` and `GeneratedClip` models for chords, melody, bass, and arpeggio generation. The implemented generators are deterministic for seed plus generator version, bound ticks/pitches/velocities/event counts, preserve lineage for related variations, and provide explicit note-offs before all delivery paths consume them.

## M3 delivery boundary

- Generation creates a candidate. Drag, export, and direct routing remain unavailable until explicit producer acceptance.
- Chords, melody, bass, and arpeggio share one harmonic plan. `More Like This` retains context and parent IDs; `Surprise Me` creates another bounded candidate.
- Piano-roll preview is a lossy projection only. It cannot become the source of exported notes.
- SMF export creates one metadata track and one track per requested part, rescales through one canonical low-PPQ quantizer, writes tempo/meter and explicit note-offs, reopens the file, and compares every source note.
- The native drag strip owns its temporary file for the editor session. Save export uses the same verified bytes and warns before overwriting.
- Direct MIDI publishes a complete fixed schedule off audio and activates it at the next block. Stop emits note-offs for tracked notes. The same scheduled messages drive the internal synth and leave through the plug-in MIDI output.
- Accepted clips are session-memory state until M6. Direct playback currently uses clip tempo and is not yet synchronized to host transport start/reposition.

FL Studio drag and Wrapper output-port routing are still human-required checks in [FL_STUDIO_TEST_MATRIX.md](FL_STUDIO_TEST_MATRIX.md); automated parity is not reported as a host pass.
