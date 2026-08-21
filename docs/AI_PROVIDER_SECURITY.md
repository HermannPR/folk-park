# AI provider security

Release 0.1 is offline-first. Deterministic generation accepts a validated `MusicIntent`; a provider is optional and may only propose that schema.

The guided sound walkthrough produces a separate bounded `SoundIntent` and `ParameterProposal`. It may ask adaptive questions and explain why values were proposed, but only catalogued parameter IDs and valid values can cross the native boundary. Preview, compare, undo, and explicit producer acceptance are mandatory; a streamed natural-language response never changes DSP state by itself.

- No provider may execute code, shell commands, HTML, or JavaScript.
- Responses are untrusted, size-limited, parsed off the audio thread, validated, and clamped.
- Audio, project names, paths, history, presets, and prompts are not transmitted without specific UI disclosure and consent.
- Credentials never enter Git, source, `.env` examples, logs, presets, DAW state, MIDI, diagnostics, or history exports.
- A real adapter requires a macOS Keychain abstraction, cancel/timeout/retry/offline behavior, and explicit opt-in.
- Producer acceptance remains mandatory after any local or remote proposal.
