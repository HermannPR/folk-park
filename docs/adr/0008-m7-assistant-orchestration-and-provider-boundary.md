# ADR-0008: Offline-first assistant orchestration and provider boundary

- Status: Accepted for M7 implementation
- Date: 2026-08-23

## Context

M7 must deliver two related but distinct producer workflows:

1. Jarvis composition text maps a bounded prompt into a validated `MusicIntent`, then uses the existing deterministic composition/candidate/Accept/delivery pipeline.
2. Guided sound design collects a bounded `SoundIntent`, produces an explained `ParameterProposal`, supports reversible original/proposal A/B audition, and changes the current sound only after explicit acceptance.

Both must work offline. A future remote provider is optional, untrusted, and cannot receive or mutate arbitrary project state. The M3 proposal schema was also frozen before M5 added 29 effect parameters, so it covers 73 of the current 102 host parameters.

## Decision

### One typed orchestration boundary

`AssistantRequest` and `AssistantResponse` are versioned native values with exactly one target: composition or sound. Every request has a UUID, a 1,024-character text limit, a declared processing origin, and exactly one validated typed context. Every response must match the request UUID/target/origin and contain exactly one validated `MusicIntent` or `ParameterProposal`.

Offline, mock-provider, and any future remote-provider path share this boundary. Natural-language text is presentation only. It cannot execute code, emit arbitrary MIDI, address the filesystem, or change DSP.

Remote origin requires explicit producer consent on each submitted request. Provider implementations expose an asynchronous non-audio-thread interface with cancellation and at-most-once completion. Stale or mismatched responses fail validation before session mutation.

### Versioned catalog validation

The original parameter-proposal schema is retained unchanged as `parameter-proposal-v1.schema.json`. Schema v1 remains limited to the 73 pre-effects IDs. The current `parameter-proposal.schema.json` becomes v2 and permits at most all 102 current host parameters.

Both versions resolve every parameter ID against `src/common/ParameterIds.h`; unknown IDs are rejected. V2 additionally requires a non-empty reason for every change and non-empty assumptions. Normalized values remain finite in `[0, 1]`, IDs remain unique, and `requiresExplicitAcceptance` remains the constant `true`.

### Reversible preview and acceptance

The sound session will own three separate non-audio values: immutable original normalized parameters captured when the proposal is prepared, a validated proposed snapshot, and the accepted current host state. A/B selection may publish either review snapshot through normal message-thread host parameter gestures. Reject/cancel restores the original; Accept explicitly commits the proposal. No provider callback, streamed text, UI render, or session restore can call audio code directly.

Composition text produces a candidate `MusicIntent` and follows the existing candidate-versus-accepted composition boundary. “Generate & Wait” never inserts material into FL Studio automatically.

### Offline-first provider and credentials

M7 first implements deterministic offline parsers plus a mock provider. `AssistantProvider` is the only provider interface. The product-owner decision for a real remote adapter remains open; no real adapter or network dependency is added in this checkpoint.

A real adapter may be added only after a macOS Keychain-backed credential implementation, explicit privacy disclosure/opt-in, bounded timeout/cancel/retry behavior, and product-owner selection. Credentials never enter UI JavaScript, presets, host state, logs, history, diagnostics, or Git.

If OpenAI is selected later, its adapter should request JSON Schema Structured Outputs, following the [official OpenAI Structured Outputs guidance](https://developers.openai.com/api/docs/guides/structured-outputs), but native Folk Park validation remains mandatory even when the provider claims schema conformance.

## Consequences and gates

- No new third-party dependency is needed for the M7 foundation.
- Tests must cover unsupported versions/origins/targets, prompt bounds/control characters, missing remote consent, mixed request variants, stale response IDs, unknown/duplicate parameters, v1/v2 catalog boundaries, non-finite values, and explicit acceptance.
- Offline text and guided sound generation must remain fully usable with no key, network, account, database, or provider.
- Provider failure, cancellation, timeout, UI close, and project reload cannot stop existing audio or silently accept a result.
- FL Studio Jarvis focus, project reopen, automation, A/B listening, and provider/offline behavior remain human tests until explicitly run.
