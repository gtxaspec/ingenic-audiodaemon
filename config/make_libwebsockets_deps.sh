#!/bin/bash

# -----------------------------------------------------------------------------
# This script automates the process of setting up a cross-compilation
# environment for the libwebsockets library. It prepares the build
# directory, sets the toolchain for cross-compilation, clones the
# libwebsockets repository if not present, configures the build using CMake,
# compiles the library, and finally copies the built library and relevant
# headers to the appropriate locations in the repository.
# -----------------------------------------------------------------------------

set -e
set -o pipefail

# Set compiler prefix here
CROSS_COMPILE="mipsel-openipc-linux-musl-"

CC="${CROSS_COMPILE}gcc"
CXX="${CROSS_COMPILE}g++"

# Ensure CC and CXX are set
if [ -z "$CC" ] || [ -z "$CXX" ]; then
    echo "Error: CC and CXX environment variables must be set."
    exit 1
fi

# Variables
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build/lws-build"
LWS_REPO="https://github.com/warmcat/libwebsockets"
LWS_DIR="${BUILD_DIR}/libwebsockets"

# Create libwebsockets build directory
echo "Creating libwebsockets build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Clone libwebsockets if not already present
if [ ! -d "$LWS_DIR" ]; then
    echo "Cloning libwebsockets..."
    git clone "$LWS_REPO"
fi

cd "$LWS_DIR"

# Create and navigate to cmake build dir
mkdir -p build
cd build

# Configure and build libwebsockets library
echo "Configuring libwebsockets library..."
cmake \
-D CMAKE_SYSTEM_NAME=Linux \
-D CMAKE_SYSTEM_PROCESSOR=mipsle \
-D CMAKE_C_COMPILER=${CC} \
-D CMAKE_CXX_COMPILER=${CXX} \
-D CMAKE_C_COMPILER_LAUNCHER=ccache \
-D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
-D CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
-DLWS_WITH_NETLINK=OFF \
-DLWS_WITH_SSL=OFF \
-DLWS_HAVE_LIBCAP=FALSE \
..

echo "Building libwebsockets library..."
make

# Copy libwebsockets library and headers
echo "Copying libwebsockets library and headers..."
cp ./lib/libwebsockets.a ../../../../lib/
cp -R ../include/libwebsockets ../../../../include/
cp -R include/*.h ../../../../include/

echo "libwebsockets build complete!"
