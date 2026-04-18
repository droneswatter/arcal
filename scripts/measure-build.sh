#!/usr/bin/env bash
# Run the build while polling /proc/meminfo to track peak memory usage.
# Usage: ./scripts/measure-build.sh [ninja args...]
#   e.g. ./scripts/measure-build.sh -j4

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
POLL_INTERVAL=0.5

mem_total=$(awk '/MemTotal/{print $2}' /proc/meminfo)
TMPFILE=$(mktemp)
echo "$mem_total" > "$TMPFILE"

poll() {
    while true; do
        avail=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
        prev=$(cat "$TMPFILE")
        (( avail < prev )) && echo "$avail" > "$TMPFILE"
        sleep "$POLL_INTERVAL"
    done
}

poll &
POLL_PID=$!
trap "kill $POLL_PID 2>/dev/null; rm -f $TMPFILE; exit" INT TERM EXIT

echo "Building with: cmake --build $ROOT/build $*"
start=$(date +%s)
cmake --build "$ROOT/build" "$@"
end=$(date +%s)

kill $POLL_PID 2>/dev/null

min_available=$(cat "$TMPFILE")
rm -f "$TMPFILE"
trap - EXIT

peak_kb=$(( mem_total - min_available ))
echo ""
echo "Wall time : $(( end - start ))s"
echo "Peak mem  : $(( peak_kb / 1024 )) MB  (of $(( mem_total / 1024 )) MB total)"
