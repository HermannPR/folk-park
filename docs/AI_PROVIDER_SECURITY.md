# AI provider security

Release 0.1 is offline-first. Deterministic generation accepts a validated `MusicIntent`; a provider is optional and may only propose that schema.

The guided sound walkthrough produces a separate bounded `SoundIntent` and `ParameterProposal`. It may ask adaptive questions and explain why values were proposed, but only catalogued parameter IDs and valid values can cross the native boundary. Preview, compare, undo, and explicit producer acceptance are mandatory; a streamed natural-language response never changes DSP state by itself.

- No provider may execute code, shell commands, HTML, or JavaScript.
- Responses are untrusted, size-limited, parsed off the audio thread, validated, and clamped.
- Audio, project names, paths, history, presets, and prompts are not transmitted without specific UI disclosure and consent.
- Credentials never enter Git, source, `.env` examples, logs, presets, DAW state, MIDI, diagnostics, or history exports.
- A real adapter requires a macOS Keychain abstraction, cancel/timeout/retry/offline behavior, and explicit opt-in.
- Producer acceptance remains mandatory after any local or remote proposal.

## M7 concrete boundary

- Assistant prompts are text-only and capped at 1,024 characters before dispatch.
- Composition and sound requests are distinct tagged variants. Mixing their typed contexts is rejected.
- Every response must match the active request UUID, target, and processing origin; late/stale responses are discarded.
- Remote processing requires explicit consent on the submitted request. Consent is not persisted as blanket authorization.
- The original proposal v1 schema remains limited to its 73 pre-effects parameters. Proposal v2 covers the current 102-parameter catalog, and every ID is resolved against the authoritative C++ list.
- Offline and mock-provider implementations never read credentials. A real provider remains an open product-owner decision until the Keychain-backed implementation and privacy UX exist.
- Provider schema conformance is defense in depth only. Folk Park always repeats native size, version, UUID, enum, catalog, finiteness, uniqueness, and explicit-acceptance validation.
