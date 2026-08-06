#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 || ! $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "usage: $0 VERSION OUTPUT_DIRECTORY" >&2
  exit 2
fi

version=$1
output=$2
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

python3 "${root}/scripts/components.py" validate
mkdir -p "${output}"
cp "${root}/components.lock.json" \
  "${output}/atrinik-components-${version}.lock.json"
(cd "${output}" && sha256sum "atrinik-components-${version}.lock.json" >SHA256SUMS)
