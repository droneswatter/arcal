#!/usr/bin/env bash
# Run a clean generated-code build while polling /proc/meminfo to track peak
# memory usage.
# Usage: ./scripts/measure-build.sh [cmake --build args...]
#   e.g. ./scripts/measure-build.sh -j4

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
POLL_INTERVAL=0.5

cd "$ROOT"

PREFIX="${ARCAL_PREFIX:-$HOME/.local}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
CXX_COMPILER="${CMAKE_CXX_COMPILER:-clang++-20}"
C_COMPILER="${CMAKE_C_COMPILER:-clang-20}"
BUILD_DIR="${ARCAL_BUILD_DIR:-build}"

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

run_start=$(date +%s)

if [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning build outputs..."
    cmake --build "$BUILD_DIR" --target clean
fi

echo "Cleaning generated schema output..."
rm -rf include/uci/type src/generated

echo "Regenerating schema headers and externalizer handlers..."
uv run tools/schema_compiler/compiler.py \
    --schema schema/OAC-STD-UCI_V2.5 \
    --out include

echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -G Ninja

cmake -E touch "$BUILD_DIR/schema_compiler.stamp"

echo "Building with: cmake --build $BUILD_DIR $*"
build_start=$(date +%s)
cmake --build "$BUILD_DIR" "$@"
build_end=$(date +%s)

kill $POLL_PID 2>/dev/null

min_available=$(cat "$TMPFILE")
rm -f "$TMPFILE"
trap - EXIT

peak_kb=$(( mem_total - min_available ))
echo ""
echo "Total wall time : $(( build_end - run_start ))s"
echo "Build wall time : $(( build_end - build_start ))s"
echo "Peak mem        : $(( peak_kb / 1024 )) MB  (of $(( mem_total / 1024 )) MB total)"
