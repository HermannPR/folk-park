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

The WebView remains an adapter over host parameter attachments and explicit native functions. File choice, confirmation, cancellation, route editing, and status happen on non-audio/UI boundaries. The full 32-route presentation is deferred to M4; the current editor intentionally presents one replace-all reviewed route.

## ADRs

- ADR-0001: JUCE pin and dependency acquisition.
- ADR-0002: UI-to-native bridge and bundled resource strategy.
- ADR-0003: Fixed-capacity immutable wavetable and modulation exchange for M2 (accepted).
- ADR-0004: History persistence implementation before M6.
- ADR-0005: AI-provider secret and validation boundary before M7.
