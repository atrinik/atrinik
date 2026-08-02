#!/bin/sh
set -eu

# Initialize a new host data folder from the image defaults.
if [ ! -e data/.atrinik-initialized ]; then
    cp -R install_data/. data/
    touch data/.atrinik-initialized
fi

# Refresh image-owned HTTP assets (including region maps) while preserving
# player saves and other mutable server data in the mounted folder.
mkdir -p data/http data/tmp
cp -R install_data/http/. data/http/

if [ -n "${ATRINIK_JOIN_PASSWORD:-}" ]; then
    set -- --join_password="${ATRINIK_JOIN_PASSWORD}" "$@"
fi

if [ -n "${ATRINIK_SERVER_HOST:-}" ]; then
    set -- --server_host="${ATRINIK_SERVER_HOST}" "$@"
fi

exec ./atrinik-server \
    --network_stack=ipv4 \
    --no_console \
    --http_url="${ATRINIK_HTTP_URL:-off}" \
    --connectivity_mode="${ATRINIK_CONNECTIVITY_MODE:-direct_only}" \
    --server_public="${ATRINIK_SERVER_PUBLIC:-false}" \
    --port_quic=1730 \
    "$@"
