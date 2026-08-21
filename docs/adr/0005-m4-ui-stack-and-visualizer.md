# ADR-0005: Bundled React UI and bounded Three.js visualizer

- Status: Accepted for M4 implementation
- Date: 2026-08-20

## Context

M4 replaces the single-file engineering page with the production Silicon Dreams component system. It must stay offline inside JUCE's WebView, recover from C++ state, communicate through bounded versioned messages, and offer a useful live wavetable visual without creating audio-thread, resize, focus, GPU, or hidden-window regressions.

## Decision

- Pin React, React DOM, TypeScript, Vite, and Three.js in `ui/package.json` and commit the npm lockfile. Vite produces deterministic `index.html`, `app.js`, and `app.css` files directly into `resources/ui`; CMake embeds those files and does not ship a development-server origin. The JavaScript stays in one bounded embedded resource so JUCE never has to discover a runtime chunk URL.
- React owns presentation and local UI-performance preferences only. JUCE parameter relays and versioned native snapshots remain authoritative for synth, composition, import, and route state.
- Three.js is restricted to the oscillator/modulation scene. It receives a fixed-size downsampled wavetable snapshot copied on the message thread. It never observes a mutable bank, audio buffer, pointer, FFT object, or callback-owned state.
- Normal graphics are capped at 30 frames per second. Low Graphics is capped at 12 frames per second and uses the 2D canvas fallback. Reduced Motion renders only after meaningful state changes. `document.visibilityState` stops animation while hidden.
- Every inbound complete snapshot is structurally validated before React publishes it. Unknown schema versions and malformed/oversize arrays are rejected without replacing the last valid view.
- The native fallback editor remains available if WebView options are unsupported. Audio and MIDI behavior remain independent of React and WebGL.

## Consequences

- UI dependency licenses and pins are compatibility/build surfaces and must be reviewed before public distribution.
- Clean setup runs `npm ci`, UI type checking/tests, and the production build before the CMake configure/build.
- M4 tests must cover snapshot validation/recovery, size bounds, hidden/reduced/low animation policy, accessible navigation, offline output scanning, and native audio behavior with the editor open/closed.
