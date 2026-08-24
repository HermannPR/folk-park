#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
cd "${project_root}"

expected_juce_license="2633539bb26d244f0966fbc4df59400ea99bdf575fa96291851c1c3ff3456146"
expected_vst3_license="d6115b263faa1cdf8c7372d70889c833dde1cec95252e7ee93e4f7d599ec96ca"
actual_juce_license="$(shasum -a 256 third_party/JUCE/LICENSE.md | awk '{print $1}')"
actual_vst3_license="$(shasum -a 256 third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt | awk '{print $1}')"
[[ "${actual_juce_license}" == "${expected_juce_license}" ]]
[[ "${actual_vst3_license}" == "${expected_vst3_license}" ]]

node -e '
const lock = require("./ui/package-lock.json");
const expected = { react: ["19.2.8", "MIT"], "react-dom": ["19.2.8", "MIT"],
  scheduler: ["0.27.0", "MIT"], three: ["0.185.1", "MIT"] };
for (const [name, pair] of Object.entries(expected)) {
  const value = lock.packages[`node_modules/${name}`];
  if (!value || value.version !== pair[0] || value.license !== pair[1])
    throw new Error(`Unexpected bundled dependency metadata for ${name}`);
}
'

for license in react react-dom scheduler three; do
    [[ -f "ui/node_modules/${license}/LICENSE" ]]
done

if git ls-files | rg -i '\.(wav|aif|aiff|mid|midi|mp3|flac|woff2?|ttf|otf|jpg|jpeg|gif|svg)$' \
    | rg -v '^evidence/' >/dev/null; then
    print -u2 "Unexpected tracked runtime media/font asset requires provenance review:"
    git ls-files | rg -i '\.(wav|aif|aiff|mid|midi|mp3|flac|woff2?|ttf|otf|jpg|jpeg|gif|svg)$' \
        | rg -v '^evidence/' >&2
    exit 1
fi

if rg -n 'https?://' ui/src --glob '!*.test.ts' >/dev/null; then
    print -u2 "Runtime UI source contains an external URL"
    rg -n 'https?://' ui/src --glob '!*.test.ts' >&2
    exit 1
fi

print "Release material audit passed"
print "JUCE license file SHA-256: ${actual_juce_license} (distribution choice unresolved)"
print "VST3 SDK license file SHA-256: ${actual_vst3_license} (MIT)"
print "Bundled UI runtimes: React 19.2.8, React DOM 19.2.8, Scheduler 0.27.0, Three.js 0.185.1 (MIT)"
print "No tracked runtime media/font assets outside evidence; no external runtime UI URLs"
