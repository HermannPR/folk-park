# Third-party notices for a future package

Public packaging is blocked until the JUCE license choice and other owner decisions are resolved. This file inventories runtime notice obligations; it does not grant authorization to distribute Folk Park.

## JUCE 8.0.13

Pinned commit: `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.

JUCE modules are dual-licensed under AGPLv3 and a commercial JUCE license. The applicable route is unresolved. The authoritative local notice and dependency list is `third_party/JUCE/LICENSE.md` (SHA-256 `2633539bb26d244f0966fbc4df59400ea99bdf575fa96291851c1c3ff3456146`). A future package must include the notices required by the chosen JUCE license and the actually linked JUCE module dependencies.

## VST 3 SDK

The SDK included by the JUCE pin is MIT-licensed, copyright © 2025 Steinberg Media Technologies GmbH. The authoritative local text is `third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` (SHA-256 `d6115b263faa1cdf8c7372d70889c833dde1cec95252e7ee93e4f7d599ec96ca`). VST trademarks/branding must follow Steinberg’s separate usage guidelines.

## Bundled JavaScript runtime

The production WebView bundle contains:

- React 19.2.8 — MIT, copyright Meta Platforms, Inc. and affiliates.
- React DOM 19.2.8 — MIT, copyright Meta Platforms, Inc. and affiliates.
- Scheduler 0.27.0 — MIT, copyright Meta Platforms, Inc. and affiliates.
- Three.js 0.185.1 — MIT, copyright © 2010–2026 three.js authors.

Their exact upstream license files are retained at `ui/node_modules/{react,react-dom,scheduler,three}/LICENSE` after the pinned clean install and are checked by `scripts/audit_release_material.sh`. A future binary package must carry the applicable copyright and permission notices.

MIT permission summary: permission is granted, free of charge, to use, copy, modify, merge, publish, distribute, sublicense, and/or sell the software, provided the copyright and permission notice is included in all copies or substantial portions. The software is provided “as is”, without warranty; consult the exact upstream files above for the controlling text.

## Build-time and system components

TypeScript 7.0.2 (Apache-2.0) and Vite 8.2.2 (MIT) are build-time tools and are not separately shipped as runtime applications. SQLite is linked from the macOS system SDK/runtime and no SQLite source/binary is redistributed by this repository. Their metadata remains inventoried in `LICENSES.md`.
