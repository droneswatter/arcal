#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 full|busmon-cert [-- <extra cmake args>]" >&2
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

prefix="${ARCAL_PREFIX:-$HOME/.local}"
build_type="${CMAKE_BUILD_TYPE:-Debug}"
cxx_compiler="${CMAKE_CXX_COMPILER:-clang++-20}"
c_compiler="${CMAKE_C_COMPILER:-clang-20}"
generator="${CMAKE_GENERATOR:-Ninja}"
build_parallel="${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
unity_batch_size="${ARCAL_UNITY_BATCH_SIZE:-8}"

arcal_suspend_cpptools
trap 'arcal_resume_cpptools' EXIT

case "$variant" in
    full)
        build_dir="${ARCAL_BUILD_DIR:-$root/build}"
        cert_cal="arcal"
        json_plugin="arcal_externalizer_json"
        lacal_target="arlacal_server"
        example_prefix="arcal"
        subset_args=()
        ;;
    busmon-cert)
        build_dir="${ARCAL_BUILD_DIR:-$root/build-busmon-cert}"
        cert_cal="arcal_busmon_cert"
        json_plugin="arcal_busmon_cert_externalizer_json"
        lacal_target="arlacal_server_busmon_cert"
        example_prefix="arcal_busmon_cert"
        subset_args=("-DARCAL_SUBSET_CONFIGS=$root/config/subsets/arcal-busmon-cert.json")
        ;;
    *)
        usage
        exit 2
        ;;
esac

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_CXX_COMPILER="$cxx_compiler" \
    -DCMAKE_C_COMPILER="$c_compiler" \
    -DCMAKE_PREFIX_PATH="$prefix" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DARCAL_CERT_CAL_LIB="$cert_cal" \
    -DARCAL_UNITY_BATCH_SIZE="$unity_batch_size" \
    "${subset_args[@]}" \
    "$@" \
    -G "$generator"

build_targets=("$cert_cal" "$json_plugin" "$lacal_target" "arcal_test_suite_all")
if [[ -f "$build_dir/CMakeCache.txt" ]] &&
    grep -qx 'ARCAL_BUILD_EXAMPLES:BOOL=ON' "$build_dir/CMakeCache.txt"; then
    build_targets+=("${example_prefix}_amti_service_demo" "${example_prefix}_smti_service_demo")
fi

cmake --build "$build_dir" --target "${build_targets[@]}" -j "$build_parallel"
