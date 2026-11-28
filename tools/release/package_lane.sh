#!/usr/bin/env bash
# The package lane of scripts/test_manifest.json (ADR-000230, RES-REL-005): verifies a built
# Linux ZIP once per deployment. It never builds and never re-runs the development suites
# (ADR-000186).
#
#   tools/release/package_lane.sh validate        # checksum, then the bundle: layout, runtime
#                                                 # closure, licences, help, start-up and every
#                                                 # demonstration under a virtual display
#   tools/release/package_lane.sh clean-machine   # the clean-container tests (docker)
#
# The ZIP is $RESONANT_PACKAGE_ZIP, or else the newest dist/RESONANT-Linux-x86_64-*.zip.
# A missing tool fails the lane: a lower-quality check is never substituted silently.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
mode=${1:-}
zip=${RESONANT_PACKAGE_ZIP:-}
if [ -z "$zip" ]; then
    zip=$(ls -t "$root"/dist/RESONANT-Linux-x86_64-*.zip 2>/dev/null | head -n 1 || true)
fi
if [ -z "$zip" ] || [ ! -f "$zip" ]; then
    echo "package lane: no ZIP to verify (build one with tools/release/build_linux_zip.sh, or set RESONANT_PACKAGE_ZIP)" >&2
    exit 2
fi
echo "package lane: $mode $(basename "$zip")"
case "$mode" in
validate)
    command -v xvfb-run >/dev/null || { echo "package lane: xvfb-run is not installed: the runtime checks need a virtual display" >&2; exit 2; }
    sh "$root/tools/release/generate_checksum.sh" --verify "$zip"
    mkdir -p "$root/tmp"
    xvfb-run -a -s "-screen 0 1600x1000x24" python3 "$root/tools/release/validate_bundle.py" --zip "$zip" --checksum "${zip%.zip}.sha256" \
        --report "$root/tmp/package-validation.json"
    ;;
clean-machine)
    command -v docker >/dev/null || { echo "package lane: docker is not installed: the clean-machine tests need it" >&2; exit 2; }
    bash "$root/tools/release/clean_machine_test.sh" "$zip" --out "$root/tmp/clean-machine"
    ;;
*)
    echo "usage: tools/release/package_lane.sh validate|clean-machine" >&2
    exit 2
    ;;
esac
