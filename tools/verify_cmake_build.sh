#!/usr/bin/env bash
# Builds Resonant with CMake in its own tree and runs CTest. The
# development-control system builds through qmake; this check keeps the
# CMake build from breaking unnoticed (ADR-000204).
#
# The tree is a Debug configuration (assertions on, including Eigen's index
# checks) compiled with -O1: unoptimized code made the numerical solver
# tests take longer than the check is allowed to run. It is a tree of its
# own so the flags never leak into a developer's build/cmake-debug.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tree="${RESONANT_CMAKE_TREE:-$root/build/cmake-verify}"
generator=()
if [ ! -f "$tree/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi
cmake -S "$root" -B "$tree" "${generator[@]}" -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS_DEBUG=-g -O1"
cmake --build "$tree" -j "$(nproc)"
ctest --test-dir "$tree" --output-on-failure
