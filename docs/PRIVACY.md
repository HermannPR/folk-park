# Privacy and local-data behavior

Release 0.1 is offline-first. The current product contains no real remote AI adapter, analytics SDK, tracker, CDN, remote font, update client, account login, telemetry upload, or automatic crash-report upload.

## Data processed locally

- DAW MIDI, transport, automation, and serialized plug-in state needed to run the instrument.
- Producer-entered Jarvis text, guided answers, and candidate/proposal state. The current deterministic assistant processes these locally; conversation presentation is not stored in history or project state.
- Native presets, user-imported wavetable copies, and accepted composition history under `~/Library/Application Support/folk park`.
- Producer-selected MIDI/WAV exports and transient uniquely named drag MIDI in the system temporary directory.
- Optional future-provider credentials only if a real provider is later selected/configured. The existing native Keychain boundary stores opaque bytes; React cannot read/write them and no credential is configured today.

## Network behavior

The macOS runtime is built with JUCE curl disabled and the UI loads embedded local resources. The UI source contains no runtime HTTP/HTTPS origin. Offline/manual synthesis, composition, presets, history, rendering, and Jarvis remain usable without networking.

A future remote provider is not covered by this statement. It requires a named provider, provider-specific disclosure, exact data-field review, per-request consent, timeout/cancel/offline behavior, and an owner decision before implementation. It may never receive audio, paths, project/preset identity, history, or credentials by default.

## Diagnostics

Diagnostics are opt-in text below 4 KiB. Previewing writes nothing. Copying requires the exact current opaque preview ID and writes only the text the producer just reviewed to the macOS clipboard. Reports contain bounded build/host/audio configuration, fixed subsystem codes, and numeric counters. They exclude credentials, prompts, project/preset names, UUIDs, tags, personal paths/filenames, audio/MIDI contents, clipboard contents, and database rows/messages. Diagnostics are not persisted or transmitted.

## Retention and deletion

- Presets/assets/history persist until the producer explicitly manages or removes them. History uses recoverable soft deletion and explicit retention cleanup from the product.
- Install, repair, and uninstall scripts never remove Application Support data.
- The safe uninstall script moves only the exact VST3 bundle to Trash.
- The editor removes only its own guarded, uniquely named temporary drag MIDI when replaced/destroyed. Producer-selected exports are never automatically removed.
- DAW project retention/deletion is controlled by the DAW and producer.
- There is deliberately no one-click destructive “erase all data” script in the engineering candidate.

## Known boundary

This is an engineering privacy description, not a finalized public privacy notice. Legal identity, contact details, distribution channel, signing/notarization, update behavior, jurisdiction-specific language, and any future remote provider remain owner/legal decisions in `docs/OPEN_DECISIONS.md`.
