#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
skip_sync=false
if [[ ${1:-} == --skip-sync ]]; then
  skip_sync=true
  shift
fi
if [[ $# -ne 0 ]]; then
  echo "usage: $0 [--skip-sync]" >&2
  exit 2
fi

if [[ ${skip_sync} == false ]]; then
  python3 "${root}/scripts/components.py" sync
fi
python3 "${root}/scripts/components.py" verify

components=${root}/build/components
build_root=${root}/build/integration
(cd "${components}/client" && python3 tools/dependencies.py verify)
(cd "${components}/server" && python3 tools/dependencies.py verify)
common_args=(
  -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  -DENABLE_WARNING_ERRORS=ON
  "-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL=${components}/protocol"
  "-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK=${components}/libatrinik"
)
if command -v ccache >/dev/null 2>&1; then
  common_args+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache")
fi

cmake -S "${components}/client" -B "${build_root}/client" "${common_args[@]}"
cmake --build "${build_root}/client" --parallel
ctest --test-dir "${build_root}/client" --output-on-failure

cmake -S "${components}/server" -B "${build_root}/server" "${common_args[@]}"
cmake --build "${build_root}/server" --parallel
ctest --test-dir "${build_root}/server" --output-on-failure
"${components}/server/tools/prepare-runtime.sh" "${build_root}/server"
(cd "${components}/server" && ./server.sh --version)
