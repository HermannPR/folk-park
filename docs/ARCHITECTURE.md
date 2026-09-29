# Architecture

## Dependency direction

```text
FL Studio / host
       | audio, MIDI, transport, automation
       v
Plugin adapter ---- State coordinator ---- UI bridge ---- bundled WebView
       |                    |
       v                    v
Real-time synth       Presets and history
       ^
       | validated immutable snapshots / bounded queues
Composition and assistant services (non-real-time)
```

The plug-in processor adapts host buses, MIDI, transport, automation, and serialized state. The synth owns preallocated real-time voice/DSP state. Composition produces host-independent, validated `GeneratedClip` values. Assistant providers can only propose bounded `MusicIntent`, `SoundIntent`, and `ParameterProposal` values; they cannot execute code or write into a DAW. A proposal is validated against the parameter catalog, previewed in a reversible comparison state, and applied only after explicit producer acceptance. Persistence consumes snapshots outside the audio thread. The WebView is presentation only and must be recoverable from a complete C++ snapshot.

## M0 vertical shell

M0 proves the dependency seams before adding production DSP:

- A VST3 instrument and Standalone target declare stereo output, MIDI input, and MIDI output.
- A minimal processor clears or safely passes defined buffers and round-trips versioned state.
- A bundled WebView resource calls one native function and receives one native event, with a native fallback if unavailable.
- A deterministic tiny MIDI file can be written and reopened by a native test before a user explicitly drags it.
- Native tests run without FL Studio.

## M2 synthesis and ownership boundary

The M2 audio path is a fixed 16-voice engine. Each voice owns two oscillator phase arrays (eight lanes each), three envelope states, two filter states, four retriggerable LFO phases, deterministic noise state, and smoothing state. Global free-running LFO phases and immutable wavetable/modulation exchanges belong to the engine.

WAV decoding, validation, cycle conversion, preview construction, SHA-256 metadata, and FFT mip generation occur on one bounded worker. Conversion creates a candidate only. Producer confirmation publishes it to one of two three-slot exchanges; the audio thread activates a complete bank at a block boundary and reads current/previous pointers during a fixed crossfade. Imported audio is not read from disk by the callback.

The central modulation registry defines source/destination IDs, polarity, full scale, units, and smoothing expectations. Message/state code validates up to 32 routes and publishes one complete fixed snapshot. The callback never parses or partially mutates a matrix.

The WebView remains an adapter over host parameter attachments and explicit native functions. File choice, confirmation, cancellation, route editing, and status happen on non-audio/UI boundaries. M4 presents all 32 reviewed route slots while retaining the same transactional native publication.

## M3 composition and MIDI boundary

The pure composition engine normalizes a typed `MusicIntent`, derives one shared harmonic plan, and returns a validated bundle of host-independent `GeneratedClip` values. It has no processor, UI, file, network, or provider dependency. A non-real-time session coordinator owns separate candidate and accepted bundles; only an explicit Accept copies candidate state into the delivery boundary.

Piano-roll presentation, SMF export/drag, and direct output all read the same validated events. SMF work and direct schedule construction happen off audio. Direct MIDI uses a fixed double schedule and atomic block-boundary publication; the callback performs bounded message insertion and note tracking without locks or owned-vector work. Accepted clips remain session-only until M6 persistence.

## M4 presentation, visualization, and audition boundary

The M4 React tree is presentation only. JUCE Web parameter relays own host gestures, and one strict version 1 complete snapshot restores parameters, actual fixed A/B wavetable copies, modulation routes, composition state, version/status, and active voices. React validates the entire payload before publishing it, so malformed or future data cannot partially replace the last coherent view.

The visualizer receives at most 16 frames × 96 samples copied under the message-thread snapshot lock. It derives geometry and spectrum in JavaScript outside audio. Three.js renders at no more than 30 FPS; the 2D fallback renders at no more than 12 FPS; reduced-motion and hidden policies stop continuous work. ResizeObserver updates presentation dimensions without changing DSP ownership.

Touch/computer audition commands cross a fixed 64-command SPSC queue. The producer never calls the synth directly. The audio callback drains bounded note events, tracks native active notes, and makes duplicate note-on/off commands idempotent. Release-all is an atomic request used by focus loss, visibility loss, editor close, Panic, and overflow recovery. Host MIDI and preview MIDI meet only at the processor block boundary.

Composition-note editing remains non-real-time. Each edit copies the candidate, clamps and validates every affected value, stable-sorts events, validates the complete bundle, and then publishes transactionally. The accepted bundle is a separate immutable value until the producer explicitly accepts the edited candidate again.

## M5 effects and offline-render boundary

The real-time chain has one fixed serial order: Distortion, Chorus, tempo-synced Delay, Reverb, Compressor, then Parametric EQ. Each stage owns only preallocated callback state and a 10 ms bypass crossfade. The processor copies 29 append-only host parameter values at the block boundary; the DSP replaces non-finite values with catalog defaults and bounds output. The measured callback includes all six enabled stages and allocates zero times.

Offline WAV rendering cannot access the live `SynthEngine` or `EffectChain`. An explicit accepted composition triggers a message-thread snapshot of synth/effect parameters, modulation routes, master gain, and shared immutable A/B banks. A single worker constructs separate engines, consumes their immutable exchanges before the first event, streams a bounded stereo 24-bit WAV to a temporary sibling, reopens and validates its rate/length/header, and only then replaces the producer-approved destination. Cancellation removes the temporary file and never stops live notes.

## ADRs

- ADR-0001: JUCE pin and dependency acquisition.
- ADR-0003: Fixed-capacity immutable wavetable and modulation exchange for M2 (accepted).
- ADR-0004: Pure deterministic composition and shared MIDI delivery for M3 (accepted).
- ADR-0005: Bundled React UI and bounded Three.js visualizer for M4 (accepted).
- ADR-0006: Ordered effects and isolated offline preview for M5 (accepted).

History persistence and provider-secret decisions remain required before M6 and M7 respectively; no ADR claims completion yet.
