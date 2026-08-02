#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${MXE_TOOLCHAIN_FILE:-}" || ! -f "${MXE_TOOLCHAIN_FILE}" ]]; then
    echo "MXE_TOOLCHAIN_FILE does not point to an MXE toolchain." >&2
    echo "Run this script in the 'Atrinik Windows cross-build (MXE)' devcontainer." >&2
    exit 1
fi

if [[ -z "${MXE_RUNTIME_DIR:-}" || ! -d "${MXE_RUNTIME_DIR}" ]]; then
    echo "MXE_RUNTIME_DIR does not point to the shared MXE runtime directory." >&2
    exit 1
fi

JOBS=${JOBS:-$(nproc)}
PACKAGE_DIR=${PACKAGE_DIR:-build/packages}

cmake --preset windows-mxe-release
cmake --build --preset windows-mxe-release --parallel "${JOBS}"
mkdir -p "${PACKAGE_DIR}"
cpack --config build/windows-mxe-release/CPackConfig.cmake \
    -G ZIP \
    -B "${PACKAGE_DIR}"

echo "Windows package written to ${PACKAGE_DIR}"
