# FL Studio manual test matrix

Status: **HUMAN RUN REQUIRED**. No case is passed merely because a build or native test succeeds.

Environment at M0 intake:

- Host: FL Studio 26.1.4.5356
- macOS: 15.7.9 (24G830)
- Architecture: x86_64-capable Intel host
- Tester: Hermann Pauwells
- M4 VST3 binary SHA-256: `58169e6dcfda3ee50298e6994ac6b9441cbc5c887d2b5fa0e0bae26932848188`

| Case | Expected result | Actual | Evidence |
| --- | --- | --- | --- |
| Discovery/rescan | folk park appears as a VST3 generator | HUMAN RUN REQUIRED | Pending |
| Editor lifecycle | Open, resize, close, and reopen safely | HUMAN RUN REQUIRED | Pending |
| MIDI/audio | Piano roll triggers finite stereo output; stop/panic leave no notes | HUMAN RUN REQUIRED | Pending |
| M4 four-octave pointer piano | Every C2–B5 key auditions once; drag/cancel/focus loss and close release cleanly | HUMAN RUN REQUIRED | Automated queue/window evidence is not an FL pass |
| M4 computer piano | A–P, sharps, Oct−/Oct+, held macOS repeat, FL shortcut ownership, and key release behave predictably without retrigger spam | HUMAN RUN REQUIRED | Standalone injected-repeat proof passed; FL focus pending |
| M4 compact layout | Resize to the minimum, scroll to all four piano octaves and every control, then restore size without clipped or trapped UI | HUMAN RUN REQUIRED | Standalone visual inspection passed; FL pending |
| M4 wavetable visuals | OSC A/B show their actual wave/frame position/spectrum and follow position automation/import; Low Graphics/Reduced Motion remain usable | HUMAN RUN REQUIRED | Bounded UI snapshot and animation tests passed; FL/GPU pending |
| M2 oscillators | A/B position, tuning, level/pan, and 1↔8 unison are audible and free of objectionable automation clicks | HUMAN RUN REQUIRED | Pending |
| M2 sources/filter | Sub/noise, all filter modes, three envelopes, and four LFO shapes behave as labelled | HUMAN RUN REQUIRED | Pending |
| Modulation | Add/edit/remove/apply/discard multiple reviewed routes and restore the complete matrix after reopen | HUMAN RUN REQUIRED | Native 32-route publication passed; FL pending |
| Valid WAV import | Review a user-owned WAV for A and B, inspect metadata, Confirm, and audition the click-safe table swap | HUMAN RUN REQUIRED | Pending |
| Import cancel/failure | Cancel and malformed/silent WAV failures leave the current oscillator unchanged and audio alive | HUMAN RUN REQUIRED | Pending |
| Transport | Tempo, start/stop, PPQ, and unavailable fields are safe | HUMAN RUN REQUIRED | Pending |
| Automation | Representative stable parameters record/replay | HUMAN RUN REQUIRED | Pending |
| Project state | Save, close FL Studio, reopen, and restore state | HUMAN RUN REQUIRED | Pending |
| Imported-table reopen limitation | Parameters/routes restore safely; imported table falls back to built-in because M6 asset persistence is not implemented | HUMAN RUN REQUIRED | Pending |
| MIDI drag | Generated `.mid` drags into a supported FL Studio destination | HUMAN RUN REQUIRED | Pending |
| MIDI routing | Wrapper output port routes to a second instrument | HUMAN RUN REQUIRED | Pending |
| Candidate note editing | Edit pitch/start/duration/velocity, verify accepted MIDI remains unchanged, accept again, then drag/direct-route the edit | HUMAN RUN REQUIRED | Native transactional edit tests passed; FL pending |
| Immediate Undo/Redo | Move a host-aware UI control, immediately Undo, then Redo; host automation/state remain coherent | HUMAN RUN REQUIRED | Native APVTS timing regression passed; FL pending |
| Preview WAV | Isolated render/import/play with correct length/rate | NOT IMPLEMENTED (M5) | Not claimable in M4 |
| UI focus/fallback | Shortcuts, low graphics, and WebView failure are safe | HUMAN RUN REQUIRED | Pending |
| Offline render | Matches real-time within documented tolerance | HUMAN RUN REQUIRED | Pending |
| Failure modes | Invalid state/provider/database/UI failure does not stop audio | HUMAN RUN REQUIRED | Pending |
