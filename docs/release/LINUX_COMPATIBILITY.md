# Linux Compatibility of the ZIP

What the self-contained Linux ZIP needs from a system, what it brings, and
where it was tested (RES-DIST-002, RES-DIST-007, RES-DIST-013, ADR-000218).
Test details: [the release report](RELEASE_REPORT.md).

## Baseline

| | |
|---|---|
| Architecture | x86_64 (64-bit Intel and AMD) only |
| C library | glibc **2.38** or newer (the highest symbol version any file of the package needs; built on Ubuntu 24.04, glibc 2.39). musl-based systems (Alpine) are not supported. |
| C++ runtime | the system's libstdc++ from GCC 13 or newer (present on every distribution with glibc 2.38+) |
| Kernel | any kernel those distributions ship; nothing kernel-specific is used |
| Graphics | OpenGL **4.5 core** through the system's driver (libglvnd: Mesa 21+ for Intel, AMD and software rendering, or the NVIDIA driver) |
| Display | X11, or Wayland with XWayland (native Wayland is supported through Qt's plugin but was not tested) |
| File system | any: the package writes nothing into its folder; paths with spaces work |

## Distributions

| Distribution | glibc | Status |
|---|---|---|
| Ubuntu 24.04 LTS | 2.39 | **tested**: clean container with only the system packages below, offline; and the build machine's desktop (Intel Mesa, NVIDIA, software rendering) |
| Fedora 41 | 2.40 | **tested**: clean container, offline |
| Fedora 42 | 2.41 | **tested**: clean container, offline |
| Debian 13, Ubuntu 24.10 and later, Fedora 40, Arch, openSUSE Tumbleweed | 2.39 - 2.41+ | expected to work (same baseline), not tested |
| Ubuntu 22.04 LTS | 2.35 | **not supported**: tested, the launcher refuses with the glibc version |
| Debian 12 | 2.36 | **not supported**: tested, the launcher refuses with the glibc version |
| RHEL / Rocky / Alma 9 | 2.34 | not supported (glibc too old) |

Supporting older systems needs a build on an older baseline (an older
distribution with its own Qt 6 build); this release does not have one.

## Bundled (inside the ZIP)

Qt 6.4.2 (Core, Gui, Widgets, OpenGL, OpenGLWidgets, DBus, XcbQpa, Svg,
WaylandClient) and its selected plugins, ICU, GLib, HarfBuzz, image codecs,
xkbcommon and the xcb utility libraries, HDF5 with its compression and
network libraries, VOLK, protobuf, SoapySDR, OpenMP (libgomp), the CUDA
runtime (static), Python 3.12 with its standard library, NumPy, protobuf,
grpcio and typing_extensions, and Resonant itself. The full list with
versions is in the package's `THIRD_PARTY_NOTICES.txt` and
[the license audit](LICENSE_AUDIT.md).

## Required from the system

Every desktop installation has these; a minimal or container installation
may not. `./resonant` names any that is missing and the package to install.

| Family | Why it is not bundled | Debian / Ubuntu | Fedora | Arch |
|---|---|---|---|---|
| C library | must match the system's loader, NSS and locales | `libc6`, `libcrypt1` | `glibc`, `libxcrypt` | `glibc`, `libxcrypt` |
| C++ and GCC runtime | GPU drivers loaded into the process need the system's | `libstdc++6`, `libgcc-s1` | `libstdc++`, `libgcc` | `gcc-libs` |
| OpenGL, EGL (libglvnd) and the GPU driver | must match the kernel driver and the hardware | `libgl1`, `libopengl0`, `libegl1` (+ `libgl1-mesa-dri` for Mesa) | `libglvnd-glx`, `libglvnd-opengl`, `libglvnd-egl`, `mesa-dri-drivers` | `libglvnd`, `mesa` |
| X11 and xcb | shared with the GL driver, follow the X server | `libx11-6`, `libx11-xcb1`, `libxcb1` and its extensions, `libsm6`, `libice6` | `libX11`, `libX11-xcb`, `libxcb`, `libSM`, `libICE` | `libx11`, `libxcb`, `libsm`, `libice` |
| Fonts | the system's font configuration and fonts | `libfontconfig1`, `libfreetype6`, a font (`fonts-dejavu-core`) | `fontconfig`, `freetype`, `dejavu-sans-fonts` | `fontconfig`, `freetype2`, `ttf-dejavu` |
| Sound (ALSA library) | its plugins and configuration are the system's | `libasound2t64` (`libasound2` before Ubuntu 24.04) | `alsa-lib` | `alsa-lib` |
| zlib, expat | base libraries of every distribution | `zlib1g`, `libexpat1` | `zlib-ng-compat`, `expat` | `zlib`, `expat` |

The exact list the clean containers were given is in
`tools/release/clean_machine_test.sh`.

## Optional system integrations

| What | Needs | Without it |
|---|---|---|
| Native Wayland | `libwayland-client`, `libwayland-cursor`, `libwayland-egl` | XWayland is used (X11 platform) |
| GPU compute (CUDA backend) | the NVIDIA driver (`libcuda.so.1`) | the CPU backends |
| SDR devices (discovery) | SoapySDR device modules installed for SoapySDR 0.8 | no SDR devices are listed |
| Audio interfaces | ALSA devices | no audio interface is listed |
| Input methods | IBus | the compose input method |
| HTTPS from Python scripts | the system's CA certificates (`SSL_CERT_FILE` on non-Debian systems) | Resonant itself makes no network connection |

## Graphics notes

- Minimum OpenGL 4.5 core. Tested: Mesa (Intel Arrow Lake integrated GPU) 4.6 core,
  NVIDIA 595 driver (PRIME render offload) 4.5 core, Mesa llvmpipe 4.5 core
  (software, in Xvfb and with `--software-gl`). AMD hardware was not
  available for testing.
- `./resonant --software-gl` forces Mesa's software renderer (slower) on
  systems whose GPU driver fails; it is never chosen automatically.
- On hybrid-graphics laptops the integrated GPU is used by default; run with
  `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./resonant`
  (or your desktop's "launch using the discrete GPU") for the NVIDIA GPU.
- Remote X11 (ssh -X) usually lacks OpenGL 4.5: `--check-runtime` says so.

## Unsupported environments

32-bit or ARM systems, musl (Alpine), glibc older than 2.38, systems without
a display server (use the headless `bin/resonant-automation` there),
Windows Subsystem for Linux without WSLg, OpenGL ES-only devices.

## Recommended

A current desktop distribution (Ubuntu 24.04 or newer, Fedora 41 or newer)
on x86_64 with a GPU driver providing OpenGL 4.5 (Mesa 21+ or NVIDIA), 8 GB
of memory or more for full-wave simulations, and 300 MB of disk for the
extracted package.
