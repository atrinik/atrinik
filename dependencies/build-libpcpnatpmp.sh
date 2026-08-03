#!/usr/bin/env bash
set -euo pipefail

script_dir=$(dirname -- "$0")
# shellcheck source=/dev/null
. "${script_dir}/libpcpnatpmp.env"

mode=${1:-native}
jobs=${2:-$(nproc)}
install_prefix=${3:-}
case "${mode}" in
    native)
        configure=(cmake)
        ;;
    mingw)
        configure=(x86_64-w64-mingw32.shared-cmake)
        ;;
    *)
        echo "Usage: $0 native|mingw [jobs] [install-prefix]" >&2
        exit 2
        ;;
esac

source_dir=$(mktemp -d /tmp/atrinik-libpcpnatpmp.XXXXXX)
build_dir=$(mktemp -d /tmp/atrinik-libpcpnatpmp-build.XXXXXX)
trap 'rm -rf -- "${source_dir}" "${build_dir}"' EXIT

git clone --quiet "${LIBPCPNATPMP_REPOSITORY}" "${source_dir}"
git -C "${source_dir}" checkout --quiet "${LIBPCPNATPMP_COMMIT}"
test "$(git -C "${source_dir}" rev-parse HEAD)" = "${LIBPCPNATPMP_COMMIT}"

if [[ "${mode}" == mingw ]]; then
    git -C "${source_dir}" apply --unidiff-zero \
        "${script_dir}/../.devcontainer/windows-cross/libpcpnatpmp-mingw.patch"
fi

configure_args=()
install_args=()
if [[ -n "${install_prefix}" ]]; then
    mkdir -p -- "${install_prefix}"
    configure_args+=("-DCMAKE_INSTALL_PREFIX=${install_prefix}")
    install_args+=(--prefix "${install_prefix}")
fi

"${configure[@]}" \
    -S "${source_dir}" \
    -B "${build_dir}" \
    "${configure_args[@]}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_CLI_CLIENT=OFF \
    -DBUILD_SERVER=OFF \
    -DBUILD_TESTS=OFF
cmake --build "${build_dir}" --parallel "${jobs}"
cmake --install "${build_dir}" "${install_args[@]}"
