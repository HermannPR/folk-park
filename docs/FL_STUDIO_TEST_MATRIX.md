# FL Studio manual test matrix

Status: **HUMAN RUN REQUIRED**. No case is passed merely because a build or native test succeeds.

Environment at M0 intake:

- Host: FL Studio 26.1.4.5356
- macOS: 15.7.9 (24G830)
- Architecture: x86_64-capable Intel host
- Tester: Hermann Pauwells
- Installed M7 VST3 binary SHA-256: `b17c88bab2c1356c7b01980b96f918a28acbdd337f7ee2e437f9c63a7d7119ca`

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
| M6 imported-table project reopen | Save a project using imported A/B tables, close FL Studio, reopen, and confirm the exact tables and sound return from content-addressed storage | HUMAN RUN REQUIRED | Native editor-independent project-state recovery passed; FL pending |
| M6 preset Save As/load | Create two presets with distinct IDs, require explicit overwrite, load each, and verify parameters/routes/effects/tables | HUMAN RUN REQUIRED | Native transactional Save As/load coverage passed; FL pending |
| M6 missing-asset recovery | Remove an isolated test asset, reopen safely without partial mutation, reject a wrong WAV, then relink the exact user-selected WAV | HUMAN RUN REQUIRED | Native hash/size rejection and exact relink transaction passed; FL chooser/listening pending |
| M6 accepted composition/history reopen | Reopen the project, verify accepted MIDI remains deliverable, search/compare/recall history, and test recoverable trash | HUMAN RUN REQUIRED | Native project-state and SQLite restart/recall coverage passed; FL pending |
| M6 database unavailable | Make only the history database unavailable and verify acceptance, presets, existing audio, and project state continue | HUMAN RUN REQUIRED | Native failure-isolation test passed; FL pending |
| M7 offline Jarvis sound | Describe a sound, inspect every explanation/current→new value, audition A/B, reject to exact A, then create and explicitly accept a second proposal | HUMAN RUN REQUIRED | Release Standalone A/B/reject interaction passed; FL listening/host state pending |
| M7 guided walkthrough | Answer role/timbre/articulation/movement/space/intensity/genre in focused steps, verify no more than two questions at once, and create a reviewable proposal | HUMAN RUN REQUIRED | Release Standalone advanced through the first two guided pairs; FL pending |
| M7 assistant project reopen | Save with active A/B on A, repeat on B, close FL Studio, reopen, verify the correct audible side, then reject/accept safely | HUMAN RUN REQUIRED | Native editor-independent v2 recovery passed; FL pending |
| M7 composition text | Ask Jarvis for chords/melody/bass/arp, verify only a candidate appears, edit it, explicitly accept, then drag/direct-route it | HUMAN RUN REQUIRED | Native/UI candidate-only boundary passed; FL delivery pending |
| M7 provider/offline failure | Confirm Settings shows offline/no provider/no credential, disconnect networking, reopen UI, and verify Jarvis/manual audio continue without interruption | HUMAN RUN REQUIRED | Offline source/security/Standalone gates passed; FL failure/listening pending |
| MIDI drag | Generated `.mid` drags into a supported FL Studio destination | HUMAN RUN REQUIRED | Pending |
| MIDI routing | Wrapper output port routes to a second instrument | HUMAN RUN REQUIRED | Pending |
| Candidate note editing | Edit pitch/start/duration/velocity, verify accepted MIDI remains unchanged, accept again, then drag/direct-route the edit | HUMAN RUN REQUIRED | Native transactional edit tests passed; FL pending |
| Immediate Undo/Redo | Move a host-aware UI control, immediately Undo, then Redo; host automation/state remain coherent | HUMAN RUN REQUIRED | Native APVTS timing regression passed; FL pending |
| M5 ordered effects | Enable Distortion → Chorus → synced Delay → Reverb → Compressor → EQ; verify every control, order, independent bypass, and no objectionable clicks | HUMAN RUN REQUIRED | Automated isolated/serial DSP tests passed; listening in FL pending |
| M5 tempo sync | Change FL tempo while delay is active and verify 1 bar through 1/16 timing and safe transitions | HUMAN RUN REQUIRED | Exact 120/240 BPM sample timing passed natively; FL transport pending |
| M5 effect automation/state | Record representative FX controls and bypasses, save-close-reopen, and verify coherent recall | HUMAN RUN REQUIRED | All 102 parameters round-trip and pluginval automation passed; FL pending |
| Preview WAV | Accept a composition, render WAV, import it into FL, and verify stereo 24-bit/48 kHz playback, expected musical length plus tail, and current sound/effects | HUMAN RUN REQUIRED | Header/rate/length/non-silence and live-engine isolation passed natively; FL import/listening pending |
| UI focus/fallback | Shortcuts, low graphics, and WebView failure are safe | HUMAN RUN REQUIRED | Pending |
| Offline render | Matches real-time within documented tolerance | HUMAN RUN REQUIRED | Pending |
| Failure modes | Invalid state/provider/database/UI failure does not stop audio | HUMAN RUN REQUIRED | Pending |
