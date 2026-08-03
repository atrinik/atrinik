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
SERVER_BUILD_DIR=build/windows-mxe-server-release
SERVER_PACKAGE_ROOT=${SERVER_BUILD_DIR}/package-root

case "${SERVER_PACKAGE_ROOT}" in
    build/windows-mxe-server-release/package-root) ;;
    *)
        echo "Refusing to replace unexpected package staging path: ${SERVER_PACKAGE_ROOT}" >&2
        exit 1
        ;;
esac

cmake -E remove_directory "${SERVER_PACKAGE_ROOT}"
cmake -E make_directory "${SERVER_PACKAGE_ROOT}"
cmake -E copy_directory arch "${SERVER_PACKAGE_ROOT}/arch"
cmake -E copy_directory maps "${SERVER_PACKAGE_ROOT}/maps"
cmake -E make_directory "${SERVER_PACKAGE_ROOT}/lib"
python3 tools/collect.py \
    --dir "${SERVER_PACKAGE_ROOT}" \
    --out "${SERVER_PACKAGE_ROOT}/lib"

cmake --preset windows-mxe-release
cmake --build --preset windows-mxe-release --parallel "${JOBS}"
cmake --preset windows-mxe-server-release
cmake --build --preset windows-mxe-server-release --parallel "${JOBS}"
mkdir -p "${PACKAGE_DIR}"
cpack --config build/windows-mxe-release/CPackConfig.cmake \
    -G ZIP \
    -B "${PACKAGE_DIR}"
cpack --config build/windows-mxe-server-release/CPackConfig.cmake \
    -G ZIP \
    -B "${PACKAGE_DIR}"

echo "Windows client and server packages written to ${PACKAGE_DIR}"
