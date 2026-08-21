# Progress

## Current checkpoint

- Milestone: M0 — Repository, toolchain, and risk spikes
- Status: Automated M0 checkpoint passed; FL Studio human run remains explicit
- Date: 2026-08-20 (America/Monterrey)

## Completed inspection

- Read the complete version 1.0 product/engineering contract and preserved a byte-identical repository copy.
- Confirmed Intel x86_64, macOS 15.7.9, Apple clang 17.0.0, Command Line Tools SDK 15.5, Node 24.18.1, and npm 11.16.0.
- Confirmed FL Studio 26.1.4.5356 and an x86_64 host slice.
- Confirmed CMake, Ninja, JUCE, GitHub CLI, and full selected Xcode were absent at intake.
- Confirmed no pre-existing `/Users/hermannpr/folk-park` repository or applicable parent `AGENTS.md` existed.

## Implemented M0 checkpoint

- Created the required repository boundaries, master-source record, assumptions, open decisions, architecture, real-time safety, compatibility, provider security, parameter/MIDI/preset contracts, routing note, release plan, and JUCE ADR.
- Pinned JUCE 8.0.13 at immutable commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.
- Added x86_64 Debug/Release CMake presets, user-local bootstrap/build/test/install scripts, VST3 and Standalone instrument targets, stereo output and MIDI input/output declarations.
- Added a transactional versioned APVTS state skeleton and stable `masterGain` parameter ID.
- Added a bundled offline WebView with a working native function and event, timing-safe initial snapshot retry, native fallback, and explicit MIDI drag strip.
- Added a deterministic standards-compliant M0 MIDI file, safe temporary write, reopen validation, and native test.

## Commands and results

- `textutil -convert txt -stdout ...docx`: PASS; full contract reviewed in bounded chunks.
- `shasum -a 256 ...docx`: PASS; source and repository copy both `b124353abd2b9ad41da7c957f741b76a31b244eb8533f1eb3b6196e0915e15d1`.
- `uname -m`, `sw_vers`, `clang --version`, `xcrun --sdk macosx --show-sdk-version`: PASS; environment recorded above.
- FL Studio `Info.plist` and `file OsxFL`: PASS; version and x86_64 slice confirmed.
- `./scripts/bootstrap_macos.sh`: PASS; exact architecture and dependency pins verified.
- `./scripts/build_x86_64.sh`: PASS; Debug and Release Standalone/VST3 targets built.
- `./scripts/test.sh`: PASS; Debug native test passed.
- `ctest --preset macos-x86_64-release`: PASS; Release native test passed.
- `file` on all four target binaries: PASS; every binary is thin x86_64.
- `codesign --verify --deep --strict`: PASS for the Release VST3 with an ad-hoc local signature.
- pluginval 1.0.4 strictness 5: SUCCESS; full report retained. Its optional separate Steinberg-validator subtest was skipped because that executable is not installed.
- Standalone visual proof: PASS; bundled UI, JavaScript-to-C++ native function, and C++-to-JavaScript event are simultaneously visible.
- `gh auth status`: GitHub CLI 2.97.0 is installed but account authentication is still pending.

## Evidence

- `docs/source/folk_park_Codex_Master_Build_Prompt.docx`
- `docs/MASTER_SOURCE.md`
- This progress record and architecture/risk contracts.
- `evidence/m0/verification.txt`
- `evidence/m0/pluginval/pluginval-release-strictness-5.txt`
- `evidence/m0/standalone-webview-bridge.png`

## Risks and limitations

- JUCE licensing selection is unresolved; private engineering may continue, but no binary distribution is authorized.
- Full Xcode is absent; Command Line Tools were sufficient for the verified CMake Debug/Release builds.
- GitHub owner cannot be confirmed until GitHub CLI is installed and authenticated.
- The shell intentionally outputs silence; playable synthesis begins at M1.
- pluginval passed, but the separate Steinberg validator executable remains unavailable and is recorded as an explicit tooling gap.
- The VST3 is ad-hoc signed only; no distribution signing, notarization, or JUCE distribution authorization exists.
- All FL Studio cases remain human-run-required.

## Next smallest verifiable task

Authenticate Hermann's GitHub account, create and verify the private remote, push the reviewed first-person commits, then begin the M1 playable synth slice without changing M0 compatibility surfaces.
