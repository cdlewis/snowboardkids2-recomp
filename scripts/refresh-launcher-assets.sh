#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-cmake}"
APP_NAME="${APP_NAME:-SnowboardKids2Recompiled.app}"
APP_DIR="$ROOT_DIR/$BUILD_DIR/$APP_NAME"
ASSETS_SRC="$ROOT_DIR/assets"
ASSETS_DST="$APP_DIR/Contents/Resources/assets"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--build-dir DIR]

Sync non-C++ UI assets into the existing macOS app bundle.

Options:
  --build-dir DIR
            Build directory containing $APP_NAME.
            Defaults to BUILD_DIR, or build-cmake when BUILD_DIR is unset.
  -h, --help
            Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            if [[ $# -lt 2 ]]; then
                echo "error: --build-dir requires a directory" >&2
                usage >&2
                exit 1
            fi
            BUILD_DIR="$2"
            APP_DIR="$ROOT_DIR/$BUILD_DIR/$APP_NAME"
            ASSETS_DST="$APP_DIR/Contents/Resources/assets"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

require_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required tool '$1' was not found in PATH" >&2
        exit 1
    fi
}

require_dir() {
    if [[ ! -d "$1" ]]; then
        echo "error: required directory '$1' was not found" >&2
        exit 1
    fi
}

sync_assets() {
    require_dir "$APP_DIR"
    mkdir -p "$ASSETS_DST"

    if command -v rsync >/dev/null 2>&1; then
        echo "==> Syncing assets into $APP_NAME"
        rsync -a --delete --exclude='.DS_Store' "$ASSETS_SRC/" "$ASSETS_DST/"
    else
        require_tool cmake
        temp_assets="$(mktemp -d)"
        trap 'rm -rf "$temp_assets"' RETURN

        echo "==> Copying assets into $APP_NAME"
        cmake -E copy_directory "$ASSETS_SRC" "$temp_assets"
        cmake -E rm -f "$temp_assets/.DS_Store" "$temp_assets/icons/.DS_Store"
        cmake -E remove_directory "$ASSETS_DST"
        cmake -E copy_directory "$temp_assets" "$ASSETS_DST"
    fi
}

sync_assets

echo "==> UI assets refreshed"
