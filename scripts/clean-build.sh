#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-cmake}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
TARGET="${TARGET:-SnowboardKids2Recompiled}"
C_COMPILER="${CC:-clang}"
CXX_COMPILER="${CXX:-clang++}"
PATCHES_LD="${PATCHES_LD:-ld.lld}"

if [[ -n "${PATCHES_CC:-}" ]]; then
    PATCHES_C_COMPILER="$PATCHES_CC"
elif [[ -x /opt/homebrew/opt/llvm/bin/clang ]]; then
    PATCHES_C_COMPILER="/opt/homebrew/opt/llvm/bin/clang"
else
    PATCHES_C_COMPILER="$C_COMPILER"
fi

BUILD_PATH="$ROOT_DIR/$BUILD_DIR"

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required tool '$1' was not found in PATH" >&2
        exit 1
    fi
}

require_file() {
    if [[ ! -e "$1" ]]; then
        echo "error: required file '$1' was not found" >&2
        exit 1
    fi
}

require_tool cmake
require_tool ninja
require_tool make
require_tool "$C_COMPILER"
require_tool "$CXX_COMPILER"
require_tool "$PATCHES_C_COMPILER"
require_tool "$PATCHES_LD"
require_file "$ROOT_DIR/N64Recomp"

echo "==> Cleaning CMake build directory: $BUILD_PATH"
rm -rf "$BUILD_PATH"
mkdir -p "$BUILD_PATH/clang-module-cache"
export CLANG_MODULE_CACHE_PATH="$BUILD_PATH/clang-module-cache"

echo "==> Cleaning patches"
make -C "$ROOT_DIR/patches" clean
rm -f "$ROOT_DIR/patches/patches.bin" "$ROOT_DIR/patches/patches.map"

echo "==> Building patches"
make -C "$ROOT_DIR/patches" CC="$PATCHES_C_COMPILER" LD="$PATCHES_LD"

echo "==> Recompiling patches"
(
    cd "$ROOT_DIR"
    ./N64Recomp patches.toml
)

echo "==> Configuring $BUILD_TYPE build"
cmake -S "$ROOT_DIR" -B "$BUILD_PATH" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DPATCHES_C_COMPILER="$PATCHES_C_COMPILER" \
    -DPATCHES_LD="$PATCHES_LD"

echo "==> Building $TARGET"
cmake --build "$BUILD_PATH" --target "$TARGET" --config "$BUILD_TYPE" --parallel

echo "==> Done"
