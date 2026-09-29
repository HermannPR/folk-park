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

Every DSP change must document allocations and ownership, add a finite-output/reset test, and be reviewed from `processBlock` down.

## M2 measured implementation

- Voices, oscillator phases, envelope/filter/LFO/noise state, modulation destinations, and unison lane fades are fixed-capacity members.
- Each oscillator owns a three-slot fixed wavetable exchange. Conversion and FFT mip construction occur on a single non-audio worker; only a validated immutable bank is copied into a free slot.
- A pending bank activates at a block boundary and uses a fixed 128-sample crossfade. Slot retirement changes atomic state only; it does not destroy heap objects from audio.
- Modulation uses two fixed 32-route snapshots. Validation/copy occurs outside audio and complete snapshots activate at a block boundary.
- Audible continuous oscillator and filter values use fixed per-sample smoothing; unison count uses per-lane weights rather than allocation or voice-container changes.
- `tests/RealtimeTests.cpp` overrides allocation entry points and measures 32 process blocks after setup. The test includes pending wavetable and modulation activation plus table crossfade and currently reports zero allocations in Debug and Release.

## M7 assistant boundary

- Prompt parsing, guided questions, provider completion, proposal validation, A/B serialization, and APVTS comparison gestures are non-audio operations.
- `processBlock` does not reference an assistant session, prompt, provider, project payload, mutex, or dynamic proposal collection; it continues reading the existing atomic host parameters.
- A/B gestures use the same host parameter path as manual UI changes. No new parameter, queue, allocation, lock, or branch was added to the callback.
- The complete Debug gate still passes `FolkParkRealtimeAllocationTests` after processor A/B and project-recovery integration.
