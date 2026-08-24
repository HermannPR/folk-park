# M8 automated verification

Date: 2026-08-24 (America/Monterrey)

## Result

The private M8 Intel macOS engineering checkpoint passes its complete automated UI, native, Release, validator, installed-parity, runtime-recovery, schema, provenance, and security gates. No Critical or High defect is known from those gates.

This is not an FL Studio pass or a distributable public release. Every FL Studio, listening, physical MIDI/audio-device, signing/notarization, JUCE distribution-licensing, final identity, privacy/legal, and asset-approval decision remains explicitly open or `HUMAN RUN REQUIRED`.

## Environment and method

- Hardware: MacBookPro15,1; Intel Core i9-9880H at 2.3 GHz; 16 GB RAM.
- Operating system: macOS 15.7.9 (24G830).
- Target: thin Intel `x86_64`, Release configuration, VST3 and Standalone.
- Audio/runtime probe: 48 kHz, 512 samples, four repeated notes, 2 x 2 unison, all six effects enabled, one panic cycle.
- Repository: `HermannPR/folk-park`, confirmed `PRIVATE` at the gate.

No serial number, hardware UUID, credential, personal project/preset name, personal path, prompt, audio, MIDI contents, or database row is retained in this evidence.

## Verified results

- Clean pinned UI install: PASS; 34 packages, 0 vulnerabilities.
- Production-dependency audit: PASS; 0 vulnerabilities.
- UI protocol/interface/Jarvis/provider/diagnostics contracts: PASS, 17/17.
- Strict TypeScript/lint: PASS.
- Production Vite bundle: PASS; local index 0.40 kB, CSS 22.55 kB (5.79 kB gzip), JavaScript 831.63 kB (214.76 kB gzip).
- Complete Release configure/build: PASS for all test products, Standalone, and VST3.
- Complete Release CTest: PASS, 16/16 in 7.83 seconds.
- Extended Release runtime hardening: PASS; 11,250 blocks and 120 simulated seconds completed in 18,367 ms (`0.153058x` realtime on this machine).
- Runtime coverage: finite output throughout; repeated note/release; 2 x 2 unison; all six effects; panic and final zero voices; preview-MIDI overflow recovery; exact direct-MIDI Stop; three editor reconstructions while a host-held voice continued.
- Real-time allocation contract: PASS; the measured callback path allocates zero times.
- Release VST3 and Standalone: thin Mach-O `x86_64` artifacts.
- Release VST3 local ad-hoc signature: PASS with deep/strict verification. The private Standalone engineering artifact remains unsigned.
- pluginval 1.0.4 strictness 5: `SUCCESS`, including editor lifecycle, editor while processing, state, automation, buses, and processing at the validator's supported rate/block matrix.
- Installed user VST3: PASS. The exact validated M8 bundle was installed; build/installed executable hashes match; thin architecture and deep/strict signature verify; an independent VST3 host instantiated it and rendered finite centred stereo audio from MIDI.
- Conservative replacement behavior: PASS. The prior bundle remains at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T133139Z`; no Application Support, preset, imported asset, history, export, or DAW-project data was touched.
- Diagnostics tests: PASS for deterministic typed output below 4 KiB, adversarial host-text sanitization, fixed status codes/counters, and exact preview-ID-before-copy behavior.
- Release-material audit: PASS. JUCE/VST3 license hashes and bundled runtime versions/licenses are pinned; no unreviewed runtime media/font asset or external runtime UI URL is tracked.
- Authored source `eval()` scan: PASS. The one production-bundle occurrence is JUCE's pinned Android user-script compatibility branch and is not invoked by the Intel macOS build.
- Runtime development-origin scan: PASS; no localhost, loopback, or WebSocket origin appears in authored or embedded runtime material.
- Sensitive-token form scan: PASS outside documented pinned-dependency/lockfile/evidence exclusions.
- All seven strict JSON schemas parse successfully.
- `git diff --check`: PASS before documentation finalization.

## Release test inventory

The 16 Release CTest suites are native engine, effects chain, offline preview isolation, preset persistence, history repository, composition properties, MIDI delivery parity, assistant schema/model, offline assistant workflow, macOS Keychain, realtime allocation, runtime hardening/recovery, bounded diagnostics, support-script contracts, processor state/UI integration, and the packaged VST3 MIDI smoke test.

## Artifact hashes

- Release and installed VST3 executable SHA-256: `9295e582e705837020f72f657105d5efd2213d5e8904dee628d7e55e52a82a84`.
- Release Standalone executable SHA-256: `bb61054c5acf8f9fb3711acd49220dc6ddcf6508d4ea4bc5513d6e82c1778386`.
- pluginval log SHA-256: `ff5a2222543e321444e3bc35994a801ae5402e06d8f44a2aeb6cb49bc94c94b4`.
- Official pluginval 1.0.4 macOS archive SHA-256: `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`.
- Embedded UI index SHA-256: `b139fcadf5728e39c965fae2a410710a6319c894a6f9bf5560ca79ac1891d4b8`.
- Embedded UI CSS SHA-256: `18bf71139fccd0f4983c19079f6746c436dccf4868630b898fddee29aa68af33`.
- Embedded UI JavaScript SHA-256: `35532a873be9192edbf7abc2a00fded3f5c6308d4857b3b016de3c97526fa3cb`.

## Retained artifacts

- `pluginval/pluginval-release-strictness-5.txt`
- `runtime-hardening-debug.md`
- Real Release diagnostics screenshot: pending the owner-visible Preview action required by macOS accessibility controls.

## Explicit gaps and observations

- Every row in `docs/FL_STUDIO_TEST_MATRIX.md` remains `HUMAN RUN REQUIRED`. Automated VST3 loading is useful host-independent evidence, not an FL Studio compatibility claim.
- The diagnostics UI intentionally refuses clipboard copy until the producer previews the exact current report. No Copy action was automated.
- The measured Release runtime ratio is a one-machine observation, not an owner-approved CPU budget and not an audible-quality result.
- pluginval's optional separate Steinberg-validator subtest was skipped because no validator executable path is installed; the complete pluginval strictness-5 run itself ended `SUCCESS`.
- The current Jarvis engine is deterministic and offline. No remote provider is selected, configured, called, or claimed as tested.
- The Standalone engineering artifact is unsigned. Distribution signing/notarization and packaging are not complete.
- Public distribution remains blocked by the owner decisions in `docs/OPEN_DECISIONS.md`, including JUCE distribution licensing.
