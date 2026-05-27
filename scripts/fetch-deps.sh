#!/usr/bin/env bash
# fetch-deps.sh — pre-stage external libraries for offline / locked-down builds.
#
# Clones the five upstream libraries at their pinned revisions into
# ./external/<lib>/. CMakeLists.txt detects this directory and uses the
# checkouts directly instead of running FetchContent.
#
# Usage:
#   scripts/fetch-deps.sh               # clone into ./external/
#   scripts/fetch-deps.sh /path/to/dir  # clone into a custom directory
#
# After running, configure CMake as usual:
#   cmake -S . -B build && cmake --build build -j
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-${HERE}/external}"

mkdir -p "$DEST"

# name                 url                                                          tag
DEPS=(
    "signalsmith-stretch|https://github.com/Signalsmith-Audio/signalsmith-stretch.git|main"
    "soundtouch|https://codeberg.org/soundtouch/soundtouch.git|2.3.3"
    "rubberband|https://github.com/breakfastquay/rubberband.git|v4.0.0"
    "libsamplerate|https://github.com/libsndfile/libsamplerate.git|0.2.2"
    "r8brain|https://github.com/avaneev/r8brain-free-src.git|version-6.5"
)

printf 'Pre-staging %d dependencies into %s\n\n' "${#DEPS[@]}" "$DEST"

for entry in "${DEPS[@]}"; do
    IFS='|' read -r name url tag <<<"$entry"
    target="$DEST/$name"
    if [[ -d "$target/.git" ]]; then
        printf '  [skip ] %-22s already present\n' "$name"
        continue
    fi
    printf '  [clone] %-22s %s @ %s\n' "$name" "$url" "$tag"
    git clone --depth 1 --branch "$tag" --quiet "$url" "$target"
done

echo
echo "Done. Build with:"
echo "    cmake -S . -B build && cmake --build build -j"
echo
echo "CMake will detect $DEST/ and skip downloads."
