#!/bin/sh
# Prints the dependency graph of ELF files or of every ELF file in a folder:
# each DT_NEEDED entry, whether the bundle policy leaves it to the system or
# bundles it, and where this system resolves it (RES-DIST-002).
#
#   tools/release/inspect_dependencies.sh build/release/src/app/resonant
#   tools/release/inspect_dependencies.sh <extracted package folder>
exec python3 "$(dirname "$0")/package.py" inspect "$@"
