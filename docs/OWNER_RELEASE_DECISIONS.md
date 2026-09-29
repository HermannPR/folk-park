# Release 0.1 owner decision worksheet

Status: **OWNER INPUT REQUIRED — no distribution choice is authorized by this document**

Reviewed: 2026-08-24 (America/Monterrey)

This worksheet turns the remaining legal, identity, signing, packaging, privacy, performance, and provider gates into explicit choices. It is an engineering decision aid, not legal advice. Confirm licensing and legal questions with the relevant licensor or qualified counsel before distributing binaries.

## Current verified facts

| Fact | Current engineering candidate |
| --- | --- |
| Product/version | `folk park` 0.1.0 |
| Formats/target | Standalone and VST3; thin Intel `x86_64` only |
| CMake company | `Hermann Pauwells` |
| Bundle identifier | `com.folkpark.audio.folkpark` |
| VST3 manufacturer/product codes | `FlPk` / `FkP1` |
| JUCE | 8.0.13 at `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` |
| Runtime network | None; no remote provider, analytics, updater, tracker, CDN, or telemetry |
| VST3 signing | Local ad-hoc signature only; no Team ID |
| Standalone signing | Unsigned |
| Notarization | Not performed |
| Installer/update channel | None |
| Repository/release status | Private engineering candidate; no public binary authorized |

Changing the bundle ID or VST3 manufacturer/product codes after users save DAW projects can affect host identity/recall. Freeze them before external testing unless a deliberate compatibility migration is designed and verified.

## Recommended low-risk sequence

1. Keep the current artifacts private while the FL Studio smoke matrix is run.
2. Decide the legal owner and closed-source/open-source intent.
3. Confirm the JUCE 8 route directly against the owner/revenue/funding/contributor facts.
4. Freeze the public product name, legal seller/developer name, bundle ID, and support contact.
5. For the first external beta, prefer a versioned **Developer ID-signed and notarized ZIP** with checksums and manual user-local VST3 installation. This avoids privileged installer scripts while the install flow is still being proven.
6. Consider a signed/notarized PKG only after the manual beta flow and rollback/uninstall behavior are human-verified.
7. Keep Release 0.1 offline-only; evaluate an opt-in remote provider as a separately reviewed later release.

The sequence above is a recommendation, not an owner decision.

## D1 — Distribution intent

Choose one:

- **Private portfolio/source review:** keep the repository and binaries private; invite selected reviewers to the repository. No external binary conveyance.
- **Closed external beta:** provide binaries to selected testers. Treat this as distribution for licensing, signing, privacy, support, and evidence purposes.
- **Public free release:** anyone can obtain the binary; all distribution gates apply even if no money is charged.
- **Commercial release:** sale/subscription/bundled commercial distribution; all gates apply and commercial terms/support obligations need review.

Recommended current choice: private portfolio/source review until FL smoke evidence and D2–D7 are resolved.

Owner must record: intended audience, free/paid status, target date, and whether anyone outside the owner’s organization will receive a binary.

## D2 — JUCE 8 licensing route

The pinned `third_party/JUCE/LICENSE.md` states that JUCE modules are dual-licensed under AGPLv3 or the commercial JUCE license. The controlling commercial document for this pin is the [JUCE 8 End User Licence Agreement](https://juce.com/legal/juce-8-licence/), not the newer JUCE 9 terms.

The current JUCE 8 EULA lists:

| Route | Published JUCE 8 threshold/price summary | Owner facts still needed |
| --- | --- | --- |
| AGPLv3 | Open-source route; compliance must cover the combined distributed work | Is a complete AGPLv3-compatible source release genuinely intended? |
| Starter | Up to USD 20,000 annual revenue or funding; free perpetual | Exact licensee and applicable prior-12-month revenue/funding under the EULA definition |
| Indie | Up to USD 300,000; USD 40/user/month with one-month minimum or USD 800/user perpetual | Eligibility, owner, users/seats, subscription vs perpetual |
| Pro | No revenue/funding limit; USD 175/user/month with 12-month minimum or USD 3,500/user perpetual | Owner, users/seats, subscription vs perpetual |
| Educational | Free while the stated educational requirements are met; not for commercial/professional/promotional/for-profit activity | Accredited institution/status and non-commercial use |

The EULA makes the Product Owner responsible and defines which Framework Users require seats; compilation/testing-only machines or services do not require additional seats. It also has a separate “Products That Create Products” clause. Do not infer how AI coding-agent assistance is counted or whether Folk Park's end-user MIDI/WAV/preset exports fall inside that clause versus the EULA's static-content/typical-sound-design boundaries. Ask JUCE sales in writing before distribution.

Recommended for a closed-source product: do not distribute until the owner records the licensee, eligibility inputs, route, proof of entitlement, and any JUCE clarification needed. The repository does not decide whether Starter, Indie, or Pro applies.

Owner must record:

- Licensee: individual/sole proprietor or legal entity.
- Closed-source commercial license or AGPLv3 source distribution.
- Applicable revenue/funding for the previous 12 months under the EULA definition.
- Every human Framework User and any written JUCE clarification about agent-assisted development.
- Written JUCE clarification on whether an instrument that exports user-created MIDI, WAV, and preset content needs an alternative agreement under the “Products That Create Products” clause.
- Selected tier, seat count, subscription/perpetual choice, purchase/entitlement evidence location, and renewal/threshold review date.

## D3 — Legal identity and stable product identity

Apple states that individual/sole-proprietor apps use the developer’s personal name, while organization enrollment uses the verified legal entity and requires a D-U-N-S Number. The [Apple membership comparison](https://developer.apple.com/support/compare-memberships/) currently lists the Apple Developer Program at USD 99 per membership year or local equivalent.

Choose and freeze:

- Legal owner/developer name.
- Individual or organization Apple enrollment.
- Public product spelling/capitalization: keep `folk park` or approve a final alternative.
- Reverse-DNS bundle identifier. Current provisional value: `com.folkpark.audio.folkpark`.
- VST3 manufacturer/product codes. Current values: `FlPk` / `FkP1`.
- Copyright line, support email, website, and privacy-notice URL.
- Trademark/name-clearance result. No trademark clearance is claimed by this repository.

Recommended: preserve current IDs for compatibility only if the owner approves the identity; otherwise change them before any external DAW project is created.

## D4 — Apple signing and notarization

Apple’s [Developer ID guidance](https://developer.apple.com/support/developer-id/) says outside-Mac-App-Store distribution uses a Developer ID certificate and notarization. Its [Gatekeeper signing guidance](https://developer.apple.com/developer-id/) explicitly names apps, plug-ins, and installer packages. Apple’s [notarization requirements](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) call for valid Developer ID signatures, hardened runtime, secure timestamps, suitable entitlements, and `notarytool`/current Xcode rather than the retired `altool` flow.

Owner must choose:

- Apple Developer Program account/team and Account Holder.
- `Developer ID Application` identity for the Standalone, VST3, and nested executable code.
- `Developer ID Installer` identity only if a PKG is selected.
- Notarization credential handling outside Git (Keychain profile or approved CI secret store).
- Hardened-runtime/entitlement review and a clean-machine Gatekeeper test.

Current artifacts are not distribution-signed: the VST3 is ad-hoc signed and the Standalone is unsigned. Do not reuse the M8 engineering hashes as final distributed hashes after signing; signing changes the bundles, so the final package needs a new complete evidence manifest.

## D5 — Package and update channel

Choose one initial package:

- **Signed/notarized ZIP (recommended first external beta):** Standalone, VST3, notices, checksums, and manual user-local install instructions; no privilege escalation.
- **Signed/notarized DMG:** polished drag/install presentation, but still needs clear VST3 placement and rollback instructions.
- **Signed/notarized PKG:** simplest end-user installation but introduces installer receipts, privileged/system installation choices, scripts, upgrade semantics, and a larger recovery surface.

Choose one update model:

- **Manual versioned downloads (recommended for 0.1):** release notes and checksums; no updater runtime/network surface.
- **Automatic updater:** defer until a separately reviewed signing, transport, rollback, privacy, and compromise-response design exists.

Record the download host, retention policy for old versions, checksum/manifest publication, support channel, and minimum macOS/Intel compatibility statement.

## D6 — Public privacy notice

The implemented product is offline-first and processes presets, imported assets, history, exports, DAW state, Jarvis text, and diagnostics locally as documented in `docs/PRIVACY.md`. No analytics, account, updater, crash uploader, tracker, or remote model is present.

Before external distribution, approve a public notice containing:

- Legal controller/developer identity and contact.
- Exact local data categories, locations, purposes, retention, and deletion behavior.
- Explicit statement that diagnostics are preview-before-copy and never uploaded automatically.
- Exact no-network/no-telemetry behavior for 0.1.
- Distribution/update website behavior, if that website itself collects data.
- Jurisdiction-specific rights/language reviewed for the intended audience.
- Effective date, version, and change-notification method.

Recommended: publish a concise no-network 0.1 notice derived from `docs/PRIVACY.md`; do not promise behavior for a future provider or updater.

## D7 — Asset, name, and notice approval

Engineering evidence currently shows project-authored code/CSS/geometry, numerically generated built-in wavetables, no bundled factory/user WAV, and pinned third-party notices. Before distribution, the owner must still approve:

- Product name/logo/trademark clearance.
- Copyright ownership or valid assignment for all project-authored code and visual material.
- Every screenshot selected for public use contains no private project, filename, account, or desktop material.
- `LICENSES.md` and `THIRD_PARTY_NOTICES.md` match the final linked/package contents.
- No Serum name, artwork, preset/state, wavetable, installer, or factory asset is shipped or implied as compatible.

## D8 — CPU and supported-host budget

Existing measurements are evidence, not a product budget:

- M2 maximum synth stress on this Intel i9: `0.768709x` realtime at 48 kHz/512.
- M8 Release recovery workload: `0.153058x` realtime at 48 kHz/512.
- The audio callback allocation probe reports zero measured allocations.

After the FL smoke session, choose the supported quality profile and measurable gate:

- Target FL Studio/macOS/Intel machine class.
- Required sample rates and block sizes.
- Default-patch and maximum-supported-patch voice/unison/effect workloads.
- Maximum acceptable host CPU and whether momentary/steady values differ.
- Dropout/xrun, stuck-note, and UI-open/closed acceptance criteria.

Recommended: collect actual FL measurements at 48 kHz and 128/256/512 samples before approving any number. Do not turn a convenient benchmark into a promise.

## D9 — Remote AI provider in 0.1

Choose one:

- **Offline-only 0.1 (recommended):** ship the deterministic text/walkthrough/proposal workflow already implemented; no account, API key, remote disclosure, or service dependency.
- **Later opt-in BYOK provider:** separate milestone with named provider, native Keychain credential entry, exact allowed fields, per-request disclosure/consent, cancellation/timeout, cost/rate limits, provider terms, and updated privacy notice.
- **Developer-funded hosted provider:** separate service/security/business project requiring accounts, abuse controls, server-side secret handling, billing, retention, incident response, and availability commitments.

No remote adapter may be added merely because the Keychain abstraction exists.

## D10 — Compatibility promise

Recommended Release 0.1 statement:

> Imports user-owned WAV audio through Folk Park’s documented conversion flow. It does not import Serum presets or private state and does not ship Serum code, branding, wavetables, or factory assets.

Any broader compatibility must be a later, separately researched and tested decision based on legitimate public specifications and user-owned fixtures.

## Owner reply template

The owner can answer incrementally; unanswered items remain blocked:

```text
D1 distribution: private portfolio / closed beta / public free / commercial
Audience, price, and target date:

D2 JUCE: licensee, open/closed source, prior-12-month EULA revenue/funding,
human Framework Users, intended tier, subscription/perpetual, clarification needed:

D3 identity: individual/organization, legal name, product name, bundle ID,
manufacturer/product codes, copyright, support email/site, trademark status:

D4 Apple: enrolled team? Developer ID Application identity? PKG identity?
approved external credential storage? clean target available?

D5 package/update: ZIP / DMG / PKG; manual / automatic; download host:

D6 privacy: controller/contact, target countries, notice URL/reviewer:

D7 assets: name/logo/code/screenshots/notices approved? exceptions:

D8 performance: target sample rates, buffers, workload, CPU/dropout criteria:

D9 provider: offline-only 0.1 / later BYOK / hosted service:

D10 compatibility statement approved? changes:
```

No answer authorizes credential extraction, certificate export, purchasing, publishing, notarization submission, installer execution, or changing the repository visibility. Each such action still requires its exact reviewed command and scope.
