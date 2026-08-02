#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=${BUILD_DIR:-${ROOT_DIR}/build/server-debug}
JOBS=${JOBS:-$(nproc)}

if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPACKAGE_TYPE=none \
    -DBUILD_CLIENT=OFF \
    -DBUILD_SERVER=ON
cmake --build "${BUILD_DIR}" --target atrinik-server --parallel "${JOBS}"

mkdir -p "${ROOT_DIR}/server/lib"
python3 "${ROOT_DIR}/tools/collect.py" \
    --dir "${ROOT_DIR}" \
    --out "${ROOT_DIR}/server/lib"

if [[ ! -d "${ROOT_DIR}/server/data" ]]; then
    cp -R "${ROOT_DIR}/server/install_data" "${ROOT_DIR}/server/data"
fi
mkdir -p "${ROOT_DIR}/server/data/tmp"

(
    cd "${ROOT_DIR}/server"
    "${BUILD_DIR}/server/atrinik-server" --worldmaker
)

map_count=$(find "${ROOT_DIR}/server/data/http/client-maps" \
    -maxdepth 1 -type f -name '*.png' | wc -l)
echo "Generated ${map_count} region maps in server/data/http/client-maps."
