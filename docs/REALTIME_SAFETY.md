# Real-time safety contract

## Audio callback prohibition

Every function reachable from `processBlock` must avoid allocation/deallocation, locks or waits, file/database/network/clipboard/shell/WebView access, formatted logging, JSON/XML parsing, exceptions crossing the callback, unbounded loops, recursion, UI calls, and destruction that might block.

## Required patterns

- Allocate voices, buffers, tables, delay lines, routes, and temporary workspaces before playback.
- Read host parameters atomically or copy them once at block boundaries.
- Smooth audible continuous values and bound all feedback paths.
- Publish only validated immutable tables/clips/state snapshots through bounded queues or atomic handoff.
- Convert audio-thread errors to fixed counters/flags and format them later.
- Reset deterministically across sample-rate/block-size changes and transport/panic events.
- Sanitize denormals and reject/replace NaN or infinity before output.

## Review gate

Every DSP change must document allocations and ownership, add a finite-output/reset test, and be reviewed from `processBlock` down. Allocation instrumentation is a planned M1/M2 debug aid; until it exists, the limitation must remain visible in `docs/PROGRESS.md`.
