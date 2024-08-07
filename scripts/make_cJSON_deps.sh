#!/bin/bash

# -----------------------------------------------------------------------------
# This script automates the process of setting up a cross-compilation
# environment for the cJSON library. It prepares the build
# directory, sets the toolchain for cross-compilation, clones the
# cJSON repository if not present, configures the build using CMake,
# compiles the library, and finally copies the built library and relevant
# headers to the appropriate locations in the repository.
# -----------------------------------------------------------------------------

set -e
set -o pipefail

# Variables
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build/cJSON-build"
CJSON_REPO="https://github.com/DaveGamble/cJSON"
CJSON_DIR="${BUILD_DIR}/cJSON"

# Create cJSON build directory
echo "Creating cJSON build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ "$1" == "download_only" ]]; then

# Clone cJSON if not already present
if [ ! -d "$CJSON_DIR" ]; then
    echo "Cloning cJSON..."
    git clone --depth 1 "$CJSON_REPO"
fi

pwd
cp -R cJSON/cJSON.h ../../include/

exit 0

fi

# Set compiler prefix here
if [ -n "$BR2_CONFIG" ]; then
    echo "Running within Buildroot build process."
else

CROSS_COMPILE="mipsel-linux-"

fi

CC="${CROSS_COMPILE}gcc"

# Ensure CC is set
if [ -z "$CC" ]; then
    echo "Error: CC environment variable must be set."
    exit 1
fi

# Clone cJSON if not already present
if [ ! -d "$CJSON_DIR" ]; then
    echo "Cloning cJSON..."
    git clone "$CJSON_REPO"
fi

cd "$CJSON_DIR"

# Create and navigate to cmake build dir
mkdir -p build
cd build

# Configure and build cJSON library
echo "Configuring cJSON library..."
cmake \
-DCMAKE_SYSTEM_NAME=Linux \
-DCMAKE_SYSTEM_PROCESSOR=mipsle \
-DCMAKE_C_COMPILER_LAUNCHER=$(which ccache) \
-DCMAKE_C_COMPILER=${CC} \
-DCMAKE_BUILD_TYPE=RELEASE \
-DBUILD_SHARED_AND_STATIC_LIBS=OFF \
..

echo "Building cJSON library..."
make

# Copy cJSON library and headers
echo "Copying cJSON library and headers..."
cp ./libcjson.so ../../../3rdparty/install/lib/
echo "cJSON build complete!"
