#!/usr/bin/env bash
#
# Builds listless into build/, mirroring the manual
# `cmake -S . -B build && cmake --build build` workflow used by CI
# (.github/workflows/ci.yml). Uses CMakeLists.txt's own default build
# type (Debug) unless -DCMAKE_BUILD_TYPE is already cached in build/;
# doesn't offer a --release flag itself (a Release build now works --
# issue #43 -- but exposing it here is a separate ask).
#
# Usage: build.sh [--stripped] [--jobs N]
#
#   --stripped   strip debug symbols from the resulting build/lss
#               binary after a successful build
#   --jobs N    parallel build jobs (default: detected CPU count)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

strip_binary=0
jobs="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --stripped)
            strip_binary=1
            shift
            ;;
        --jobs)
            jobs="$2"
            shift 2
            ;;
        -h | --help)
            awk 'NR==1 {next} /^#/ {sub(/^# ?/, ""); print; next} {exit}' "$0"
            exit 0
            ;;
        *)
            echo "build.sh: unknown argument '$1' (see --help)" >&2
            exit 1
            ;;
    esac
done

generator_args=()
if command -v ninja >/dev/null 2>&1; then
    generator_args=(-G Ninja)
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" "${generator_args[@]}"
cmake --build "$BUILD_DIR" -j "$jobs"

binary="$BUILD_DIR/lss"
if [[ ! -f "$binary" ]]; then
    echo "build.sh: expected binary at $binary, not found after build" >&2
    exit 1
fi

if [[ "$strip_binary" -eq 1 ]]; then
    strip "$binary"
    echo "build.sh: stripped $binary"
fi

echo "build.sh: binary at $binary"
