#!/usr/bin/env bash
set -euo pipefail

arguments=(--preset "${CMAKE_PRESET:?CMake preset is required}" -DBUILD_TESTING=ON)
version="${PXL_RELEASE_VERSION:-}"
if [[ -n "$version" ]]; then
    number='(0|[1-9][0-9]*)'
    if [[ ! "$version" =~ ^$number\.$number\.$number-alpha\.$number$ ]]; then
        echo "PXL release version must use x.y.z-alpha.N" >&2
        exit 1
    fi
    if [[ "${BUILD_TYPE:-}" != Release ]]; then
        echo "An explicit PXL alpha version requires a Release build" >&2
        exit 1
    fi
    arguments+=("-DLauncher_RELEASE_VERSION=$version")
fi
cmake "${arguments[@]}"
