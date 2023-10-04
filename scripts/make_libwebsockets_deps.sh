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
STRIP="${CROSS_COMPILE}strip"

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
MAKEFILE="$SCRIPT_DIR/../Makefile"

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

if grep -q "CONFIG_STATIC_BUILD=n" "$MAKEFILE"; then
echo "Found CONFIG_STATIC_BUILD=n"
echo "Configuring libwebsockets library..."
cmake \
-DCMAKE_SYSTEM_NAME=Linux \
-DCMAKE_SYSTEM_PROCESSOR=mipsle \
-DCMAKE_C_COMPILER_LAUNCHER=$(which ccache) \
-DCMAKE_CXX_COMPILER_LAUNCHER=$(which ccache) \
-DCMAKE_C_COMPILER=${CC} \
-DCMAKE_CXX_COMPILER=${CXX} \
-DCMAKE_BUILD_TYPE=RELEASE \
-DLWS_WITH_DIR=OFF \
-DLWS_WITH_LEJP_CONF=OFF \
-DLWS_WITHOUT_EXTENSIONS=1 \
-DLWS_WITH_NETLINK=OFF \
-DLWS_WITH_SSL=OFF \
-DLWS_HAVE_LIBCAP=OFF \
-DLWS_CTEST_INTERNET_AVAILABLE=OFF \
-DLWS_WITH_MINIMAL_EXAMPLES=OFF \
-DLWS_IPV6=ON \
-DLWS_ROLE_RAW_FILE=OFF \
-DLWS_UNIX_SOCK=OFF \
-DLWS_WITH_HTTP_BASIC_AUTH=OFF \
-DLWS_WITH_HTTP_UNCOMMON_HEADERS=OFF \
-DLWS_WITH_SYS_STATE=OFF \
-DLWS_WITH_SYS_SMD=OFF \
-DLWS_WITH_UPNG=OFF \
-DLWS_WITH_GZINFLATE=OFF \
-DLWS_WITH_JPEG=OFF \
-DLWS_WITH_DLO=OFF \
-DLWS_WITH_SECURE_STREAMS=OFF \
-DLWS_SSL_CLIENT_USE_OS_CA_CERTS=OFF \
-DLWS_WITH_TLS_SESSIONS=OFF \
-DLWS_WITH_EVLIB_PLUGINS=OFF \
-DLWS_WITH_LEJP=ON \
-DLWS_WITH_CBOR_FLOAT=OFF \
-DLWS_WITH_LHP=OFF \
-DLWS_WITH_JSONRPC=OFF \
-DLWS_WITH_LWSAC=ON \
-DLWS_WITH_CUSTOM_HEADERS=OFF \
-DLWS_CLIENT_HTTP_PROXYING=OFF \
-DLWS_WITH_FILE_OPS=ON \
-DLWS_WITH_CONMON=OFF \
-DLWS_WITH_CACHE_NSCOOKIEJAR=OFF \
..

elif grep -q "CONFIG_STATIC_BUILD=y" "$MAKEFILE"; then
echo "Found CONFIG_STATIC_BUILD=y"
echo "Configuring libwebsockets library..."
cmake \
-DCMAKE_SYSTEM_NAME=Linux \
-DCMAKE_SYSTEM_PROCESSOR=mipsle \
-DCMAKE_C_COMPILER_LAUNCHER=$(which ccache) \
-DCMAKE_CXX_COMPILER_LAUNCHER=$(which ccache) \
-DCMAKE_C_COMPILER=${CC} \
-DCMAKE_CXX_COMPILER=${CXX} \
-DCMAKE_BUILD_TYPE=RELEASE \
-DLWS_WITH_DIR=OFF \
-DLWS_WITH_LEJP_CONF=OFF \
-DLWS_WITHOUT_EXTENSIONS=1 \
-DLWS_WITH_NETLINK=OFF \
-DLWS_WITH_SSL=OFF \
-DLWS_HAVE_LIBCAP=OFF \
-DLWS_CTEST_INTERNET_AVAILABLE=OFF \
-DLWS_WITH_MINIMAL_EXAMPLES=OFF \
-DLWS_IPV6=ON \
-DLWS_ROLE_RAW_FILE=OFF \
-DLWS_UNIX_SOCK=OFF \
-DLWS_WITH_HTTP_BASIC_AUTH=OFF \
-DLWS_WITH_HTTP_UNCOMMON_HEADERS=OFF \
-DLWS_WITH_SYS_STATE=OFF \
-DLWS_WITH_SYS_SMD=OFF \
-DLWS_WITH_UPNG=OFF \
-DLWS_WITH_GZINFLATE=OFF \
-DLWS_WITH_JPEG=OFF \
-DLWS_WITH_DLO=OFF \
-DLWS_WITH_SECURE_STREAMS=OFF \
-DLWS_SSL_CLIENT_USE_OS_CA_CERTS=OFF \
-DLWS_WITH_TLS_SESSIONS=OFF \
-DLWS_WITH_EVLIB_PLUGINS=OFF \
-DLWS_WITH_LEJP=OFF \
-DLWS_WITH_CBOR_FLOAT=OFF \
-DLWS_WITH_LHP=OFF \
-DLWS_WITH_JSONRPC=OFF \
-DLWS_WITH_LWSAC=OFF \
-DLWS_WITH_CUSTOM_HEADERS=OFF \
-DLWS_CLIENT_HTTP_PROXYING=OFF \
-DLWS_WITH_FILE_OPS=OFF \
-DLWS_WITH_CONMON=OFF \
-DLWS_WITH_CACHE_NSCOOKIEJAR=OFF \
..
else
echo "CONFIG_STATIC_BUILD setting not found or is set to an unexpected value."
fi

echo "Building libwebsockets library..."
make

# Copy libwebsockets library and headers
echo "Copying libwebsockets library and headers..."
$STRIP ./lib/libwebsockets.so.19
cp ./lib/libwebsockets.a ../../../../lib/
cp ./lib/libwebsockets.so.19 ../../../../lib/libwebsockets.so
cp -R ../include/libwebsockets ../../../../include/
cp -R include/*.h ../../../../include/

echo "libwebsockets build complete!"
