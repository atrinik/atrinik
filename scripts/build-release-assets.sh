#!/usr/bin/env bash
set -euo pipefail

if (($# != 1)); then
    echo "usage: $0 VERSION" >&2
    exit 2
fi

version=$1
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+([+-][0-9A-Za-z.-]+)?$ ]]; then
    echo "invalid release version: $version" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repository=$(cd -- "$script_dir/.." && pwd)
output=$repository/build/release

if [[ -L $output || (-e $output && ! -d $output) ]]; then
    echo "refusing unsafe release output: $output" >&2
    exit 1
fi
mkdir -p -- "$output"
find "$output" -maxdepth 1 \( -type f -o -type l \) \
    \( -name 'atrinik-workspace-*.json' -o -name SHA256SUMS \) -delete
cp -- "$repository/components.json" "$output/atrinik-workspace-$version.json"
(
    cd -- "$output"
    sha256sum -- "atrinik-workspace-$version.json" >SHA256SUMS
)
