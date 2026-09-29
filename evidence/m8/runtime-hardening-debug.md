# M8 Debug runtime-hardening evidence

Recorded 2026-08-24 in America/Monterrey from `feat/m8-release-hardening` after diagnostics commit `0b1c07e`.

## Environment

- Mac: MacBook Pro `MacBookPro15,1`
- CPU: 8-core Intel Core i9-9880H at 2.30 GHz
- Memory: 16 GB
- OS: macOS 15.7.9 (`24G830`)
- Architecture: `x86_64`
- Build type: Debug

No serial number, hardware UUID, personal path, project/preset identity, credential, or provider content is retained in this evidence.

## Final extended run

Command:

```sh
FOLK_PARK_RUNTIME_SECONDS=120 \
  build-debug/FolkParkRuntimeHardeningTests_artefacts/Debug/FolkParkRuntimeHardeningTests
```

Exact result:

```text
JUCE v8.0.13
M8 runtime evidence: 11250 blocks, 120 simulated seconds at 48 kHz/512, four notes, 2x2 unison, all six effects enabled, 1 panic checks, elapsed=87280.9 ms, ratio=0.727341x realtime
PASS: M8 finite long-run, panic, preview overflow, and direct-MIDI Stop recovery
```

The run checks every rendered sample for finiteness, cycles four MIDI notes, executes panic during rendering, waits for bounded release to zero active voices, proves held-key repeat idempotence, forces preview-queue overflow/release recovery, and proves direct-MIDI Stop emits the tracked note-off and clears pending/playing state.

The ratio is an observation, not an approved release budget. It must not be generalized to FL Studio, a physical audio device, another project, or another machine.

## Routine Debug gate

The default test simulates 12 seconds to keep routine CTest practical. A focused final run reported:

```text
M8 runtime evidence: 1125 blocks, 12 simulated seconds at 48 kHz/512, four notes, 2x2 unison, all six effects enabled, 1 panic checks, elapsed=6297.36 ms, ratio=0.52478x realtime
PASS: M8 finite long-run, panic, preview overflow, and direct-MIDI Stop recovery
```

Repeated construction/destruction of the bundled editor also passed inside `FolkParkPluginStateAndUiTests` while a host-held note and audio callback remained active. The complete Debug CTest gate passed 14/14.

CTest's aggregate duration reporting was inconsistent with both the test's monotonic internal timer and the command runner's observed duration during this session. Therefore CTest is retained as functional pass/fail evidence, while performance observations use the runtime test's explicit internal timer and fully printed configuration.

All FL Studio, audible-quality, physical MIDI/audio-device, and Release parity checks remain human or later-gate work. This file does not claim them.
