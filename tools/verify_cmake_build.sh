#!/usr/bin/env bash
# Builds Resonant with CMake in its own tree and runs CTest. The
# development-control system builds through qmake; this check keeps the
# CMake build from breaking unnoticed (ADR-000204).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tree="${RESONANT_CMAKE_TREE:-$root/build/cmake-debug}"
generator=()
if [ ! -f "$tree/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi
cmake -S "$root" -B "$tree" "${generator[@]}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$tree" -j "$(nproc)"
ctest --test-dir "$tree" --output-on-failure
