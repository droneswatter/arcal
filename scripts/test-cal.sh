#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 full|busmon-cert [-- <extra ctest args>]" >&2
}

if [[ $# -lt 1 ]]; then
    usage
    exit 2
fi

variant="$1"
shift
if [[ "${1:-}" == "--" ]]; then
    shift
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$script_dir/.."

source "$script_dir/build-support.sh"

case "$variant" in
    full)
        build_dir="${ARCAL_BUILD_DIR:-$root/build}"
        ;;
    busmon-cert)
        build_dir="${ARCAL_BUILD_DIR:-$root/build-busmon-cert}"
        ;;
    *)
        usage
        exit 2
        ;;
esac

arcal_suspend_cpptools
trap 'arcal_resume_cpptools' EXIT

cmake --build "$build_dir" --target arcal_test_suite_all -j "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
ctest --test-dir "$build_dir" --output-on-failure "$@"
