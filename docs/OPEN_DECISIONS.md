# Open decisions

These do not block private engineering. Items marked **distribution gate** must be resolved before giving binaries to external testers, publishing, or selling them. The complete facts, options, recommendations, official-source links, and reply template are in `docs/OWNER_RELEASE_DECISIONS.md`; this file remains the concise status index.

1. **Distribution gate:** Choose the legally applicable JUCE license for this owner, funding/revenue level, and intended commercial use. No binary will be published until resolved.
2. **Distribution gate:** Confirm legal developer/company name and final bundle identifier.
3. **Distribution gate:** Decide code-signing identity, notarization, installer, update channel, privacy notice, and final asset rights.
4. Approve a CPU budget after M2 records real x86_64 benchmarks; do not invent a target before evidence.
5. Decide whether a remote AI provider belongs in 0.1 after offline Jarvis is complete and Keychain-backed credentials exist.
6. Define any future Serum compatibility only from legitimate public specifications and user-owned fixtures. Release 0.1 supports user-owned WAV wavetable import, not Serum preset state.

Current recommended path, not yet owner-approved: private source/portfolio review → FL Studio smoke evidence → confirm JUCE 8 route and identity → signed/notarized ZIP for a limited external beta → consider a PKG only after install/rollback evidence. Keep 0.1 offline-only and manual-update unless the owner explicitly selects a separately reviewed provider/updater scope.
