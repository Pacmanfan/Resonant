# Release Report: Resonant 0.1.0 for Linux (ZIP)

The record of the first Linux ZIP (REQ-CHAT-20261126-013, RES-DIST-001..015,
ADR-000218): what was built, how it was validated, where it was tested, and
the status of every acceptance criterion. The evidence record of the work
item holds the run records and the validation report.

## The archive

| | |
|---|---|
| Output | `dist/RESONANT-Linux-x86_64-0.1.0.zip` and `dist/RESONANT-Linux-x86_64-0.1.0.sha256` |
| Size | 94 873 838 bytes (94.9 MB); 254 MB extracted, 2 454 entries, 2 159 files |
| SHA-256 (validated build) | `0e057fea6fd219d3e880ac0b020f0b3aff354a0bc0b067fcb1fced8e192a8083` |
| Application version | 0.1.0 |
| Commit | built from `4bc4168` plus this request's uncommitted changes (`resonant-runtime.json` says `-dirty`); a rebuild after the commit names the commit and has a different checksum |
| Reproducibility | packaging the same build twice (different staging folders) gave byte-identical ZIPs |

## Build environment and toolchain

Ubuntu 24.04.5 LTS (x86_64, glibc 2.39), GCC 13.3.0, CMake 3.28.3, Ninja,
Qt 6.4.2, Python 3.12.3, protobuf 3.21.12, CUDA 13.5 (static runtime),
pybind11 2.11.1, Eigen 3.4.0; packaging with `cmake --install --strip`,
`tools/release/package.py` (readelf, the loader's cache and trace mode,
patchelf 0.18, dpkg-query, Python's zipfile), validation with unzip,
sha256sum, xvfb-run and Node.js (browser search), clean machines with
Docker. Command: `tools/release/build_linux_zip.sh`.

## Contents

- **Bundled**: the application and the headless automation host; Qt 6.4.2
  (10 libraries, 16 plugins); 68 further libraries (ICU, GLib, HarfBuzz,
  image codecs, xkbcommon, xcb utilities, HDF5 and its network stack, VOLK,
  protobuf, SoapySDR, OpenMP and others); Python 3.12.3 with its standard
  library, NumPy 2.2.6, protobuf 6.33.5, grpcio 1.78.0, typing_extensions
  4.15.0; the help (122 topics, `help/index.html`), 43 example projects, 8
  demonstrations, 4 example plugins; licenses of 60 components.
- **From the system** (33 libraries in 8 families): the C library, the C++
  runtime, OpenGL/EGL (libglvnd) and the GPU driver, X11/xcb, Wayland
  (optional), fontconfig and FreeType, ALSA, zlib and expat. Minimum glibc 2.38.

## Validation of the final ZIP

`tools/release/validate_bundle.py` on the ZIP: **41 checks passed, 0
failed** (40 s). Static: checksum (also `sha256sum -c`); one top-level
folder, no absolute or escaping entries, no links; extraction with `unzip`
into a folder with spaces; executable bits of the launchers, both programs
and the Python interpreter; no development files; no private data (no home,
repository or staging path in any file, no key or token); 154 ELF files,
all ELF64 x86-64 with only `$ORIGIN`-relative DT_RPATH; every dependency
resolves inside the package or to a system library of the policy, in a
sanitized environment; all 16 Qt plugins; required resources and the native
module; the package's own file list (`sha256sum -c diagnostics/FILES.sha256`);
the help (122 topics, 1 661 internal links, 39 images, no remote resources,
browser search); the licenses (60 components, every library, plugin and
Python package accounted for).

Runtime, under Xvfb (Mesa llvmpipe, OpenGL 4.5 core) in a sanitized
environment (`env -i`, scratch HOME, `LD_LIBRARY_PATH` set to a probe
value): the launcher's `--check-runtime` (0 failed); `--diagnostics
--report` (home shown as `~`, the probe value not shown); `--check-help`;
the main window with the viewport (screenshot); all 43 example projects
open; all 8 demonstrations complete (H drives the bundled Python
interpreter); the bundled Python with NumPy drives the headless host over the
local socket and through the gRPC bridge, with `sys.path` inside the
package; relocated (renamed, another folder, through a symbolic link, from
another current directory); `resonant.sh`; the package read-only; nothing
written into the package by any of these runs; user data under the scratch
HOME; a ZIP extracted without permissions is refused with the reason.

## Clean machines

`tools/release/clean_machine_test.sh` (containers; the desktop images have
only the declared system packages, Mesa and Xvfb, and run **without
network**):

| Image | System | Test | Result |
|---|---|---|---|
| ubuntu:24.04 | Ubuntu 24.04.5 LTS, x86_64, glibc 2.39 | bare image (package mounted read-only) | PASS: refused, 19 missing system libraries named with their packages |
| ubuntu:24.04 | | desktop image, offline | PASS: unzip, `--check-runtime` 0 failed (llvmpipe, LLVM 20.1.2, OpenGL 4.5 core), help, main window, 43 of 43 projects, 8 of 8 demonstrations, restart, relocation |
| fedora:41 | Fedora Linux 41, x86_64, glibc 2.40 | bare image | PASS: refused, 19 libraries named |
| fedora:41 | | desktop image, offline | PASS: the same (llvmpipe, LLVM 19.1.7) |
| fedora:42 | Fedora Linux 42, x86_64, glibc 2.41 | bare image | PASS: refused, 19 libraries named |
| fedora:42 | | desktop image, offline | PASS: the same (llvmpipe, LLVM 20.1.8) |
| ubuntu:22.04 | Ubuntu 22.04.5 LTS, glibc 2.35 | bare image | PASS: refused, "this system has glibc 2.35; this package needs glibc 2.38 or newer" |
| debian:bookworm | Debian 12, glibc 2.36 | bare image | PASS: refused with the glibc versions |

None of the images had Qt; Python was absent from the Ubuntu images and the
bundled interpreter was used everywhere.

**Real desktop session.** On the build machine's X11 desktop (not a clean
system) `--check-runtime` obtained OpenGL 4.6 core from Mesa on the Intel
Arrow Lake GPU, 4.5 core from the NVIDIA 595 driver (PRIME offload) and 4.5
core from llvmpipe with `--software-gl`; no window was opened there. A
clean machine with a real desktop session and GPU (a VM or second computer)
was **not available**: containers with Xvfb stand in for it and do not
exercise GPU drivers, compositors or Wayland.

## Acceptance criteria

| ID | Criterion | Status | Evidence |
|---|---|---|---|
| PKG-001 | A versioned x86_64 Linux ZIP is generated | PASS | `dist/RESONANT-Linux-x86_64-0.1.0.zip` |
| PKG-002 | ZIP contains the runnable executable | PASS | validation: executables, main window |
| PKG-003 | Application shared libraries bundled | NOT APPLICABLE | Resonant has no shared libraries of its own (static libraries linked into the programs) |
| PKG-004 | Qt runtime libraries bundled | PASS | 10 Qt libraries; "Qt libraries" check of `--check-runtime` |
| PKG-005 | Qt plugins bundled | PASS | 16 plugins, all resolve |
| PKG-006 | Third-party dependencies resolved | PASS | "dependencies resolve", "nothing from the development environment" |
| PKG-007 | Runtime paths relocatable | PASS | relative DT_RPATH; relocation checks |
| PKG-008 | No development-directory paths | PASS | "no private data or machine paths" |
| PKG-009 | Launcher works without environment setup | PASS | sanitized-environment runs; clean containers |
| PKG-010 | OpenGL viewport starts on a supported system | PASS | screenshots under Mesa llvmpipe; OpenGL 4.5/4.6 contexts with Intel Mesa and NVIDIA drivers (no window there) |
| PKG-011 | Offline help fully included and functional | PASS | help validation, `--check-help`, containers without network |
| PKG-012 | Help navigation and search work offline | PASS | in-app search (`--check-help`), browser search (Node.js) |
| PKG-013 | Shaders, fonts, icons and resources included | PASS | shaders compiled in, fonts from the system (fontconfig), no icon files exist; resources check |
| PKG-014 | Example projects open | PASS | 43 of 43, also in three clean containers |
| PKG-015 | Demo projects and sequences work | PASS | all 8 demonstrations from the package (validation and three clean containers) |
| PKG-016 | Python works as documented | PASS | bundled interpreter, NumPy, the worker (demonstration H), client and bridge |
| PKG-017 | Solver and plugin discovery works | PASS | `--check-runtime` lists compute backends and 4 plugins from the package |
| PKG-018 | User data outside the bundle | PASS | "nothing written into the package", read-only package |
| PKG-019 | Runs from a different directory | PASS | relocation checks |
| PKG-020 | Runs from a path with spaces | PASS | every validation run |
| PKG-021 | Sanitized environment test | PASS | all runtime checks run under `env -i` |
| PKG-022 | Clean Linux environment test | PASS (containers) | three distributions offline; a clean machine with a real GPU desktop was not available |
| PKG-023 | Licensing and notices included | PASS (third parties); application license **BLOCKED** | 60 components; Resonant's own license is the owner's decision |
| PKG-024 | SHA-256 generated and verified | PASS | `.sha256`, `sha256sum -c` |
| PKG-025 | Release documentation complete | PASS | `docs/release/*`, `README.txt` |
| PKG-026 | Packaging pipeline automated | PASS | `build_linux_zip.sh`, `package_linux_zip` |
| PKG-027 | No private development data | PASS | private-data scan |
| PKG-028 | Final package passes automated validation | PASS | 41 of 41 |

## Known limitations and open items

1. **Application license**: none chosen; `LICENSE` says so. Needed before publishing.
2. **LGPL sources**: publish the corresponding source packages of the LGPL
   components next to the ZIP for a public release ([license audit](LICENSE_AUDIT.md)).
3. **Baseline**: glibc 2.38+ only; older distributions need a build on an older baseline.
4. **Clean desktop with a real GPU** and **native Wayland** were not tested; AMD GPUs not available.
5. SoapySDR on the build machine is a source build; device modules are not bundled.
6. HTTPS from Python scripts may need `SSL_CERT_FILE` outside Debian/Ubuntu.
7. The framework's `package` test lane and `AGENTS.md`'s `Deployment` section are protected placeholders; wiring them to `tools/release/` is a process change.
