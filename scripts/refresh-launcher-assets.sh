#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
APP_NAME="${APP_NAME:-SnowboardKids2Recompiled.app}"
APP_DIR="$ROOT_DIR/$BUILD_DIR/$APP_NAME"
ASSETS_SRC="$ROOT_DIR/assets"
ASSETS_DST="$APP_DIR/Contents/Resources/assets"
SCSS_DIR="$ASSETS_SRC/scss"
RCSS_FILE="$ASSETS_SRC/recomp.rcss"
BUILD_SCSS=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [--scss]

Sync non-C++ launcher assets into the existing macOS app bundle.

Options:
  --scss    Rebuild assets/recomp.rcss from assets/scss before syncing.
  -h, --help
            Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scss)
            BUILD_SCSS=1
            shift
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

scss_is_newer_than_rcss() {
    [[ ! -f "$RCSS_FILE" ]] && return 0
    find "$SCSS_DIR" -type f -name '*.scss' -newer "$RCSS_FILE" -print -quit | grep -q .
}

has_dart_sass() {
    command -v sass >/dev/null 2>&1 && ! sass --version 2>/dev/null | grep -qi 'ruby sass'
}

build_rcss() {
    if [[ "$BUILD_SCSS" -eq 0 ]]; then
        return
    fi

    if [[ ! -d "$SCSS_DIR" ]]; then
        return
    fi

    if [[ -x "$SCSS_DIR/node_modules/.bin/sass" ]]; then
        echo "==> Building assets/recomp.rcss"
        (
            cd "$SCSS_DIR"
            ./node_modules/.bin/sass --no-source-map --no-charset --style=compressed main.scss ../recomp.rcss
        )
    elif has_dart_sass; then
        echo "==> Building assets/recomp.rcss"
        sass --no-source-map --no-charset --style=compressed "$SCSS_DIR/main.scss" "$RCSS_FILE"
    elif scss_is_newer_than_rcss; then
        echo "error: SCSS changed, but Dart Sass is not available." >&2
        echo "       Run 'cd assets/scss && npm install', or install sass globally." >&2
        exit 1
    else
        echo "==> Dart Sass not available; assets/recomp.rcss is already current enough"
    fi
}

sync_assets() {
    require_dir "$APP_DIR"
    mkdir -p "$ASSETS_DST"

    if command -v rsync >/dev/null 2>&1; then
        echo "==> Syncing assets into $APP_NAME"
        rsync -a --delete --exclude '/scss/' "$ASSETS_SRC/" "$ASSETS_DST/"
    else
        require_tool cmake
        temp_assets="$(mktemp -d)"
        trap 'rm -rf "$temp_assets"' RETURN

        echo "==> Copying assets into $APP_NAME"
        cmake -E copy_directory "$ASSETS_SRC" "$temp_assets"
        cmake -E remove_directory "$temp_assets/scss"
        cmake -E copy_directory "$temp_assets" "$ASSETS_DST"
    fi
}

build_rcss
sync_assets

echo "==> Launcher assets refreshed"
