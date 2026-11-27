#!/usr/bin/env bash
# Builds the self-contained Linux ZIP (RES-DIST-001, ADR-000218).
#
#   tools/release/build_linux_zip.sh [--build-dir build/release] [--jobs N] [--static-only]
#
# 1. configures a Release CMake tree with the bundle layout (no tests),
# 2. builds it, then
# 3. runs its package_linux_zip target: install into tmp/release, bundle the
#    runtime, write licenses and manifests, check the tree, write
#    dist/RESONANT-Linux-x86_64-<version>.zip and .sha256, validate the ZIP
#    (tools/release/validate_bundle.py; --static-only without a display).
# Needs cmake, a C++20 compiler, Qt 6 development files, patchelf, readelf,
# dpkg-query, unzip and sha256sum; xvfb-run for the runtime checks.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
tree="$root/build/release"
jobs="$(nproc)"
static=()
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) tree="$(realpath -m "$2")"; shift 2 ;;
        --jobs) jobs="$2"; shift 2 ;;
        --static-only) static=(--no-runtime-checks); shift ;;
        -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
for tool in cmake patchelf readelf dpkg-query unzip sha256sum qtpaths6; do
    command -v "$tool" >/dev/null || { echo "build_linux_zip: $tool is not installed" >&2; exit 1; }
done
generator=()
if [ ! -f "$tree/CMakeCache.txt" ] && command -v ninja >/dev/null; then
    generator=(-G Ninja)
fi
cmake -S "$root" -B "$tree" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DRESONANT_BUILD_TESTS=OFF -DRESONANT_BUNDLE_LAYOUT=ON
cmake --build "$tree" -j "$jobs"
if [ ${#static[@]} -gt 0 ]; then
    python3 "$root/tools/release/package.py" bundle --build-dir "$tree" --out "$root/dist" "${static[@]}"
else
    cmake --build "$tree" --target package_linux_zip
fi
