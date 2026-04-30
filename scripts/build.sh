#!/usr/bin/env bash
# Build arcal: regenerate schema, configure, and compile.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

source "$SCRIPT_DIR/build-support.sh"

cd "$ROOT"

PREFIX="${ARCAL_PREFIX:-$HOME/.local}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
CXX_COMPILER="${CMAKE_CXX_COMPILER:-clang++-20}"
C_COMPILER="${CMAKE_C_COMPILER:-clang-20}"
BUILD_PARALLEL="${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
UNITY_BATCH_SIZE="${ARCAL_UNITY_BATCH_SIZE:-8}"

arcal_suspend_cpptools
trap 'arcal_resume_cpptools' EXIT

echo "==> Regenerating schema headers and CDR handlers..."
uv run tools/schema_compiler/compiler.py \
    --schema schema/OAC-STD-UCI_V2.5 \
    --out include

echo "==> Configuring CMake..."
cmake -S . -B build \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DARCAL_UNITY_BATCH_SIZE="$UNITY_BATCH_SIZE" \
    -G Ninja

echo "==> Building..."
cmake --build build -j "$BUILD_PARALLEL"

echo "==> Done. Run tests with: ctest --test-dir build --output-on-failure"
