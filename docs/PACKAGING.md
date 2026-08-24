# Private engineering packaging notes

No public installer/package is authorized. The current deliverables are the Release Standalone app and VST3 bundle produced by the pinned CMake preset after the embedded UI build.

## Candidate contents

- `folk park.app` — Intel `x86_64` Standalone engineering application.
- `folk park.vst3` — Intel `x86_64` VST3 instrument/generator.
- `README.md`, `docs/SUPPORT_PLAYBOOK.md`, `docs/PRIVACY.md`, `LICENSES.md`, and `THIRD_PARTY_NOTICES.md`.
- A verification manifest generated from the final retained gate with version, architecture, executable/bundle hashes, signature status, build commit, macOS/toolchain, validator result, and known limitations.

User presets, imported assets, history databases, projects, prompts, credentials, temporary MIDI, screenshots containing personal material, build directories, source `.env` files, certificates, and notarization credentials must never enter a package.

## Current signature truth

Local CMake products use an ad-hoc engineering signature that can be checked deeply/strictly. That is not Developer ID signing and is not notarization. GateKeeper distribution behavior is not claimed. Final identity, bundle identifier, certificate, hardened runtime/entitlements, notarization profile, installer format, update channel, privacy notice, and JUCE license must be explicitly resolved first.

## Required order once decisions exist

1. Rebuild from the exact private commit with the pinned UI lockfile and JUCE commit.
2. Run clean UI, Debug, Release, packaged-VST3 smoke, extended runtime, pluginval, schema/security/license/asset scans.
3. Freeze version and final identity; verify no compatibility surface changed after validation.
4. Sign nested code and outer bundles with the approved identity/options.
5. Verify signatures, submit/notarize using an external credential profile, staple if applicable, and verify again on a clean target.
6. Assemble notices/support/privacy/manifest without user/runtime data.
7. Test install, discovery, repair, rollback, and uninstall on the target Intel Mac and complete the FL Studio matrix.
8. Hash the final distributed bytes and retain exact evidence.

No automation may embed a signing certificate, password, Apple credential, notarization profile secret, API key, or provider token in Git.
