#!/usr/bin/env bash
# Tests the Linux ZIP on clean systems in containers (RES-DIST-013, ADR-000218).
#
#   tools/release/clean_machine_test.sh dist/RESONANT-Linux-x86_64-<version>.zip [--out tmp/clean-machine] [image ...]
#
# Images default to ubuntu:24.04, fedora:41, fedora:42 (expected to run) and
# ubuntu:22.04, debian:bookworm (glibc too old: the launcher must say so).
# For every image:
#   bare     the package (extracted on the host, mounted read only) on the
#            image as it is: the launcher must refuse with the missing system
#            libraries (or the too old C library) named, never crash;
#   desktop  an image with only the system packages the package declares
#            (tools/release/bundle_policy.json), Mesa and a virtual display -
#            no Qt, no Resonant dependency - built once with the network; then,
#            WITHOUT network: extract the ZIP with unzip, --check-runtime,
#            --check-help, the main window (screenshot), every example project,
#            every demonstration, a second start, relocation.
# Results: <out>/<image>/*.log and <out>/summary.md. Exit status 1 when an
# expected result was not met. Needs docker. Containers cannot show a real
# desktop session or a GPU driver: those are reported separately.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
zip="${1:?usage: clean_machine_test.sh <zip> [--out DIR] [image ...]}"; shift
zip="$(realpath "$zip")"
out="$root/tmp/clean-machine"
images=()
while [ $# -gt 0 ]; do
    case "$1" in
        --out) out="$(realpath -m "$2")"; shift 2 ;;
        *) images+=("$1"); shift ;;
    esac
done
[ ${#images[@]} -gt 0 ] || images=(ubuntu:24.04 fedora:41 fedora:42 ubuntu:22.04 debian:bookworm)
command -v docker >/dev/null || { echo "docker is not installed" >&2; exit 1; }
name="$(basename "$zip" .zip)"
rm -rf "$out"; mkdir -p "$out/package"
unzip -q "$zip" -d "$out/package"
pkg="$out/package/$name"

desktop_packages() {
    case "$1" in
        ubuntu:24.04|debian:trixie*|debian:13*) echo "apt libasound2t64" ;;
        ubuntu:*|debian:*) echo "apt libasound2" ;;
        fedora:*) echo "dnf" ;;
        *) echo "unknown" ;;
    esac
}

build_desktop_image() {  # image -> tag
    local image="$1" tag="resonant-clean/${1//[:\/]/-}:1"
    if docker image inspect "$tag" >/dev/null 2>&1; then echo "$tag"; return 0; fi
    read -r manager alsa <<<"$(desktop_packages "$image")"
    local file="$out/Dockerfile.${image//[:\/]/-}"
    case "$manager" in
        apt) cat > "$file" <<EOF
FROM $image
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    unzip xvfb xauth libgl1 libopengl0 libegl1 libgl1-mesa-dri libx11-6 libx11-xcb1 libxcb1 libxcb-glx0 libxcb-randr0 \
    libxcb-render0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xinput0 libxcb-xkb1 libxext6 libxrender1 \
    libsm6 libice6 libfontconfig1 libfreetype6 fonts-dejavu-core $alsa libstdc++6 zlib1g libexpat1 && rm -rf /var/lib/apt/lists/*
EOF
        ;;
        dnf) cat > "$file" <<EOF
FROM $image
RUN dnf install -y --setopt=install_weak_deps=False unzip xorg-x11-server-Xvfb xorg-x11-xauth which mesa-dri-drivers libglvnd-glx \
    libglvnd-opengl libglvnd-egl libX11 libX11-xcb libxcb libXext libXrender libSM libICE fontconfig freetype dejavu-sans-fonts \
    alsa-lib libstdc++ zlib expat && dnf clean all
EOF
        ;;
        *) return 1 ;;
    esac
    docker build -q -t "$tag" -f "$file" "$out" >"$out/build-${image//[:\/]/-}.log" 2>&1 || return 1
    echo "$tag"
}

summary="$out/summary.md"
printf '# Clean-machine results for %s\n\n| Image | System | Test | Result | Detail |\n|---|---|---|---|---|\n' "$name" > "$summary"
status=0
note() { printf '| %s | %s | %s | %s | %s |\n' "$1" "$2" "$3" "$4" "$5" >> "$summary"; echo "$4  $1 $3: $5"; }

for image in "${images[@]}"; do
    dir="$out/${image//[:\/]/-}"; mkdir -p "$dir"
    system="$(docker run --rm "$image" sh -c '. /etc/os-release; printf "%s, %s, glibc %s" "$PRETTY_NAME" "$(uname -m)" "$(getconf GNU_LIBC_VERSION | cut -d" " -f2)"' 2>/dev/null)"
    glibc_ok=1
    case "$image" in ubuntu:22.04|debian:bookworm|debian:12) glibc_ok=0 ;; esac

    # bare: the image as it is, the package mounted read only.
    docker run --rm --network none -v "$pkg:/opt/resonant:ro" "$image" /opt/resonant/resonant --check-runtime >"$dir/bare.log" 2>&1
    rc=$?
    if [ "$glibc_ok" = 1 ]; then
        if [ "$rc" -ne 0 ] && grep -q "the system lacks libraries" "$dir/bare.log" && grep -q "Debian/Ubuntu:" "$dir/bare.log"; then
            note "$image" "$system" "bare image" PASS "refused with the missing system libraries named: $(grep '^  [A-Z]' "$dir/bare.log" | grep -o 'lib[^ ]*\.so[^ ]*' | wc -l) libraries"
        else
            note "$image" "$system" "bare image" FAIL "exit $rc: $(tail -n 2 "$dir/bare.log" | tr '\n' ' ')"; status=1
        fi
    else
        if [ "$rc" -ne 0 ] && grep -q "needs glibc" "$dir/bare.log"; then
            note "$image" "$system" "bare image" PASS "refused: $(grep -o 'this system has glibc [0-9.]*; this package needs glibc [0-9.]* or newer' "$dir/bare.log")"
        else
            note "$image" "$system" "bare image" FAIL "exit $rc: $(tail -n 2 "$dir/bare.log" | tr '\n' ' ')"; status=1
        fi
        continue  # a desktop image cannot help: the C library is too old
    fi

    if ! tag="$(build_desktop_image "$image")"; then
        note "$image" "$system" "desktop image" BLOCKED "the image with the system packages could not be built (network?): see build-${image//[:\/]/-}.log"; status=1
        continue
    fi
    docker run --rm --network none -v "$zip:/dist/$name.zip:ro" -v "$dir:/out" "$tag" sh -c '
        set -u
        x() { xvfb-run -a -s "-screen 0 1600x1000x24" "$@"; }
        echo "== no Qt, Python or Resonant dependency on the image:"; command -v python3 || echo "python3: none"; ls /usr/lib*/libQt6Core* /usr/lib/*/libQt6Core* 2>/dev/null || echo "Qt 6: none"
        cd /root && unzip -q /dist/'"$name"'.zip && cd '"$name"' || exit 10
        echo "== check-runtime";  x ./resonant --check-runtime > /out/check-runtime.txt 2>/out/check-runtime.err || exit 11
        echo "== check-help";     x ./resonant --check-help > /out/check-help.txt 2>&1 || exit 12
        echo "== main window";    x ./resonant --screenshot /out/main-window.png examples/acoustics/c-focused-planar-array.resonant > /out/screenshot.log 2>&1 || exit 13
        echo "== projects";       x ./resonant --check-projects examples > /out/projects.txt 2>/out/projects.err || exit 14
        echo "== demonstrations"; for p in demos/profiles/*.json; do d=$(basename "$p" .json); \
                                  x ./resonant --run-demo "$d" --demo-output /root/demo-run >> /out/demo.txt 2>>/out/demo.err || exit 15; done
        echo "== restart";        x ./resonant --check-runtime > /out/restart.txt 2>&1 || exit 16
        echo "== relocation";     mkdir -p "/root/other place" && mv /root/'"$name"' "/root/other place/Resonant moved" && cd / && \
                                  x "/root/other place/Resonant moved/resonant" --check-runtime > /out/relocated.txt 2>&1 || exit 17
        echo done' >"$dir/desktop.log" 2>&1
    rc=$?
    gl="$(grep -o 'OpenGL: [^(]*([^;]*' "$dir/check-runtime.txt" 2>/dev/null | head -n 1)"
    if [ "$rc" -eq 0 ]; then
        note "$image" "$system" "desktop image, offline" PASS "unzip; check-runtime 0 failed ($gl); help $(grep -o 'search .array.: [0-9]* topics' "$dir/check-help.txt"); $(tail -n 1 "$dir/projects.txt"); $(grep -c 'demo: completed' "$dir/demo.txt") of $(ls "$pkg/demos/profiles" | grep -c '\.json$') demonstrations completed; restart and relocation pass"
    else
        note "$image" "$system" "desktop image, offline" FAIL "step $rc failed: $(tail -n 3 "$dir/desktop.log" | tr '\n' ' ')"; status=1
    fi
done
echo; cat "$summary"
exit $status
