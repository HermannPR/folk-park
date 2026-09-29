# Third-party licenses and asset provenance

No folk park binary is authorized for public distribution at this checkpoint.

| Component | Pin | Purpose | License status |
| --- | --- | --- | --- |
| JUCE | 8.0.13, `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` | Plug-in framework, DSP/platform/UI integration | JUCE 8 license choice remains an owner decision before distribution. Starter, Indie, Pro, and Educational terms have different eligibility. |
| VST3 SDK as bundled by JUCE | JUCE pin above | VST3 target | VST 3.8+ SDK is MIT-licensed; trademark usage remains subject to Steinberg guidelines. |
| React / React DOM | 19.2.8 | Bundled WebView component system | MIT; include upstream notices before distribution. |
| TypeScript | 7.0.2 | UI type checking | Apache-2.0; build-time only. |
| Vite | 8.2.2 | Deterministic production UI bundle | MIT; build-time dependency graph is locked by npm. |
| Three.js | 0.185.1 | Bounded oscillator/modulation scene only | MIT; no third-party example assets are bundled. |
| SQLite | macOS system SQLite C API; SDK/runtime supplied by the target OS | Transactional local preset/history index behind `HistoryRepository` | System library only; no SQLite source or binary is committed or redistributed. SQLite is public domain. |

Project-created waveforms and UI assets must have provenance recorded here. User-imported WAV files remain user-owned and are not committed. Serum factory assets, names, artwork, source code, and private preset payloads are prohibited.
