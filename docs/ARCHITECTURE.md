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

Piano-roll presentation, SMF export/drag, and direct output all read the same validated events. SMF work and direct schedule construction happen off audio. Direct MIDI uses a fixed double schedule and atomic block-boundary publication; the callback performs bounded message insertion and note tracking without locks or owned-vector work. M6 persists accepted clips in bounded host project state and searchable local history without changing this delivery boundary.

## M4 presentation, visualization, and audition boundary

The M4 React tree is presentation only. JUCE Web parameter relays own host gestures, and one strict version 1 complete snapshot restores parameters, actual fixed A/B wavetable copies, modulation routes, composition state, version/status, and active voices. React validates the entire payload before publishing it, so malformed or future data cannot partially replace the last coherent view.

The visualizer receives at most 16 frames × 96 samples copied under the message-thread snapshot lock. It derives geometry and spectrum in JavaScript outside audio. Three.js renders at no more than 30 FPS; the 2D fallback renders at no more than 12 FPS; reduced-motion and hidden policies stop continuous work. ResizeObserver updates presentation dimensions without changing DSP ownership.

Touch/computer audition commands cross a fixed 64-command SPSC queue. The producer never calls the synth directly. The audio callback drains bounded note events, tracks native active notes, and makes duplicate note-on/off commands idempotent. Release-all is an atomic request used by focus loss, visibility loss, editor close, Panic, and overflow recovery. Host MIDI and preview MIDI meet only at the processor block boundary.

Composition-note editing remains non-real-time. Each edit copies the candidate, clamps and validates every affected value, stable-sorts events, validates the complete bundle, and then publishes transactionally. The accepted bundle is a separate immutable value until the producer explicitly accepts the edited candidate again.

## M5 effects and offline-render boundary

The real-time chain has one fixed serial order: Distortion, Chorus, tempo-synced Delay, Reverb, Compressor, then Parametric EQ. Each stage owns only preallocated callback state and a 10 ms bypass crossfade. The processor copies 29 append-only host parameter values at the block boundary; the DSP replaces non-finite values with catalog defaults and bounds output. The measured callback includes all six enabled stages and allocates zero times.

Offline WAV rendering cannot access the live `SynthEngine` or `EffectChain`. An explicit accepted composition triggers a message-thread snapshot of synth/effect parameters, modulation routes, master gain, and shared immutable A/B banks. A single worker constructs separate engines, consumes their immutable exchanges before the first event, streams a bounded stereo 24-bit WAV to a temporary sibling, reopens and validates its rate/length/header, and only then replaces the producer-approved destination. Cancellation removes the temporary file and never stops live notes.

## M6 persistence and recovery boundary

`PersistenceCoordinator` owns preset files, content-addressed assets, and SQLite history on non-audio threads below one validated application-support root. Preset availability and history availability are separate so a database failure cannot block native sound recall or composition acceptance. Filesystem paths, JSON, and SQLite handles never cross into `processBlock`.

A preset load prepares both oscillator banks and the complete modulation snapshot before one fixed-capacity block-boundary publication. Busy or invalid publication leaves the active engine unchanged. APVTS parameters and session identity update only after the complete native sound is accepted. Save As creates a new stable UUID unless the producer explicitly chooses to replace the current library preset.

Host project state contains one bounded, versioned preset payload plus optional accepted composition and history lineage. Restoration validates the complete payload without an editor. If a referenced imported asset is unavailable, the processor retains the current parameters, wavetables, and composition while exposing an exact SHA-256/size recovery request. Only a matching explicit relink completes the pending transaction.

## M7 assistant orchestration boundary

Offline composition text and guided sound design share one versioned `AssistantRequest`/`AssistantResponse` boundary but use distinct typed targets. Composition returns a validated candidate `MusicIntent`; sound design returns a catalog-validated `ParameterProposal`. Request UUID, target, processing origin, typed context, and result must match before a session can change.

The provider interface is non-audio and asynchronous with cancellation and at-most-once completion. Offline and mock implementations require no credential. The macOS implementation stores bounded opaque future-provider bytes behind a native generic-password Keychain interface; strict identifiers, device-only accessibility, exact removal, and a move-only read result prevent credentials from entering React or serialized product state. A future remote origin still requires a selected adapter, provider-specific disclosure, and per-request producer consent. Natural language, credentials, and provider output never enter the audio callback or host project state.

The bundled Jarvis workspace calls seven strict message-thread native functions. The bridge accepts only bounded named sound-intent fields or one bounded composition prompt/seed, rejects unknown object properties, and returns finite typed payloads. React validates progress, UUID-linked proposals, statuses, sides, unique parameter changes, explicit-acceptance flags, and candidate composition summaries before replacing view state. The conversation transcript is presentation-only and is neither serialized into a host project nor sent anywhere in the offline path.

Sound A/B captures immutable current/proposed normalized values for every changed host parameter. The processor canonicalizes B through the real parameter's legal range, then message-thread host gestures may select either review side. Temporary switches do not dirty the preset; rejection restores A and only explicit acceptance commits B. External host edits invalidate the comparison without being overwritten. Version-2 host project state restores an active comparison on its audible side while version 1 remains supported. Composition retains the existing candidate/accepted boundary and accepted-only MIDI/WAV delivery.

## ADRs

- ADR-0001: JUCE pin and dependency acquisition.
- ADR-0003: Fixed-capacity immutable wavetable and modulation exchange for M2 (accepted).
- ADR-0004: Pure deterministic composition and shared MIDI delivery for M3 (accepted).
- ADR-0005: Bundled React UI and bounded Three.js visualizer for M4 (accepted).
- ADR-0006: Ordered effects and isolated offline preview for M5 (accepted).
- ADR-0007: Versioned native presets, content-addressed assets, transactional history, and project recovery for M6 (accepted).
- ADR-0008: Offline-first assistant orchestration, proposal-version migration, provider consent, and A/B acceptance for M7 (accepted).

The real-provider selection remains open M7 product work; the native Keychain boundary, offline/manual behavior, and explicit assistant acceptance are fixed. Distribution licensing/signing and release packaging remain M8 boundaries.
