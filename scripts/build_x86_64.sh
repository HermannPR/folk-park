#!/bin/zsh
set -euo pipefail

project_root="${0:A:h:h}"
python_user_base="$(python3 -m site --user-base)"
export PATH="${python_user_base}/bin:${PATH}"

cd "${project_root}"
cmake --preset macos-x86_64-debug
cmake --build --preset macos-x86_64-debug
cmake --preset macos-x86_64-release
cmake --build --preset macos-x86_64-release
