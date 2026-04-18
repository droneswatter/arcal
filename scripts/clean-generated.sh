#!/usr/bin/env bash
# Remove all schema-compiler-generated files.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

rm -rf "$ROOT/include/uci/type"
rm -rf "$ROOT/src/generated"

echo "Removed include/uci/type and src/generated"
