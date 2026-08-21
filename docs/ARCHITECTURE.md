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

The plug-in processor adapts host buses, MIDI, transport, automation, and serialized state. The synth owns preallocated real-time voice/DSP state. Composition produces host-independent, validated `GeneratedClip` values. Assistant providers can only propose a bounded `MusicIntent`; they cannot execute code or write into a DAW. Persistence consumes snapshots outside the audio thread. The WebView is presentation only and must be recoverable from a complete C++ snapshot.

## M0 vertical shell

M0 proves the dependency seams before adding production DSP:

- A VST3 instrument and Standalone target declare stereo output, MIDI input, and MIDI output.
- A minimal processor clears or safely passes defined buffers and round-trips versioned state.
- A bundled WebView resource calls one native function and receives one native event, with a native fallback if unavailable.
- A deterministic tiny MIDI file can be written and reopened by a native test before a user explicitly drags it.
- Native tests run without FL Studio.

## Planned ADRs

- ADR-0001: JUCE pin and dependency acquisition.
- ADR-0002: UI-to-native bridge and bundled resource strategy.
- ADR-0003: Wavetable interpolation/band-limiting after the M1 baseline.
- ADR-0004: History persistence implementation before M6.
- ADR-0005: AI-provider secret and validation boundary before M7.
