#!/bin/sh
# Writes or verifies the SHA-256 checksum of a release archive (RES-DIST-001).
#
#   tools/release/generate_checksum.sh dist/RESONANT-Linux-x86_64-<version>.zip           # writes <name>.sha256
#   tools/release/generate_checksum.sh --verify dist/RESONANT-Linux-x86_64-<version>.zip  # checks it
set -eu
verify=0
if [ "${1:-}" = "--verify" ]; then verify=1; shift; fi
[ $# -eq 1 ] || { sed -n '2,5p' "$0"; exit 2; }
dir=$(dirname -- "$1"); name=$(basename -- "$1"); sum="${name%.zip}.sha256"
cd "$dir"
if [ "$verify" = 1 ]; then
    exec sha256sum -c "$sum"
fi
sha256sum "$name" > "$sum"
cat "$sum"
