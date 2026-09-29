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

## Runtime notice audit

The embedded JavaScript runtime is exactly React 19.2.8, React DOM 19.2.8, Scheduler 0.27.0, and Three.js 0.185.1; all four report MIT in the pinned lockfile and retain upstream license files. `THIRD_PARTY_NOTICES.md` records their notices and the pinned JUCE/VST3 license-file hashes. TypeScript/Vite remain build-only.

`scripts/audit_release_material.sh` fails if those versions/licenses or JUCE/VST3 license files differ, if a tracked runtime media/font asset appears outside evidence, or if runtime UI source gains an external URL. Passing that script is provenance evidence, not legal approval of the unresolved JUCE license choice.

## Project asset inventory

- Built-in wavetables are generated numerically by project source; no factory/user WAV is bundled.
- UI surfaces, controls, wave/spectrum visualization, and atmosphere are project-authored code/CSS/geometry. No external image, font, model, or texture is bundled.
- M0–M7 PNG files are real engineering evidence and are not embedded into the Standalone/VST3 runtime.
- User-imported WAVs and producer-exported MIDI/WAV remain user-owned local data and are excluded from Git/packages.
- No Serum code, preset payload, factory name, artwork, wavetable, installer, license file, or reverse-engineered private state is present.

Project-created waveforms and UI assets must have provenance recorded here. User-imported WAV files remain user-owned and are not committed. Serum factory assets, names, artwork, source code, and private preset payloads are prohibited.
