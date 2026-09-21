#!/bin/bash
# Build script for the Catime Linux port (Ubuntu 24.04).
#
# Usage:
#   ./build.sh            # Release build into ./build, run smoke check
#   ./build.sh Debug
set -u

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

echo ">> Configuring ($BUILD_TYPE) in $BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || exit 1

echo ">> Building"
cmake --build "$BUILD_DIR" -j"$(nproc)" || exit 1

BIN="$BUILD_DIR/catime"
echo ">> Built: $BIN"

# Quick smoke: print help (no display server needed).
"$BIN" --version && "$BIN" --help | head -5
echo
echo "Run it with:  $BIN"
echo "Send commands to a running instance, e.g.:  $BIN 25   |   $BIN pr   |   $BIN v"
