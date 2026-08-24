#!/bin/zsh
set -euo pipefail

operation="${1:---dry-run}"
if [[ "${operation}" != "--dry-run" && "${operation}" != "--execute" ]]; then
    print -u2 "Usage: $0 [--dry-run|--execute]"
    exit 1
fi

user_name="$(id -un)"
user_dir="$(dscl . -read "/Users/${user_name}" NFSHomeDirectory | awk '{print $2}')"
bundle="${user_dir}/Library/Audio/Plug-Ins/VST3/folk park.vst3"
data_root="${user_dir}/Library/Application Support/folk park"

if [[ ! -e "${bundle}" && ! -L "${bundle}" ]]; then
    print "No installed user VST3 was found at ${bundle}"
    print "User presets/assets/history remain untouched at ${data_root}"
    exit 0
fi

timestamp="$(date -u '+%Y%m%dT%H%M%SZ')"
trash_dir="${user_dir}/.Trash"
trash_bundle="${trash_dir}/folk park.vst3.removed-${timestamp}"
if [[ -e "${trash_bundle}" || -L "${trash_bundle}" ]]; then
    trash_bundle="${trash_bundle}-$$"
fi

if [[ "${operation}" == "--dry-run" ]]; then
    print "DRY RUN: would move only ${bundle}"
    print "DRY RUN: recoverable destination ${trash_bundle}"
    print "DRY RUN: would preserve all user presets/assets/history at ${data_root}"
    exit 0
fi

mkdir -p "${trash_dir}"
mv "${bundle}" "${trash_bundle}"
print "Moved installed VST3 to Trash: ${trash_bundle}"
print "User presets/assets/history were not changed: ${data_root}"
