#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build/linux-debug}
TARGET=${1:-all}
JOBS=${JOBS:-$(nproc)}

cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DPACKAGE_TYPE=none
cmake --build "${BUILD_DIR}" --target "${TARGET}" --parallel "${JOBS}"
