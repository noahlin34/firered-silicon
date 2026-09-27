#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"

LOG=$(mktemp "${TMPDIR:-/tmp}/openfirered-autoresearch.XXXXXX")
TIMING=$(mktemp "${TMPDIR:-/tmp}/openfirered-autoresearch-time.XXXXXX")
cleanup() {
    rm -f "$LOG" "$TIMING"
}
trap cleanup EXIT HUP INT TERM

if /usr/bin/time -p sh -c 'MAKEFLAGS= make clean && MAKEFLAGS= make' >"$LOG" 2>"$TIMING"; then
    :
else
    status=$?
    printf '%s\n' "Clean build failed (exit $status):" >&2
    sed -n '1,200p' "$LOG" >&2
    sed -n '1,40p' "$TIMING" >&2
    exit "$status"
fi

real_seconds=$(sed -n 's/^real //p' "$TIMING")
case "$real_seconds" in
    ''|*[!0-9.]* )
        printf '%s\n' "Unable to parse /usr/bin/time output:" >&2
        sed -n '1,40p' "$TIMING" >&2
        exit 1
        ;;
esac

printf 'METRIC clean_build_seconds=%s\n' "$real_seconds"
