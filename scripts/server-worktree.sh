#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
usage: server-worktree.sh prepare --worktree PATH [--build-dir PATH]
       server-worktree.sh run --worktree PATH [--build-dir PATH] [-- SERVER_ARGS...]

Prepare or run an atrinik/server Git worktree while keeping mutable server data
in one shared directory. Set ATRINIK_SHARED_SERVER_DATA to override the default
shared directory.
EOF
}

if [[ $# -eq 0 ]]; then
  usage >&2
  exit 2
fi

action=$1
shift
server_worktree=
build_directory=build/linux-debug
server_args=()
while [[ $# -gt 0 ]]; do
  case $1 in
    --worktree)
      [[ $# -ge 2 ]] || { usage >&2; exit 2; }
      server_worktree=$2
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || { usage >&2; exit 2; }
      build_directory=$2
      shift 2
      ;;
    --)
      shift
      server_args=("$@")
      break
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ${action} != prepare && ${action} != run ]]; then
  echo "unknown action: ${action}" >&2
  usage >&2
  exit 2
fi
if [[ -z ${server_worktree} ]]; then
  echo "--worktree is required" >&2
  usage >&2
  exit 2
fi
if [[ ${action} == prepare && ${#server_args[@]} -ne 0 ]]; then
  echo "server arguments are only valid with the run action" >&2
  exit 2
fi

script_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
server_worktree=$(cd "${server_worktree}" && pwd -P)
for required_path in install_data server.sh tools/prepare-runtime.sh; do
  if [[ ! -e ${server_worktree}/${required_path} ]]; then
    echo "not an atrinik/server worktree; missing ${required_path}: ${server_worktree}" >&2
    exit 1
  fi
done

if [[ -n ${ATRINIK_SHARED_SERVER_DATA:-} ]]; then
  if [[ ${ATRINIK_SHARED_SERVER_DATA} != /* ]]; then
    echo "ATRINIK_SHARED_SERVER_DATA must be an absolute path" >&2
    exit 1
  fi
  configured_data=${ATRINIK_SHARED_SERVER_DATA%/}
else
  git_common_dir=$(git -C "${script_root}" rev-parse --path-format=absolute --git-common-dir)
  configured_data=$(dirname "${git_common_dir}")/build/shared/server-data
fi
if [[ -z ${configured_data} || ${configured_data} == / ]]; then
  echo "refusing unsafe shared server data path: ${configured_data:-<empty>}" >&2
  exit 1
fi

data_link=${server_worktree}/data
if [[ -e ${data_link} && ! -L ${data_link} ]]; then
  echo "refusing to replace existing server worktree data: ${data_link}" >&2
  echo "Back it up and move it to ${configured_data}, then rerun this command." >&2
  exit 1
fi

configured_parent=$(dirname "${configured_data}")
configured_name=$(basename "${configured_data}")
if [[ ${configured_name} == . || ${configured_name} == .. ]]; then
  echo "refusing unsafe shared server data path: ${configured_data}" >&2
  exit 1
fi
mkdir -p "${configured_parent}"
configured_parent=$(cd "${configured_parent}" && pwd -P)
configured_data=${configured_parent}/${configured_name}

case ${configured_data}/ in
  "${server_worktree}/"*)
    echo "shared server data must be outside the server worktree: ${configured_data}" >&2
    exit 1
    ;;
esac
if [[ -L ${data_link} ]]; then
  linked_data=$(readlink -m "${data_link}")
  intended_data=$(readlink -m "${configured_data}")
  if [[ ${linked_data} != "${intended_data}" ]]; then
    echo "server worktree data points elsewhere: ${data_link} -> ${linked_data}" >&2
    exit 1
  fi
fi

if ! command -v flock >/dev/null 2>&1; then
  echo "flock is required to protect shared server data" >&2
  exit 1
fi
lock_file=${configured_data}.lock
exec {lock_fd}>"${lock_file}"
if ! flock --exclusive --nonblock "${lock_fd}"; then
  echo "another server worktree is already using ${configured_data}" >&2
  exit 1
fi

staging=
cleanup() {
  if [[ -n ${staging} && -d ${staging} ]]; then
    rm -rf -- "${staging}"
  fi
}
trap cleanup EXIT
if [[ -e ${configured_data} && ! -d ${configured_data} ]]; then
  echo "shared server data path is not a directory: ${configured_data}" >&2
  exit 1
fi
if [[ ! -e ${configured_data} ]]; then
  staging=$(mktemp -d "${configured_parent}/.${configured_name}.init.XXXXXX")
  cp -a "${server_worktree}/install_data/." "${staging}/"
  mkdir -p "${staging}/tmp"
  if [[ -e ${configured_data} ]]; then
    echo "shared server data appeared during initialization: ${configured_data}" >&2
    exit 1
  fi
  mv "${staging}" "${configured_data}"
  staging=
fi
shared_data=$(cd "${configured_data}" && pwd -P)
case ${shared_data}/ in
  "${server_worktree}/"*)
    echo "shared server data must be outside the server worktree: ${shared_data}" >&2
    exit 1
    ;;
esac
for required_data_file in bans motd; do
  if [[ ! -f ${shared_data}/${required_data_file} ]]; then
    echo "shared path does not look like Atrinik server data; missing file ${required_data_file}: ${shared_data}" >&2
    exit 1
  fi
done
for required_data_directory in keys unique-items; do
  if [[ ! -d ${shared_data}/${required_data_directory} ]]; then
    echo "shared path does not look like Atrinik server data; missing directory ${required_data_directory}: ${shared_data}" >&2
    exit 1
  fi
done
mkdir -p "${shared_data}/tmp"

if [[ -L ${data_link} ]]; then
  linked_data=$(readlink -f "${data_link}" || true)
  if [[ ${linked_data} != "${shared_data}" ]]; then
    echo "server worktree data points elsewhere: ${data_link} -> ${linked_data}" >&2
    exit 1
  fi
elif [[ -e ${data_link} ]]; then
  echo "refusing to replace existing server worktree data: ${data_link}" >&2
  echo "Back it up and move it to ${shared_data}, then rerun this command." >&2
  exit 1
else
  ln -s "${shared_data}" "${data_link}"
fi

"${server_worktree}/tools/prepare-runtime.sh" "${build_directory}"
printf 'Shared server data: %s\n' "${shared_data}"

if [[ ${action} == prepare ]]; then
  exit 0
fi
(
  cd "${server_worktree}"
  exec ./server.sh "${server_args[@]}"
)
