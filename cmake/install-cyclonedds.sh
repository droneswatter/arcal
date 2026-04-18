#!/usr/bin/env bash
# Install Eclipse Cyclone DDS (C library + C++ binding) to a local prefix.
# Usage: ./cmake/install-cyclonedds.sh [prefix]
# Default prefix: /usr/local

set -euo pipefail

PREFIX="${1:-/usr/local}"
TAG="0.10.5"
JOBS=$(nproc 2>/dev/null || echo 4)
BUILD_DIR="$(mktemp -d)"

echo "Installing Cyclone DDS ${TAG} to ${PREFIX}"
echo "Build dir: ${BUILD_DIR}"

build_and_install() {
    local repo="$1"
    local name="$2"
    local extra_args="${3:-}"

    echo "--- ${name} ---"
    git clone --depth 1 --branch "${TAG}" "${repo}" "${BUILD_DIR}/${name}"
    cmake -S "${BUILD_DIR}/${name}" -B "${BUILD_DIR}/${name}-build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DBUILD_TESTING=OFF \
        ${extra_args}
    cmake --build "${BUILD_DIR}/${name}-build" -j"${JOBS}"
    cmake --install "${BUILD_DIR}/${name}-build"
}

build_and_install \
    https://github.com/eclipse-cyclonedds/cyclonedds \
    cyclonedds

build_and_install \
    https://github.com/eclipse-cyclonedds/cyclonedds-cxx \
    cyclonedds-cxx \
    "-DCycloneDDS_DIR=${PREFIX}/lib/cmake/CycloneDDS"

rm -rf "${BUILD_DIR}"
echo "Done. Add ${PREFIX} to CMAKE_PREFIX_PATH when building arcal."
