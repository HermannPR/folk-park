# M8 release-candidate checklist

M8 begins at `eee67cf` on `feat/m8-release-hardening`, stacked exactly on `feat/m7-guided-assistant`. M7 is the regression baseline, not work to repeat.

Release 0.1 remains a **private engineering candidate — FL Studio human validation pending** until every required host row has real producer evidence. It is not authorized for public distribution while the owner decisions in `docs/OPEN_DECISIONS.md` remain unresolved.

## Defect severity gate

- **Critical:** credible data loss, secret disclosure, arbitrary execution, destructive filesystem behavior, repeatable host/Standalone crash or hang, or audio-thread behavior that can corrupt process state.
- **High:** repeatable project/preset corruption, non-finite output escaping containment, stuck notes after the documented release boundaries, a broken VST3 install/discovery artifact, or a required offline/manual workflow that cannot complete.
- **Medium:** recoverable workflow, presentation, performance, or compatibility defect with a documented safe workaround.
- **Low:** cosmetic or documentation issue that does not mislead about safety, compatibility, privacy, or evidence.

The M8 automated gate cannot close with a known Critical or High defect. Unrun human cases and unresolved distribution decisions are explicit release blockers/statuses, not silently downgraded defects.

## Deliverables and proof

| Area | Required deliverable | Evidence required | Current status |
| --- | --- | --- | --- |
| Diagnostics | Bounded native snapshot, sanitized subsystem codes, lock-free fault counters, explicit preview-before-copy UI | Pure contract tests, processor integration tests, UI adversarial tests, real Release inspection | DEBUG AUTOMATION PASS; RELEASE INSPECTION PENDING |
| Runtime hardening | Long-run finite audio/MIDI, stop/panic/no-stuck-note, UI-independent state, corrupted-state rollback | Deterministic Debug and Release tests with measured duration/configuration | DEBUG PASS; RELEASE PENDING |
| Performance | Current x86_64 synth/FX/modulation and UI-analysis baselines with machine/OS/build/method | Retained machine-readable or plain-text benchmark evidence; no invented budget | PENDING OWNER BUDGET |
| Installation | Exact build, install, rescan, repair, version/hash verification, safe uninstall, and rollback notes | Script tests/read-only checks plus human-ready FL steps | SCRIPT/DRY-RUN PASS; FL ACTIONS PENDING |
| Routing/troubleshooting | MIDI drag/direct routing, WAV import/render, focus, WebView, preset/database/provider recovery | Exact expected/actual/evidence fields; FL rows remain HUMAN RUN REQUIRED | PLAYBOOK COMPLETE; FL RUN PENDING |
| Licensing/assets | Dependency pins/licenses/notices, system-library boundary, project-created asset inventory, prohibited assets | `LICENSES.md`, provenance scan, explicit unresolved owner decisions | AUTOMATED AUDIT PASS; OWNER DECISIONS PENDING |
| Packaging | Private artifact layout and packaging notes without false signing/notarization claims | Architecture/signature/hash inspection; distribution actions gated | PRIVATE NOTES COMPLETE; OWNER DECISIONS PENDING |
| Final gate | Clean UI, Debug, Release, VST3 smoke, pluginval, installed parity, source/security/schema scans | `evidence/m8/verification.md` and retained logs/hashes | PENDING |
| Host matrix | Complete or exact human-ready FL Studio matrix | Only Hermann may replace HUMAN RUN REQUIRED with a real result | HUMAN RUN REQUIRED |

## Diagnostics contract

Diagnostics are opt-in support text, never an automatic log upload.

- Native snapshot maximum: 4 KiB UTF-8 and a fixed set of keys in deterministic order.
- Allowed configuration: product/version, build type, architecture, wrapper format, bounded host description, host version only when supplied, sample rate, block size, and active voice count.
- Allowed subsystem state: sanitized preset/history/provider/UI-bridge codes and numeric fault counters.
- Required audio-safe counters: final non-finite sample containment, direct-MIDI overflow, and preview-MIDI queue overflow. The callback may only increment atomics; formatting happens on the message thread.
- Forbidden content: API keys/tokens, credentials, complete prompts, project/preset names, UUIDs, tags, personal paths/filenames, audio/MIDI contents, clipboard contents, database rows, and unrestricted operating-system data.
- `Preview diagnostics` creates the complete bounded text and an opaque one-editor preview ID. `Copy previewed diagnostics` succeeds only for that exact current ID, after the producer has seen the text. Copying never occurs from the audio callback.
- Diagnostics are not serialized into presets, project state, history, generated MIDI, or provider traffic.

## Automated gate commands

Run UI generation before CMake configure/build, never concurrently:

```sh
cd ui
npm ci --ignore-scripts
npm audit --omit=dev
npm test
npm run lint
npm run build
cd ..

/Users/hermannpr/Library/Python/3.9/bin/cmake --preset macos-x86_64-debug
/Users/hermannpr/Library/Python/3.9/bin/cmake --build --preset macos-x86_64-debug
/Users/hermannpr/Library/Python/3.9/bin/ctest --preset macos-x86_64-debug --output-on-failure

/Users/hermannpr/Library/Python/3.9/bin/cmake --preset macos-x86_64-release
/Users/hermannpr/Library/Python/3.9/bin/cmake --build --preset macos-x86_64-release
/Users/hermannpr/Library/Python/3.9/bin/ctest --preset macos-x86_64-release --output-on-failure
```

The default runtime-hardening CTest simulates 12 seconds so routine gates remain practical. The retained M8 Release evidence must also run the bounded extended mode explicitly:

```sh
FOLK_PARK_RUNTIME_SECONDS=120 \
  build/macos-x86_64-release/FolkParkRuntimeHardeningTests_artefacts/Release/FolkParkRuntimeHardeningTests
```

Performance tests must require finite output and a valid positive measurement, then record the result. They must not fail against the legacy arbitrary `<4×` realtime threshold or a newly invented number; only an owner-approved budget may become a release gate.

Then run pluginval strictness 5 with GUI tests, inspect thin `x86_64` architecture and local signatures, install the exact validated VST3, compare installed/build hashes, run the independent installed-bundle MIDI smoke, parse all schemas, and scan tracked project/runtime material for credentials and development origins.

## Owner/human stop conditions

Do not infer or automate approval for:

- JUCE license eligibility or commercial/open-source distribution choice;
- legal developer/company name or final bundle identifier;
- signing identity, notarization profile, installer/update channel, privacy notice, or asset-rights approval;
- an approved CPU budget derived from measured baselines;
- a remote provider for 0.1;
- any FL Studio, listening, physical MIDI/audio-device, or offline-versus-real-time parity result.
