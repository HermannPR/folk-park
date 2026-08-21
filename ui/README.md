# Web UI boundary

M4 uses pinned React, TypeScript, Vite, and Three.js dependencies with a committed npm lockfile. `npm run build` writes the production `index.html`, `app.js`, and `app.css` directly to `resources/ui`, where CMake embeds them. No development server URL or remote runtime asset may ship.

The browser is presentation only. C++ owns parameters and state. Complete bridge snapshots are versioned, structurally validated, and bounded before publication. The wavetable scene consumes fixed UI copies, and the playable piano crosses a fixed single-producer/single-consumer MIDI queue. The WebView never calls DSP directly. Audio remains independent of React/WebGL, and the native fallback stays available.

From this directory, run `npm ci`, `npm run lint`, `npm test`, and `npm run build`. Commit both source and generated resources whenever the production bundle changes.
