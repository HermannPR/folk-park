# MIDI generation contract

M0 contains one deterministic proof clip only: middle C, MIDI channel 1, velocity 100, starting at tick 0 and ending at tick 960, with PPQ 960, 4/4 time signature, and 120 BPM tempo metadata. The same in-memory MIDI data is written, reopened, and checked for complete note-on/note-off lifecycle by `FolkParkM0Tests`.

The producer starts external drag explicitly from the native strip. The receiving application may copy but is not permitted to move the source file. The generated temporary file remains available for the duration of the drag; no audio-thread file access occurs.

M3 replaces this proof with shared `MusicIntent` and `GeneratedClip` models for chords, melody, bass, and arpeggio generation. Those models must remain deterministic for seed + generator version, bound ticks/pitches/velocities/event counts, and provide explicit note-offs before all delivery paths consume them.
