#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: $0 [--check|--write]" >&2
}

mode="${1:---write}"
case "${mode}" in
--check)
    format_args=(--dry-run --Werror)
    ;;
--write)
    format_args=(-i)
    ;;
*)
    usage
    exit 2
    ;;
esac

if ! command -v git >/dev/null 2>&1; then
    echo "git not found" >&2
    exit 1
fi

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found" >&2
    exit 1
fi

repo_root=$(git rev-parse --show-toplevel)
cd "${repo_root}"

# Work only on tracked sources. clang-format applies the repository's
# .clang-format-ignore rules for generated and bundled third-party files.
git ls-files -z -- \
    '*.c' '*.h' '*.cc' '*.cpp' '*.cxx' '*.hpp' \
    | xargs -0 -r clang-format "${format_args[@]}"
