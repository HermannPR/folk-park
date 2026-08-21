#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
build_type="${1:-release}"
case "${build_type}" in
  debug) artefact_configuration="Debug" ;;
  release) artefact_configuration="Release" ;;
  *)
    print -u2 "Usage: $0 [debug|release]"
    exit 1
    ;;
esac

source_bundle="${project_root}/build/macos-x86_64-${build_type}/FolkPark_artefacts/${artefact_configuration}/VST3/folk park.vst3"
user_name="$(id -un)"
user_dir="$(dscl . -read "/Users/${user_name}" NFSHomeDirectory | awk '{print $2}')"
destination_dir="${user_dir}/Library/Audio/Plug-Ins/VST3"

if [[ ! -d "${source_bundle}" ]]; then
  print -u2 "Built VST3 not found: ${source_bundle}"
  exit 1
fi

mkdir -p "${destination_dir}"
ditto "${source_bundle}" "${destination_dir}/folk park.vst3"
print "Installed to ${destination_dir}/folk park.vst3"
