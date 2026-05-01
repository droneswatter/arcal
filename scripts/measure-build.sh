#!/usr/bin/env bash
# Run a clean generated-code build while polling /proc/meminfo to track peak
# memory usage.
# Usage: ./scripts/measure-build.sh full|<subset-config.json> [cmake --build args...]
#   e.g. ./scripts/measure-build.sh config/subsets/arcal-busmon-cert.json -j8

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
POLL_INTERVAL=0.5

source "$SCRIPT_DIR/build-support.sh"

cd "$ROOT"

usage() {
    echo "usage: $0 full|<subset-config.json> [cmake --build args...]" >&2
    exit 2
}

if [[ $# -lt 1 ]]; then
    usage
fi

variant="$1"
shift

PREFIX="${ARCAL_PREFIX:-$HOME/.local}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
CXX_COMPILER="${CMAKE_CXX_COMPILER:-clang++-20}"
C_COMPILER="${CMAKE_C_COMPILER:-clang-20}"
UNITY_BATCH_SIZE="${ARCAL_UNITY_BATCH_SIZE:-100}"
BUILD_ARGS=("$@")
if [[ ${#BUILD_ARGS[@]} -eq 0 ]]; then
    BUILD_ARGS=(-j "${CMAKE_BUILD_PARALLEL_LEVEL:-8}")
fi

# Resolve variant → build dir, subset config, targets to build
if [[ "$variant" == "full" ]]; then
    BUILD_DIR="${ARCAL_BUILD_DIR:-build}"
    SUBSET_CONFIGS=""
    BUILD_TARGETS=()
else
    config_path="$variant"
    [[ ! -f "$config_path" ]] && config_path="$ROOT/$variant"
    if [[ ! -f "$config_path" ]]; then
        echo "error: not a file: $variant" >&2
        usage
    fi
    suffix=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['cal_name_suffix'])" "$config_path")
    BUILD_DIR="${ARCAL_BUILD_DIR:-build-${suffix}}"
    SUBSET_CONFIGS="$config_path"
    _target=$(arcal_target_name_for_config "$config_path")
    BUILD_TARGETS=("$_target" "${_target}_externalizer_json")
fi

SUMMARY_FILE="${ARCAL_MEASURE_SUMMARY_FILE:-$BUILD_DIR/measure-build-summary.txt}"

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

arcal_suspend_cpptools

poll &
POLL_PID=$!

cleanup() {
    local status=$?
    arcal_resume_cpptools
    kill $POLL_PID 2>/dev/null || true
    rm -f "$TMPFILE"
    exit "$status"
}

trap cleanup INT TERM EXIT

run_start=$(date +%s)

if [[ -d "$BUILD_DIR" ]]; then
    if [[ -f "$BUILD_DIR/build.ninja" || -f "$BUILD_DIR/Makefile" ]]; then
        echo "Cleaning build outputs..."
        cmake --build "$BUILD_DIR" --target clean
    else
        echo "Removing unconfigured build directory..."
        rm -rf "$BUILD_DIR"
    fi
fi

echo "Cleaning generated schema output..."
rm -rf include/uci/type src/generated "$BUILD_DIR"

if [[ -z "$SUBSET_CONFIGS" ]]; then
    echo "Regenerating schema headers and externalizer handlers..."
    uv run tools/schema_compiler/compiler.py \
        --schema schema/OAC-STD-UCI_V2.5 \
        --out include
else
    echo "Subset benchmark mode: skipping eager full-schema regeneration."
fi

echo "Configuring CMake..."
cmake_args=(
    -S . -B "$BUILD_DIR"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_PREFIX_PATH="$PREFIX"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DARCAL_SUBSET_CONFIGS="$SUBSET_CONFIGS"
    -DARCAL_UNITY_BATCH_SIZE="$UNITY_BATCH_SIZE"
    -G Ninja
)

if [[ -n "$SUBSET_CONFIGS" ]]; then
    cmake_args+=(
        -DARCAL_BUILD_TESTS=OFF
        -DARCAL_BUILD_E2E_TESTS=OFF
        -DARCAL_BUILD_INSTALL_TESTS=OFF
        -DARCAL_BUILD_EXAMPLES=OFF
        -DARCAL_BUILD_LACAL=OFF
    )
fi

cmake "${cmake_args[@]}"

if [[ -z "$SUBSET_CONFIGS" ]]; then
    cmake -E touch "$BUILD_DIR/schema_compiler.stamp"
fi

if [[ ${#BUILD_TARGETS[@]} -gt 0 ]]; then
    echo "Subset benchmark targets: ${BUILD_TARGETS[*]}"
    echo "Building with: cmake --build $BUILD_DIR --target ${BUILD_TARGETS[*]} ${BUILD_ARGS[*]}"
else
    echo "Building with: cmake --build $BUILD_DIR ${BUILD_ARGS[*]}"
fi
build_start=$(date +%s)
if [[ ${#BUILD_TARGETS[@]} -gt 0 ]]; then
    cmake --build "$BUILD_DIR" --target "${BUILD_TARGETS[@]}" "${BUILD_ARGS[@]}"
else
    cmake --build "$BUILD_DIR" "${BUILD_ARGS[@]}"
fi
build_end=$(date +%s)

kill $POLL_PID 2>/dev/null
arcal_resume_cpptools

min_available=$(cat "$TMPFILE")
rm -f "$TMPFILE"
trap - EXIT

peak_kb=$(( mem_total - min_available ))
mkdir -p "$(dirname "$SUMMARY_FILE")"
cat > "$SUMMARY_FILE" <<EOF
Total wall time : $(( build_end - run_start ))s
Build wall time : $(( build_end - build_start ))s
Peak mem        : $(( peak_kb / 1024 )) MB  (of $(( mem_total / 1024 )) MB total)
EOF
echo ""
echo "Total wall time : $(( build_end - run_start ))s"
echo "Build wall time : $(( build_end - build_start ))s"
echo "Peak mem        : $(( peak_kb / 1024 )) MB  (of $(( mem_total / 1024 )) MB total)"
echo "Summary saved   : $SUMMARY_FILE"
