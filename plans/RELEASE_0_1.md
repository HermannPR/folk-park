# Release 0.1 milestone plan

Each gate must leave the repository buildable. Automated evidence and human host evidence are different statuses.

## M0 — Repository, toolchain, and risk spikes (automated gate passed)

Deliver the documented layout, pinned JUCE, scripts, native tests, x86_64 Standalone/VST3 shell, versioned state skeleton, bundled WebView proof, tiny MIDI export/drag proof, routing/licensing notes, and validator result.

Commands: `./scripts/bootstrap_macos.sh`, CMake debug/release presets, `./scripts/test.sh`, binary `file` inspection, available VST3 validator. Gate is green only when clean configure/build/tests and evidence succeed without a production development server.

## M1 — Playable vertical slice (automated gate passed; human host run pending)

Deliver a polyphonic engine, one legal built-in wavetable oscillator, sub oscillator, amp ADSR, low-pass filter, master gain, panic, stable parameters/state, and minimal host-aware UI controls. Gate: finite audio from MIDI, state round trip, voice stealing/no-stuck-note, identical UI-open/closed render.

## M2 — Dual wavetable and modulation

Deliver oscillators A/B, safe user-WAV import/conversion, three envelopes, four LFOs, bounded modulation matrix, multimode filter, and parameter catalog. Gate: malformed imports rejected, click-safe no-allocation table swap, validated routes, spectral/CPU baseline.

## M3 — Composition and MIDI delivery (automated gate passed; FL Studio human drag/route pending)

Deliver MusicIntent/GeneratedClip, deterministic chords/melody/bass/arp, macros and variations, preview data, MIDI export/drag, and direct output. Gate: determinism/properties, export reopen parity, bounded events, explicit FL steps/results.

## M4 — Silicon Dreams UI

Deliver original bundled React/TypeScript views and chrome/glass/grid system; live oscillator A/B 2D waveform, optional 3D wavetable-frame/morph surface, current-position/modulation animation, and spectrum feedback; a four-octave C2–B5 touch/mouse piano with an octave-shiftable computer-key zone that auditions the current synth through a bounded native note queue with guaranteed release on pointer cancel, blur, hide, panic, and editor close; optional Three.js with low-graphics/reduced-motion fallback; accessible focus; and snapshot recovery. Visualizers must consume bounded UI snapshots, pause when hidden, and never read mutable audio state directly. Gate: offline bundle, resize, hidden animation pause, WebView/audio independence, malformed bridge rejection, visualizer CPU/frame-rate evidence, and no-stuck-note audition tests.

## M5 — Effects and preview

Deliver independent bypassable distortion, chorus, synced delay, reverb, compressor, EQ, ordered state, and isolated offline WAV render. Gate: finite/bypass/reset/sample-rate/isolation tests, WAV verification, gain-safe defaults.

## M6 — Presets and history

Deliver versioned native presets/assets/migrations and a searchable transactional history repository with lineage/retention. Gate: current/old/malformed/missing/oversize/traversal fixtures, database failure isolation, correct recall.

## M7 — Jarvis text, guided sound walkthrough, and providers

Deliver offline text-to-intent, adaptive sound questions, bounded/explained parameter proposals, reversible A/B audition, mock/provider boundary, Keychain abstraction, cancellation/offline UX, and at most one opted-in provider. Gate: schemas and the parameter catalog cannot be bypassed, no secrets/sensitive state, offline/manual paths remain complete, and acceptance is always explicit.

## M8 — Release candidate hardening

Deliver Release packaging notes, complete automated suite/validator/host matrix status, benchmarks, license/asset audit, recovery/diagnostics, install/routing/troubleshooting docs. Gate: all automated checks green, no critical/high known defect, unresolved human checks visible.
