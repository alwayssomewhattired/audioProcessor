#!/bin/bash

VCPKG_ROOT="/vcpkg"
TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

BUILD_DIR="./build"

# Create build directory if missing
mkdir -p "$BUILD_DIR"

# Run cmake configure and generate build system
cmake -B "$BUILD_DIR" -S . \
-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
-DVCPKG_TARGET_TRIPLET=x64-linux


# Build
cmake --build "$BUILD_DIR"
