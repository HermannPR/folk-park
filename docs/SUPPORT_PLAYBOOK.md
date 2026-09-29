# Installation, recovery, and troubleshooting playbook

This playbook applies to the private Intel macOS engineering candidate. It does not claim signing, notarization, public distribution readiness, or a completed FL Studio test.

## What lives where

| Data | Exact location | Removal policy |
| --- | --- | --- |
| User VST3 | `~/Library/Audio/Plug-Ins/VST3/folk park.vst3` | May be moved to Trash by the safe uninstall script |
| Native presets | `~/Library/Application Support/folk park/Presets` | Never removed by install/repair/uninstall |
| Imported wavetable assets | `~/Library/Application Support/folk park/Presets/assets` | Never removed automatically; required by presets/projects that reference them |
| Composition history | `~/Library/Application Support/folk park/history.sqlite3` plus SQLite sidecars while open | Never removed automatically |
| DAW project state | Inside the FL Studio project/container controlled by the host | Never edited by support scripts |
| Drag MIDI | One uniquely named `folk-park-*.mid` in the macOS temporary directory | The creating editor removes only its exact guarded file on replacement/destruction |
| Rendered MIDI/WAV | Producer-selected destination | User-owned output; never removed automatically |

Before repair or experimentation, close FL Studio and copy the entire `folk park` Application Support directory to an external or otherwise independent backup if the presets/history are important. Do not recursively change its owner or permissions and do not delete the database to fix a scan problem.

## Build and verify

Build the bundled UI first, then CMake; never run those generation steps concurrently:

```sh
cd ui
npm ci --ignore-scripts
npm audit --omit=dev
npm test
npm run lint
npm run build
cd ..
cmake --preset macos-x86_64-release
cmake --build --preset macos-x86_64-release
ctest --preset macos-x86_64-release --output-on-failure
```

Verify the exact bundle before installation:

```sh
./scripts/verify_user_vst3.sh \
  "build/macos-x86_64-release/FolkPark_artefacts/Release/VST3/folk park.vst3"
```

Verification requires a regular bundle/executable, one thin `x86_64` architecture, a valid deep/strict local signature, and prints the executable SHA-256 and size. It does not turn an ad-hoc engineering signature into Developer ID signing/notarization.

## Install or repair safely

Preview without writing:

```sh
./scripts/install_user_vst3.sh release --dry-run
```

First install when no destination exists:

```sh
./scripts/install_user_vst3.sh release
```

Repair/replace only after reviewing the dry run:

```sh
./scripts/install_user_vst3.sh release --replace
```

Replacement moves the previous exact bundle to a timestamped `.backup-*` sibling, copies the new bundle, verifies architecture/signature/hash parity, and retains the rollback copy. If verification fails, the failed copy is retained as `.failed-*` and the previous bundle is restored. Nothing under Application Support is touched.

After installation, open FL Studio → Plugin Manager, rescan installed plug-ins, and record discovery as a real result in `docs/FL_STUDIO_TEST_MATRIX.md`. Do not mark it passed from the script alone.

The post-M8 Compose-control repair currently installed for that human run has executable SHA-256 `823bf765a1744b7de6e8232ef17ad4d93d209628e01dfd6f716b2dff14b0131d`. Its build/install parity, thin `x86_64` architecture, deep/strict local signature, pluginval strictness-5 result, and independent VST3 MIDI render pass. The former M8 bundle remains at `~/Library/Audio/Plug-Ins/VST3/folk park.vst3.backup-20260824T140808Z`. These facts prove the installed artifact identity; they do not prove FL Studio discovery, physical interaction, or audible behavior.

## Roll back

1. Close FL Studio.
2. Run `./scripts/uninstall_user_vst3.sh --execute` to move only the current exact bundle to Trash.
3. Identify the intended timestamped `.backup-*` sibling in `~/Library/Audio/Plug-Ins/VST3`.
4. Verify that backup by passing its full quoted path to `./scripts/verify_user_vst3.sh`.
5. Move that exact verified backup back to `folk park.vst3` and rescan FL Studio.

The scripts deliberately do not guess which backup the producer wants.

## Uninstall without deleting user work

Read-only preview:

```sh
./scripts/uninstall_user_vst3.sh --dry-run
```

Recoverable uninstall:

```sh
./scripts/uninstall_user_vst3.sh --execute
```

The execute action moves only the exact installed VST3 to a unique item in `~/.Trash`. Presets, imported wavetable assets, history, rendered files, and DAW projects remain untouched. There is intentionally no automated “delete all user data” operation.

## Troubleshooting by symptom

### FL Studio does not discover the plug-in

1. Close FL Studio and run `./scripts/verify_user_vst3.sh`.
2. Confirm the reported architecture is exactly `x86_64` and note the hash.
3. Reopen Plugin Manager and perform a rescan. Record scan messages and the exact FL version.
4. Inspect quarantine metadata read-only with `xattr -l "…/folk park.vst3"`; do not remove attributes by assumption.
5. If repair is needed, use the reviewed `--replace` flow so rollback remains possible.

### Editor is blank, stale, or closes

Audio/state belongs to C++, not the WebView. Close/reopen the editor, resize once, then request a complete native snapshot. If Settings is visible, Preview diagnostics and review the text before Copy. Record whether audio continued, the host/build/audio fields, and fixed UI/preset/database counters. No diagnostics are uploaded automatically.

### No sound

Confirm Folk Park is loaded as a generator, MIDI reaches the channel, Master/oscillator levels are audible, and Panic has not left the host sending no notes. Test a C3 note with effects bypassed, then enable one stage at a time. Record sample rate, block size, wrapper format, active voice count, and whether the Standalone behaves differently; listening remains a human result.

### Stuck note or MIDI routing problem

Use Folk Park Panic, then FL Studio Stop/All Notes Off. For direct routing, Folk Park Wrapper output port and the receiving instrument input port must match exactly. Keep FL’s normal “Send All Notes Off when playback stops” behavior enabled. A generated candidate is not routed/exported until explicitly accepted.

### Presets/assets/history are degraded

- Missing imported asset: use the exact relink flow; a wrong SHA-256/size must be rejected.
- History unavailable: presets, project state, acceptance, and audio should continue. Preserve `history.sqlite3` and sidecars for diagnosis.
- Storage path warning: inspect `ls -ld` on the exact Application Support root and children. Symbolic-link roots are rejected intentionally. Do not recursively `chown`, `chmod`, or delete.
- Provider unavailable: expected for 0.1. Offline/manual Jarvis remains the supported complete workflow.

### Project state appears corrupted

Open a copy of the DAW project first. Malformed/oversized state is rejected transactionally before live mutation; it is not repaired in place. Preserve the original project and Application Support data, capture the bounded diagnostics preview, and reproduce with the smallest copied project. Do not clean presets/history while investigating.

## Evidence record

For every human case record: build executable hash, FL Studio/macOS versions, sample rate/block size, exact steps, expected result, actual result, screenshot/log path, workaround (if any), and whether the original project/user data remained unchanged. Only Hermann may convert a matrix row from `HUMAN RUN REQUIRED` to passed.
