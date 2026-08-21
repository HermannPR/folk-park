# Web UI boundary

M0 uses `resources/ui/index.html` solely to prove bundled, offline JUCE native integration before adding a package graph. React, TypeScript, Vite, and a committed npm lockfile enter at M4 (or an earlier isolated slice) through an ADR and license entries. Production output will be embedded; no development server URL may ship.

The browser is presentation only. C++ owns parameters/state, bridge payloads are versioned and bounded, all callbacks run away from the audio thread, meters are throttled/lossy, and a native fallback must remain available.
