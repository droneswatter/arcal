#!/usr/bin/env bash
# Build arcal: regenerate schema, configure, and compile.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

cd "$ROOT"

PREFIX="${ARCAL_PREFIX:-$HOME/.local}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
CXX_COMPILER="${CMAKE_CXX_COMPILER:-clang++-20}"
C_COMPILER="${CMAKE_C_COMPILER:-clang-20}"

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
    -G Ninja

echo "==> Building..."
cmake --build build

echo "==> Done. Run tests with: ctest --test-dir build --output-on-failure"
