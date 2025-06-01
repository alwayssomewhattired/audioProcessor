#!/bin/bash

VCPKG_ROOT="/vcpkg"
TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

BUILD_DIR="build"

# Create build directory if missing
mkdir -p "$BUILD_DIR"

# Run cmake configure and generate build system
cmake -B "$BUILD_DIR" -S . -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"

# Build
cmake --build "$BUILD_DIR"