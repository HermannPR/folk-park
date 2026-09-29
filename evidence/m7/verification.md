# M7 verification

Date: 2026-08-23 (America/Monterrey)

## Verified results

- Clean pinned UI install and production-dependency audit: PASS; npm reported 0 vulnerabilities.
- UI protocol/interface/Jarvis/provider contracts and strict TypeScript: PASS, 15/15.
- Production Vite bundle: PASS; `app.js` 829.27 kB (214.23 kB gzip), `app.css` 22.29 kB (5.72 kB gzip), and local index 0.40 kB.
- Complete Debug Standalone/VST3 build and CTest: PASS, 12/12.
- Complete Release Standalone/VST3 build and CTest: PASS, 13/13, including packaged VST3 scan/instantiate/finite-stereo MIDI rendering and the real macOS Keychain round trip.
- Release Standalone and VST3 binaries are thin Mach-O `x86_64` artifacts.
- The Release VST3 local ad-hoc signature passes `codesign --verify --deep --strict`; the private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS` for editor, editor-while-processing, state, automation, buses, and 44.1/48/96 kHz at 64/128/256/512/1024 samples.
- Installed user VST3: PASS; installed/build hashes match, architecture/signature verify, and the installed bundle independently instantiates and renders finite stereo MIDI audio.
- Source/runtime-origin scan: PASS; no localhost, loopback, WebSocket, or development origin appears in Folk Park native/UI source or embedded index/style. All 14 project-source HTTP strings are JSON Schema identifiers.
- Folk Park native/UI source contains no `eval` call. The known Vite warning remains confined to JUCE's pinned Android compatibility helper.
- Sensitive-token scan: PASS; no private-key or common provider/GitHub access-token form matched tracked project material outside the pinned dependency/lockfile exclusions.
- All 7 strict JSON schemas parse successfully; `git diff --check` passes.

## M7 behavior covered

- Typed versioned composition/sound assistant requests and responses with target/origin/UUID matching, bounded prompts, remote-consent requirements, cancellation, and at-most-once completion.
- Deterministic offline composition-text mapping and a stable sound walkthrough that asks no more than two focused questions at a time.
- Catalog-resolved, bounded, finite, explained parameter proposals with explicit acceptance and current-to-proposed values.
- Processor-owned reversible A/B preview, host-value canonicalization, rejection restoring exact A, explicit B acceptance, stale host-edit invalidation, and temporary-preview dirty suppression.
- Versioned editor-independent project recovery of an active comparison without weakening version-1 project compatibility.
- Strict bounded native operations and frontend parsing for progress, proposal, A/B, explicit decisions, and candidate-only composition output.
- Native Keychain storage of opaque credential bytes under exact service/provider identifiers with a 16 KiB bound, device-only accessibility, fail-closed query construction, exact update/read/remove, and a move-only best-effort-zeroed read owner.
- Provider settings expose availability/configuration booleans only. There is no JavaScript credential field, remote adapter, configured credential, outbound request, or provider claim.
- Real Release Standalone interaction: native settings reported offline/no-provider/no-credential truthfully; a typed sound goal created an explained 12-change proposal on original A; B became the selected preview; Reject restored A; guided answers persisted and advanced from role/timbre to articulation/movement; the app accepted its application quit event.

## Evidence hashes

- Release/installed VST3 binary SHA-256: `b17c88bab2c1356c7b01980b96f918a28acbdd337f7ee2e437f9c63a7d7119ca`.
- Release Standalone binary SHA-256: `4523ffa815cfcdd7fb4d666644f75dde82869f6ebf673f9707f31314c8d3b1da`.
- pluginval log SHA-256: `b1caccc89b62a5e3bcf19a18388b3c093954e1e152bab27ccaaad3762edcf6e2`.
- Official pluginval 1.0.4 macOS archive SHA-256: `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`.
- Active-B screenshot SHA-256: `7c7d81313a9420d2606d7ab8dff5f03e5c802c5a3bfffdb66a03256a139ae0b6`.
- Proposal-ready screenshot SHA-256: `d9c5a695d559e1ac0fb02ca4d7ac7fe1f84e7ee6a34725d08606d8763387bba5`.
- Guided-start screenshot SHA-256: `0818b393cac20a2a3bef8ea3b94f3b05b2255c460d8c76a3d881707eea49427b`.
- Guided-progress screenshot SHA-256: `98c25190822b97f33e7faa69dbe1baae2451cb77dbb0cdbc7b5f31158341fcd6`.
- Provider-settings screenshot SHA-256: `360c4b4d6c470c2746715a47bd4b71eb2c8a723510fa7697078f494933364f1a`.

## Retained artifacts

- `pluginval/pluginval-release-strictness-5.txt`
- `standalone-m7-jarvis-proposal.png`
- `standalone-m7-jarvis-ab.png`
- `standalone-m7-jarvis-guided.png`
- `standalone-m7-jarvis-guided-progress.png`
- `standalone-m7-settings.png`

## Explicit gaps and observations

- Every FL Studio discovery, insertion, listening, input/focus, automation, project reopen, Jarvis A/B/recovery, composition delivery, and failure-isolation case remains `HUMAN RUN REQUIRED`.
- No real remote provider is selected. This is an intentional offline-first product boundary, not a claim that provider networking was tested.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed.
- The Vite build warning comes from direct `eval` in pinned JUCE `check_native_interop.js`, an Android interoperability helper. Folk Park source does not call `eval`, and the macOS UI uses embedded local resources.
- A synthetic Command-Q sent while a WebView field retained focus did not close the automated test process; the application's direct quit event closed it normally. Physical keyboard/menu focus behavior remains part of human host/Standalone testing.
- Standalone visual/interaction evidence does not prove audible quality, physical MIDI/audio-device behavior, or FL Studio compatibility.
- Distribution signing/notarization, JUCE distribution licensing, final identity, privacy/legal review, and asset-rights gates remain M8/open decisions. These are private engineering artifacts, not a distributable release.
