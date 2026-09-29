#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
scripts=(
  "${project_root}/scripts/install_user_vst3.sh"
  "${project_root}/scripts/verify_user_vst3.sh"
  "${project_root}/scripts/uninstall_user_vst3.sh"
  "${project_root}/scripts/audit_release_material.sh"
)

for script in "${scripts[@]}"; do
    /bin/zsh -n "${script}"
    [[ -x "${script}" ]]
done

if rg -n '(^|[[:space:]])rm([[:space:]]|$)' "${scripts[@]}" >/dev/null; then
    print -u2 "Support scripts must use recoverable moves, never rm"
    exit 1
fi

rg -q -- '--replace' "${project_root}/scripts/install_user_vst3.sh"
rg -q 'backup-' "${project_root}/scripts/install_user_vst3.sh"
rg -q 'shasum -a 256' "${project_root}/scripts/install_user_vst3.sh"
rg -q '\.Trash' "${project_root}/scripts/uninstall_user_vst3.sh"
rg -q 'Application Support/folk park' "${project_root}/scripts/uninstall_user_vst3.sh"
rg -q 'x86_64' "${project_root}/scripts/verify_user_vst3.sh"

"${project_root}/scripts/audit_release_material.sh"
print "Support script contract tests passed"
