# Product-owner amendments

This file records instructions added by the product owner after the immutable version 1.0 DOCX baseline. These requirements add to the baseline and may not silently weaken its safety, legal, real-time, validation, or explicit-acceptance rules.

## 2026-08-20 — Guided AI sound walkthrough

Folk Park must offer an optional assistant that helps the producer reach a sound accurately without requiring them to understand every synth control.

- Entry modes: describe a sound directly, choose a guided walkthrough, or continue editing manually with the assistant off.
- The walkthrough asks a small adaptive sequence about musical role, timbre, articulation, movement, space, intensity, genre/context, and any reference description the producer chooses to provide.
- The interface shows progress and begins forming a response as sufficient intent becomes available. It must not pretend certainty when the request is ambiguous; it asks a targeted follow-up instead.
- Output is a structured `ParameterProposal`: catalogued parameter IDs, bounded values, current-to-proposed differences, a concise explanation, assumptions, and confidence/uncertainty where useful.
- The producer can audition A/B, refine conversationally, undo, reject, or explicitly accept. No proposal auto-commits and no AI path writes directly to the audio thread or DAW.
- Manual controls remain fully functional. Offline deterministic assistance remains available without an account or API key; any remote provider is optional and follows the credential/privacy gate.
- Later composition assistance may use the same conversational pattern for samples, chords, melodies, bass, arpeggios, MIDI, and sound generation, while preserving deterministic validated delivery and explicit producer action.

Planned milestone placement: define schemas alongside M3 composition models, build the production interaction in M4, add reversible preview/history support in M6, and implement the offline text/provider workflow in M7.

## 2026-08-20 — Live wavetable and spectrum visualization

Folk Park must make oscillator behavior visually understandable with an original visualization system comparable in usefulness—not appearance or proprietary implementation—to modern wavetable synthesizers.

- Oscillators A and B each receive a readable live 2D waveform view and an optional 3D wavetable frame/morph surface.
- The view shows current table position and bounded modulation movement; a spectrum view helps reveal harmonic changes and alias control.
- Imported wavetables use the same reviewed visualization after conversion, without exposing unsafe or unbounded source data to the WebView.
- Rendering uses bounded native snapshots, never mutable audio-thread objects. It pauses or reduces work when hidden and has reduced-motion and low-graphics fallbacks.
- The Silicon Dreams presentation, geometry, colors, interaction, assets, and code must remain original and must not copy Serum/Serum 2 trade dress or proprietary behavior.

Planned milestone placement: implement the production visualization and performance/accessibility gates in M4, then reuse the same isolated analysis surfaces for M5 effects and WAV preview where appropriate.

## 2026-08-20 — On-screen piano audition

Folk Park must include an on-screen piano so the producer can hear the current sound without first creating a DAW piano roll or attaching a MIDI controller.

- Support touch, mouse/pointer, and a documented computer-key mapping with visible active-note feedback.
- Present four playable octaves from C2 through B5. Keep every touch/mouse key available at normal width, allow compact-width horizontal scrolling, and provide explicit octave controls for the mapped computer-key zone.
- Held computer keys sustain one note-on. macOS key-repeat events and redundant releases stay silent and cannot retrigger the preview voice.
- Provide accessible note labels and keyboard focus behavior without stealing typing while a text/number/select control is active.
- UI note actions cross a bounded native queue and become MIDI only at the processor boundary. The WebView never calls synth/DSP code directly.
- Pointer cancel, window blur, visibility loss, editor close, Stop, and Panic must release preview notes. Queue overflow must fail safely and must not create a stuck note.
- Preview MIDI may also leave the plug-in MIDI output consistently with the current instrument contract; FL Studio shortcut/focus and routing behavior remain human tests.

Planned milestone placement: implement and prove the keyboard/no-stuck-note boundary in M4.
