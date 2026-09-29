# ADR-0009: Bounded diagnostics and evidence-gated release hardening

Status: accepted for M8 implementation

## Context

Release 0.1 needs actionable support information and reproducible release evidence without weakening the audio-thread, privacy, state, or host-truthfulness boundaries established through M7. Existing UI/status messages are workflow-facing and may contain filenames or operation details; copying them wholesale would create an unnecessary privacy risk. Automatic logging/upload would also contradict the offline-first product.

## Decision

- Add a small typed diagnostics model with fixed enums/counters and deterministic bounded text encoding. It contains configuration and sanitized codes, never arbitrary workflow error strings.
- Keep audio-path observations as relaxed lock-free counters owned by the processor. The callback performs no formatting, allocation, clipboard, file, UI, or network work.
- Build the human-readable preview on the message thread from one immutable snapshot. Cap the complete UTF-8 text at 4 KiB and reject unsupported/unbounded input before publication.
- Expose separate native `previewDiagnostics` and `copyPreviewedDiagnostics` operations. Copy requires the exact opaque ID of the most recently displayed preview in that editor; reopening or refreshing invalidates older IDs.
- Use the macOS system clipboard only after explicit producer action. Diagnostics remain local and are not persisted or transmitted.
- Treat the M7 clean UI/Debug/Release/pluginval/installed-artifact/security gate as the M8 regression floor. New M8 evidence adds long-run, diagnostics, install/repair/uninstall, benchmark, license/asset, and limitations proof.
- Label the result `private engineering candidate — FL Studio human validation pending` unless the complete target-host matrix has real producer results. Distribution remains blocked on explicit owner decisions.

## Consequences

- Support data is reviewable and useful without copying prompts, paths, project identity, credentials, or audio.
- Fault visibility adds a few lock-free counter increments to exceptional real-time paths but no locks, owned allocation, or formatting.
- A frontend cannot request an unseen arbitrary diagnostic payload to be copied; native state enforces the preview boundary.
- Host version may honestly remain `unavailable` when JUCE/the host does not supply it.
- Signing, notarization, licensing, remote-provider selection, and human FL results are deliberately outside automated completion.
