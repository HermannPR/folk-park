# Folk Park repository instructions

## Required continuation context

Before changing code, read `docs/CURRENT_WORK.md` completely. It is the authoritative handoff for the active branch, completed gates, outstanding human checks, requested product behavior, and the next milestone. Reconcile it with `git status`, `git log`, `docs/PROGRESS.md`, and `plans/RELEASE_0_1.md`; never restart completed work or infer completion from a plan alone. Update `docs/CURRENT_WORK.md` at every milestone checkpoint and before handing work to another agent.

## Product boundary

Build `folk park` 0.1 as an original Intel macOS x86_64 VST3 instrument and Standalone application. The release combines a playable wavetable synth with deterministic, offline-first MIDI composition. It does not clone Serum, import proprietary Serum state, ship third-party factory assets, or modify a DAW project without explicit producer acceptance.

## Directory map

- `src/plugin`: JUCE host adapter, buses, processor/editor, and state coordination.
- `src/synth`, `src/modulation`, `src/filters`, `src/effects`: real-time DSP only.
- `src/midi`, `src/assistant`: bounded composition and validated intent outside the audio callback.
- `src/persistence`, `src/platform`: filesystem, database, and Keychain abstractions.
- `src/diagnostics`: bounded non-realtime technical reports and preview-before-copy enforcement.
- `src/ui_bridge`, `ui`: message-thread bridge and bundled React/TypeScript UI.
- `tests`: deterministic native tests and fixtures.
- `docs`, `plans`, `evidence`: contracts, decisions, progress, and reproducible proof.
- `third_party/JUCE`: JUCE 8.0.13 pinned at commit `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.

## Commands

Tool binaries installed with `python3 -m pip install --user cmake ninja` may be under `~/Library/Python/3.9/bin`. The scripts resolve that location explicitly.

```sh
./scripts/bootstrap_macos.sh
cmake --preset macos-x86_64-debug
cmake --build --preset macos-x86_64-debug
cmake --preset macos-x86_64-release
cmake --build --preset macos-x86_64-release
cd ui && npm ci && npm run build && npm test && npm run lint
./scripts/test.sh
./scripts/install_user_vst3.sh
```

Formatting and validation commands will be added only when their pinned tools exist. Do not claim a missing command passed.

## Real-time rules

Nothing reachable from `processBlock` may allocate, lock, block, access files/databases/network/clipboard/WebView/shell, parse JSON/XML, log formatted text, call UI code, use unbounded work, or destroy potentially blocking objects. Preallocate in `prepareToPlay`; exchange validated immutable snapshots through bounded or atomic handoff; smooth audible continuous parameters; keep output finite and feedback bounded.

## Compatibility rules

Parameter IDs, schema fields, preset versions, database migrations, and bridge message names are append-only compatibility surfaces unless a tested migration exists. Never reuse a parameter ID or silently change an existing meaning, range, or default. C++ is authoritative for audio parameters and state. State parsing is transactional and must not depend on the editor being open.

## Security and dependencies

Never commit credentials or include them in plug-in state, presets, logs, generated MIDI, or history exports. Treat imported files and provider responses as untrusted. Remote AI is opt-in; Release 0.1 must work offline. A new dependency requires a short ADR and an entry in `LICENSES.md`; pin every dependency and commit lockfiles or immutable revisions.

## Definition of done

A milestone is complete only when its gate commands have run and evidence is recorded in `docs/PROGRESS.md`. Before reporting completion, review `git diff`, run targeted tests, distinguish automated proof from human FL Studio checks, and state failures plainly. Release 0.1 requires x86_64 Standalone and VST3 Release builds, validator evidence, automated tests, host validation status, asset/license provenance, and documented limitations.
