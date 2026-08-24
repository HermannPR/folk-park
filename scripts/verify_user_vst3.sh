#!/bin/zsh
set -euo pipefail

user_name="$(id -un)"
user_dir="$(dscl . -read "/Users/${user_name}" NFSHomeDirectory | awk '{print $2}')"
bundle="${1:-${user_dir}/Library/Audio/Plug-Ins/VST3/folk park.vst3}"
executable="${bundle}/Contents/MacOS/folk park"

if [[ ! -d "${bundle}" || -L "${bundle}" ]]; then
    print -u2 "VST3 bundle is missing or is an unsupported symbolic link: ${bundle}"
    exit 1
fi
if [[ ! -f "${executable}" || -L "${executable}" ]]; then
    print -u2 "VST3 executable is missing or is an unsupported symbolic link: ${executable}"
    exit 1
fi

architectures="$(lipo -archs "${executable}")"
if [[ "${architectures}" != "x86_64" ]]; then
    print -u2 "Expected one thin x86_64 executable; found: ${architectures}"
    exit 1
fi

codesign --verify --deep --strict "${bundle}"
executable_hash="$(shasum -a 256 "${executable}" | awk '{print $1}')"
print "Verified bundle: ${bundle}"
print "Architecture: ${architectures}"
print "Executable SHA-256: ${executable_hash}"
print "Size: $(du -sh "${bundle}" | awk '{print $1}')"
