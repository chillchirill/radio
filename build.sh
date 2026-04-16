#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
BUILD_DIR="${ROOT_DIR}/build"
VCPKG_DIR="${ROOT_DIR}/vcpkg"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN=0

log() {
    printf '[build] %s\n' "$*"
}

die() {
    printf '[build] Error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: ./build.sh [options]

Options:
  --clean               Remove the build directory before configuring
  --build-type TYPE     Build type for CMake (default: Release)
  -h, --help            Show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --clean)
            CLEAN=1
            shift
            ;;
        --build-type)
            (($# >= 2)) || die "--build-type requires a value"
            BUILD_TYPE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown argument: $1"
            ;;
    esac
done

run_with_sudo() {
    if [[ "${EUID}" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "sudo is required for package installation."
    fi
}

detect_triplet() {
    case "$(uname -m)" in
        aarch64|arm64)
            printf 'arm64-linux\n'
            ;;
        armv7l|armv7|armv6l)
            printf 'arm-linux\n'
            ;;
        *)
            die "This script is only for Raspberry Pi (ARM Linux). Current architecture: $(uname -m)"
            ;;
    esac
}

ensure_raspberry_pi() {
    [[ "$(uname -s)" == "Linux" ]] || die "This script is only for Raspberry Pi OS/Linux."
    [[ -f /etc/os-release ]] || die "Cannot verify operating system."

    if ! grep -qiE 'raspbian|debian|ubuntu' /etc/os-release; then
        die "Expected Raspberry Pi OS or another Debian-based Linux."
    fi
}

install_packages() {
    log "Installing git, cmake, ninja and build tools"
    run_with_sudo apt-get update
    run_with_sudo apt-get install -y \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        ninja-build \
        pkg-config \
        tar \
        unzip \
        zip
}

ensure_vcpkg() {
    if [[ -d "${VCPKG_DIR}/.git" ]]; then
        log "Using existing vcpkg checkout"
    elif [[ -d "${VCPKG_DIR}" ]]; then
        die "Directory ${VCPKG_DIR} exists but is not a vcpkg git checkout."
    else
        log "Cloning vcpkg"
        git clone https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}"
    fi

    if [[ ! -x "${VCPKG_DIR}/vcpkg" ]]; then
        log "Bootstrapping vcpkg"
        "${VCPKG_DIR}/bootstrap-vcpkg.sh" -disableMetrics
    else
        log "vcpkg is already bootstrapped"
    fi
}

main() {
    local triplet
    local toolchain_file

    ensure_raspberry_pi

    [[ -f "${ROOT_DIR}/vcpkg.json" ]] || die "vcpkg.json was not found in ${ROOT_DIR}"
    [[ -f "${ROOT_DIR}/CMakeLists.txt" ]] || die "CMakeLists.txt was not found in ${ROOT_DIR}"

    triplet="$(detect_triplet)"
    toolchain_file="${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake"

    log "Triplet: ${triplet}"
    install_packages
    ensure_vcpkg

    if [[ "${CLEAN}" -eq 1 ]]; then
        log "Removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi

    log "Installing dependencies from vcpkg.json"
    "${VCPKG_DIR}/vcpkg" install \
        --triplet "${triplet}" \
        --x-manifest-root="${ROOT_DIR}"

    log "Configuring CMake"
    cmake \
        -S "${ROOT_DIR}" \
        -B "${BUILD_DIR}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
        -DVCPKG_TARGET_TRIPLET="${triplet}"

    log "Building project"
    cmake --build "${BUILD_DIR}" --parallel

    log "Build completed: ${BUILD_DIR}"
}

main "$@"
