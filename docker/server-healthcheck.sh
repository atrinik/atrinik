#!/bin/sh
set -eu

heartbeat="data/tmp/server-heartbeat"
test -r "$heartbeat"
kill -0 1

timestamp=$(sed -n '1p' "$heartbeat")
case "$timestamp" in
    ''|*[!0-9]*) exit 1 ;;
esac

now=$(date +%s)
age=$((now - timestamp))
test "$age" -ge 0
test "$age" -le 60
