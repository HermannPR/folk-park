# M3 verification record

Date: 2026-08-20 (America/Monterrey)

## Automated proof

- Debug CTest: 6/6 passed (native synthesis/import, composition properties, MIDI delivery parity, assistant schema models, real-time allocation, and processor/state/UI).
- Release CTest: 7/7 passed (the same six plus an external-host smoke against the actual built VST3).
- Release Standalone and VST3 binaries are thin `x86_64` Mach-O artifacts.
- The Release VST3 local ad-hoc signature passes `codesign --verify --deep --strict`. The Release Standalone is currently unsigned and is a private engineering artifact, not a distributable package.
- Installed user VST3 binary hash matches the validated Release build and its local ad-hoc signature verifies.
- VST3 binary SHA-256: `08aa6cef56aa9ad61df6eda5418fd5e401bd08ec516bc59e4a1b7e81bc67e75c`.
- pluginval 1.0.4 strictness 5: `SUCCESS`; log SHA-256: `3b0acf903df7b1ab38fb6066a54d7aaa83130f24d471adca51daea5458a952d2`.
- Visual M3 Standalone inspection passed for the M3 identity, composition controls entering the viewport, and accepted-only drag state. Screenshot SHA-256: `f573f456527b560e4fef823a3ef925b01c20e1322086448064c76e962d1476b9`.
- All JSON schema files parse successfully; `git diff --check` passes.

## Composition and delivery behavior covered

- Deterministic four-part generation for the same normalized intent, seed, generator version, and creation metadata.
- All 12 keys across seven supported scales, 7/8 meter, 64-bar input, bounded range/polyphony/event count, canonical ordering, humanization, and malformed intent/clip rejection.
- Functional chord labels and cadence, voice-led harmony, melody rests/leap limits, bass register, monophonic arp lifecycle, density mapping, controlled `More Like This` difference with parent lineage, and bounded `Surprise Me`.
- Candidate/accepted separation: no export, drag, or route before explicit acceptance; a new candidate does not silently replace previously accepted material.
- Multitrack SMF export and reopen parity at PPQ 96, 480, 960, and 1920 with tempo, 7/8 signature, bounded pitch range, humanization, low-PPQ quantization, and explicit note-offs.
- Invalid PPQ, malformed clips, and truncated MIDI are rejected. Test-owned temporary files are verified and removed after use.
- Direct MIDI preserves exact note-on/note-off absolute sample positions across blocks, rejects competing pending publication, and sends tracked note-offs on Stop.
- The processor routes accepted messages to both its synth and host MIDI output. M3 composition state intentionally does not survive project state round trip before M6.
- Measured rendering allocated zero times for 32 audio blocks including synth swaps/crossfade and direct MIDI schedule activation/insertion with a pre-sized host buffer.
- `SoundIntent` and `ParameterProposal` typed validation rejects malformed IDs, unbounded values, duplicate parameter IDs, and any attempt to disable explicit acceptance.

## Human or later-milestone work

- Physical interaction with Generate, preview, More Like This, Surprise Me, Accept, drag, save chooser, direct Route, and Stop in the Standalone is not yet recorded as a human pass.
- FL Studio MIDI drag and imported-note parity are not yet run.
- FL Studio Wrapper output-port routing to a second instrument, timing, note-offs, Stop/panic, and save-close-reopen behavior are not yet run.
- Direct playback begins on the next callback at clip tempo. Host transport start/reposition/loop synchronization is not claimed for M3.
- Accepted composition/history persistence is M6. Interactive piano-roll editing and the production interface are M4. Isolated WAV preview is M5.
- M3 only defines the strict guided-sound schema/model foundation. The conversational walkthrough, offline/provider intelligence, reversible A/B audition, and parameter proposal application are M7.
- The optional external Steinberg validator remains unavailable; pluginval records that subtest as skipped.
- Distribution licensing, final identity, signing/notarization, privacy, and asset-rights gates remain open.
