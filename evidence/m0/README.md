# M0 evidence

## Automated proof

- `standalone-webview-bridge.png`: Release Standalone visual proof of a bundled offline page, successful JavaScript-to-C++ native function, successful C++-to-JavaScript event, and the explicit producer MIDI drag strip.
- `pluginval/pluginval-release-strictness-5.txt`: pluginval 1.0.4 strictness-5 report ending in `SUCCESS`. It covers discovery, cold/warm open, editor lifecycle, editor while processing, 44.1/48/96 kHz audio processing at 64/128/256/512/1024 samples, state, automation, parameter access, and stereo bus layouts.
- `verification.txt`: reproducible architecture, signature, dependency, and test summary captured after the final build.

## Important limits

- pluginval explicitly skipped its optional Steinberg validator subtest because the separate validator executable was not installed. This remains an M0 gap, not a pass.
- The screenshot proves the Standalone WebView bridge, not FL Studio behavior.
- The deterministic MIDI file is automatically reopened by native tests, but dragging it into FL Studio remains a human action.
- The VST3 is ad-hoc signed for local engineering only; it is not notarized or ready to distribute.
