# FL Studio manual test matrix

Status: **HUMAN RUN REQUIRED**. No case is passed merely because a build or native test succeeds.

Environment at M0 intake:

- Host: FL Studio 26.1.4.5356
- macOS: 15.7.9 (24G830)
- Architecture: x86_64-capable Intel host
- Tester: Hermann Pauwells
- Build hash: not available until first verified build

| Case | Expected result | Actual | Evidence |
| --- | --- | --- | --- |
| Discovery/rescan | folk park appears as a VST3 generator | HUMAN RUN REQUIRED | Pending |
| Editor lifecycle | Open, resize, close, and reopen safely | HUMAN RUN REQUIRED | Pending |
| MIDI/audio | Piano roll triggers finite stereo output; stop/panic leave no notes | HUMAN RUN REQUIRED | Pending |
| Transport | Tempo, start/stop, PPQ, and unavailable fields are safe | HUMAN RUN REQUIRED | Pending |
| Automation | Representative stable parameters record/replay | HUMAN RUN REQUIRED | Pending |
| Project state | Save, close FL Studio, reopen, and restore state | HUMAN RUN REQUIRED | Pending |
| MIDI drag | Generated `.mid` drags into a supported FL Studio destination | HUMAN RUN REQUIRED | Pending |
| MIDI routing | Wrapper output port routes to a second instrument | HUMAN RUN REQUIRED | Pending |
| Preview WAV | Rendered file imports/plays with correct length/rate | HUMAN RUN REQUIRED | Pending |
| UI focus/fallback | Shortcuts, low graphics, and WebView failure are safe | HUMAN RUN REQUIRED | Pending |
| Offline render | Matches real-time within documented tolerance | HUMAN RUN REQUIRED | Pending |
| Failure modes | Invalid state/provider/database/UI failure does not stop audio | HUMAN RUN REQUIRED | Pending |
