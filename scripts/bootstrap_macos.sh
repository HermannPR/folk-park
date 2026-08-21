#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
python_user_base="$(python3 -m site --user-base)"
export PATH="${python_user_base}/bin:${PATH}"

if [[ "$(uname -m)" != "x86_64" ]]; then
  print -u2 "folk park 0.1 requires an Intel x86_64 Mac"
  exit 1
fi

for required_tool in git clang cmake ninja node npm; do
  if ! command -v "${required_tool}" >/dev/null 2>&1; then
    print -u2 "Missing tool: ${required_tool}"
    exit 1
  fi
done

if [[ ! -f "${project_root}/third_party/JUCE/CMakeLists.txt" ]]; then
  print -u2 "JUCE submodule is missing; run: git submodule update --init --recursive"
  exit 1
fi

actual_juce_commit="$(git -C "${project_root}/third_party/JUCE" rev-parse HEAD)"
expected_juce_commit="7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
if [[ "${actual_juce_commit}" != "${expected_juce_commit}" ]]; then
  print -u2 "JUCE pin mismatch: ${actual_juce_commit}"
  exit 1
fi

print "Architecture: $(uname -m)"
print "macOS: $(sw_vers -productVersion)"
print "Clang: $(clang --version | head -n 1)"
print "CMake: $(cmake --version | head -n 1)"
print "Ninja: $(ninja --version)"
print "Node: $(node --version)"
print "npm: $(npm --version)"
print "JUCE: ${actual_juce_commit}"
