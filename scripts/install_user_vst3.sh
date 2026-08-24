#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
build_type="${1:-release}"
operation="${2:-}"
case "${build_type}" in
  debug) artefact_configuration="Debug" ;;
  release) artefact_configuration="Release" ;;
  *)
    print -u2 "Usage: $0 [debug|release]"
    exit 1
    ;;
esac
case "${operation}" in
  ""|--replace|--dry-run) ;;
  *)
    print -u2 "Usage: $0 [debug|release] [--replace|--dry-run]"
    exit 1
    ;;
esac

source_bundle="${project_root}/build/macos-x86_64-${build_type}/FolkPark_artefacts/${artefact_configuration}/VST3/folk park.vst3"
user_name="$(id -un)"
user_dir="$(dscl . -read "/Users/${user_name}" NFSHomeDirectory | awk '{print $2}')"
destination_dir="${user_dir}/Library/Audio/Plug-Ins/VST3"
destination_bundle="${destination_dir}/folk park.vst3"
verify_script="${project_root}/scripts/verify_user_vst3.sh"

if [[ ! -d "${source_bundle}" || -L "${source_bundle}" ]]; then
    print -u2 "Built VST3 not found: ${source_bundle}"
    exit 1
fi
"${verify_script}" "${source_bundle}"

if [[ "${operation}" == "--dry-run" ]]; then
    print "DRY RUN: verified source ${source_bundle}"
    if [[ -e "${destination_bundle}" || -L "${destination_bundle}" ]]; then
        print "DRY RUN: existing bundle requires --replace and will be moved to a timestamped .backup-* sibling"
    else
        print "DRY RUN: destination is empty"
    fi
    print "DRY RUN: would install to ${destination_bundle} and verify exact executable hash parity"
    exit 0
fi

if [[ ( -e "${destination_bundle}" || -L "${destination_bundle}" )
      && "${operation}" != "--replace" ]]; then
    print -u2 "Existing VST3 preserved: ${destination_bundle}"
    print -u2 "Re-run with --replace only after reviewing the source; a rollback copy will be kept."
    exit 1
fi

mkdir -p "${destination_dir}"
timestamp="$(date -u '+%Y%m%dT%H%M%SZ')"
backup_bundle=""
if [[ -e "${destination_bundle}" || -L "${destination_bundle}" ]]; then
    backup_bundle="${destination_bundle}.backup-${timestamp}"
    if [[ -e "${backup_bundle}" || -L "${backup_bundle}" ]]; then
        backup_bundle="${backup_bundle}-$$"
    fi
    mv "${destination_bundle}" "${backup_bundle}"
    print "Preserved previous bundle at ${backup_bundle}"
fi

if ! ditto "${source_bundle}" "${destination_bundle}"; then
    print -u2 "Install copy failed. No previous bundle was deleted."
    if [[ -e "${destination_bundle}" || -L "${destination_bundle}" ]]; then
        partial_bundle="${destination_bundle}.failed-copy-${timestamp}"
        if [[ -e "${partial_bundle}" || -L "${partial_bundle}" ]]; then
            partial_bundle="${partial_bundle}-$$"
        fi
        mv "${destination_bundle}" "${partial_bundle}"
        print -u2 "Retained partial copy at ${partial_bundle}"
    fi
    if [[ -n "${backup_bundle}" ]]; then
        mv "${backup_bundle}" "${destination_bundle}"
        print -u2 "Restored previous bundle after copy failure."
    fi
    exit 1
fi

source_executable="${source_bundle}/Contents/MacOS/folk park"
destination_executable="${destination_bundle}/Contents/MacOS/folk park"
source_hash="$(shasum -a 256 "${source_executable}" | awk '{print $1}')"
destination_hash="$(shasum -a 256 "${destination_executable}" | awk '{print $1}')"
if [[ "${source_hash}" != "${destination_hash}" ]] || ! "${verify_script}" "${destination_bundle}"; then
    failed_bundle="${destination_bundle}.failed-${timestamp}"
    mv "${destination_bundle}" "${failed_bundle}"
    print -u2 "Installed verification failed; retained failed copy at ${failed_bundle}"
    if [[ -n "${backup_bundle}" ]]; then
        mv "${backup_bundle}" "${destination_bundle}"
        print -u2 "Restored previous bundle after verification failure."
    fi
    exit 1
fi

print "Installed and verified ${destination_bundle}"
print "Executable SHA-256: ${destination_hash}"
if [[ -n "${backup_bundle}" ]]; then
    print "Rollback copy retained: ${backup_bundle}"
fi
