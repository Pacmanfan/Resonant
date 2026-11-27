# Packaging Audit: the Linux ZIP

The audit behind the self-contained Linux ZIP (REQ-CHAT-20261126-013,
`docs/requirements/distribution.md` RES-DIST-001..015, ADR-000218). It
describes the repository and the build as found, and the measured
dependency graph of release 0.1.0. The tooling it led to is described in
[the packaging decisions](PACKAGING_DECISIONS.md); the result in
[the release report](RELEASE_REPORT.md).

## 1. Build configuration

| | Development | Release (the ZIP) |
|---|---|---|
| Build system | qmake (`project.pro`, used by the development-control system) and CMake (`CMakeLists.txt`, ADR-000204) | CMake only |
| Configuration | Debug (qmake), Debug or Release (CMake) | `-DCMAKE_BUILD_TYPE=Release -DRESONANT_BUILD_TESTS=OFF -DRESONANT_BUNDLE_LAYOUT=ON` |
| Toolchain on the build machine | Ubuntu 24.04.5 LTS, GCC 13.3.0, CMake 3.28.3, Ninja, Qt 6.4.2, Python 3.12.3, protobuf 3.21.12, CUDA 13.5 | the same |
| Optional features found | VOLK, CUDA (static runtime), HDF5, ALSA, SoapySDR, the Python module (pybind11 2.11.1), the automation API | all on |

Resonant's own code is built as static libraries (`resonant_core`,
`resonant_render`, `resonant_hardware`, `resonant_app`,
`resonant_automation_api`) linked into the programs; there is no
project-owned shared library to deploy.

## 2. Executables and shared libraries

| File | What | Size (stripped) |
|---|---|---|
| `bin/resonant` | the desktop application | 16 MB |
| `bin/resonant-automation` | the headless automation host (ADR-000216) | |
| `python/resonant/_resonant.cpython-312-x86_64-linux-gnu.so` | the native Python module (pybind11, built for CPython 3.12) | 12 MB |

`bin/resonant` needs directly: `libasound.so.2`, `libSoapySDR.so.0.8-3`,
`libQt6OpenGLWidgets`, `libQt6Widgets`, `libQt6OpenGL`, `libQt6Gui`,
`libQt6Core`, `libhdf5_serial.so.103`, `libvolk.so.3.1`,
`libprotobuf.so.32`, `libstdc++`, `libm`, `libgcc_s`, `libc`.

## 3. Runtime dependency inventory

Resolved for every ELF file in the package (programs, the native module,
the Qt plugins, the Python interpreter and its extension modules, the
wheels' modules) with `readelf -d` and the loader's cache, transitively
through every library that is bundled (`tools/release/package.py`,
`tools/release/inspect_dependencies.sh`):

- **78 libraries bundled** (`lib/`): Qt (Core, Gui, Widgets, OpenGL,
  OpenGLWidgets, DBus, XcbQpa, Svg, WaylandClient,
  WaylandEglClientHwIntegration), ICU 74, GLib, PCRE2, HarfBuzz, Graphite2,
  libpng, libjpeg-turbo, double-conversion, md4c, libb2, D-Bus, libsystemd,
  libcap, libgcrypt, libgpg-error, LZ4, XZ, Zstandard, Brotli, bzip2,
  libxkbcommon(-x11), xcb-util (icccm, image, keysyms, render-util, util),
  HDF5 with libaec/libsz and its network stack (libcurl, OpenSSL, nghttp2,
  libpsl, libidn2, libunistring, librtmp, GnuTLS, Nettle, GMP, libtasn1,
  p11-kit, libssh, MIT Kerberos, com_err, keyutils, OpenLDAP, Cyrus SASL),
  VOLK with Orc, protobuf, SoapySDR, libgomp, and for Python's standard
  library libffi, SQLite, ncurses.
- **33 libraries left to the system**, by family (the policy in
  `tools/release/bundle_policy.json`, reasons in
  [the compatibility baseline](LINUX_COMPATIBILITY.md)):

| Family | Libraries | Needed by |
|---|---|---|
| C library | `ld-linux-x86-64.so.2`, `libc`, `libm`, `libpthread`, `librt`, `libresolv`, `libcrypt` | everything |
| C++ and GCC runtime | `libstdc++.so.6`, `libgcc_s.so.1` | the programs, Qt, the native module |
| OpenGL, EGL and GPU drivers | `libOpenGL.so.0`, `libGLX.so.0`, `libEGL.so.1` | Qt Gui, the GLX/EGL integrations |
| X11 and xcb | `libX11`, `libX11-xcb`, `libxcb` and 9 of its extensions, `libSM`, `libICE` | the xcb platform plugin (Qt XcbQpa) |
| Wayland | `libwayland-client`, `libwayland-cursor`, `libwayland-egl` | the Wayland platform plugin only |
| Fonts | `libfontconfig.so.1`, `libfreetype.so.6` | Qt Gui, HarfBuzz |
| Sound | `libasound.so.2` | `bin/resonant` (the ALSA driver) |
| Base libraries | `libz.so.1`, `libexpat.so.1` | Qt, Python |

Found only when a feature is used:

- **CUDA**: the static CUDA runtime in the programs opens the NVIDIA driver
  (`libcuda.so.1`) with `dlopen` when a GPU backend is chosen; without it
  the CPU backends are used (ADR-000212).
- **SoapySDR**: loads device modules from its compiled-in prefix; none are
  bundled (discovery only, RES-EXP-*).
- **ALSA**: loads its own plugins and configuration from the system.
- **OpenGL**: libglvnd loads the vendor's driver (Mesa or NVIDIA).
- **Native Resonant plugins**: `dlopen` of a plugin the user installed and
  enabled (never without explicit configuration, ADR-000215).

`ldd` alone would have missed the Qt plugins, Python's extension modules
and the wheels' libraries: they are not dependencies of `bin/resonant`.

## 4. Qt components and plugins

Modules used: Core, Gui, Widgets, OpenGL, OpenGLWidgets (the viewport is a
`QOpenGLWidget`), plus DBus (by Gui), XcbQpa, Svg and WaylandClient through
plugins. Not used: Network, Concurrent, XML, Print Support, SQL, QML.

| Plugin type | Bundled | Not bundled |
|---|---|---|
| platforms | `xcb`, `wayland-egl`, `wayland-generic`, `offscreen` (runtime check without a display) | `eglfs`, `linuxfb`, `minimal`, `minimalegl`, `vnc`, `vkkhrdisplay` |
| xcbglintegrations | `glx`, `egl` | |
| imageformats | `jpeg`, `gif`, `ico`, `svg` | `icns`, `mng`, `tga`, `tiff`, `wbmp`, `webp` |
| iconengines | `svg` | |
| platforminputcontexts | `compose`, `ibus` | |
| wayland | `xdg-shell`, `bradient` decorations, `wayland-egl` graphics integration | other shells, server-side integrations |
| platformthemes | none: `gtk3` would load the system's GTK and GLib into the process | `gtk3` |
| others | none | `tls`, `sqldrivers`, `printsupport`, `networkinformation`, `egldeviceintegrations`, `generic`, `qmltooling`, `designer` |

## 5. Dynamic loading mechanisms

- Qt plugins: found through `bin/qt.conf` (`Prefix = ..`, `Plugins = lib/qt6/plugins`).
- Python: a separate worker process (`python3 -m resonant.worker`) for the
  console, the script editor and Python plugins; the gRPC bridge is another
  process (`python3 -m resonant_api.bridge`). Before this change both ran
  whatever `python3` was on `PATH`.
- `dlopen`: CUDA driver, SoapySDR modules, native Resonant plugins, ALSA plugins, the GL vendor library.
- External programs started by the application: only the Python worker and
  the bridge (no browser, no `xdg-open`).

## 6. Runtime resources

| Resource | Where | Notes |
|---|---|---|
| Help | `help/` (121 topics, 23 image files, navigation, search index) | generated from `help/src` at build time; the release build fails on a missing screenshot |
| Example projects | `examples/` (43 projects, 8 domains) | all open in the packaged application |
| Demonstrations | `demos/` (8 profiles and sequences, presenter notes, manifest) | |
| Example plugins | `plugins/examples/` (4) | disabled until enabled by the user |
| Python package, examples, templates | `python/` | |
| Automation descriptor set | `automation/resonant_api.desc` | read by the Python client and the bridge |
| Icons, fonts, shaders, themes | none as files | shaders are compiled into the program, fonts come from fontconfig, icons are Qt's |

Development-machine assumptions found and fixed:

- `RESONANT_SOURCE_DIR` (the source tree's absolute path) was compiled into
  the application; a bundle build no longer defines it, and
  `-ffile-prefix-map` removes source and build paths from what the compiler
  embeds.
- The help's `build-report.json` recorded the build directory's absolute path.
- The Python worker and the gRPC bridge ran `python3` from `PATH`.
- Screenshot links used the in-app `zoom:` scheme, broken in a browser.
- `cmake --install` copied `__pycache__` folders of the example plugins from the source tree.

## 7. Python

Python is not needed to start Resonant; it is needed for the console, the
script editor, workflows with Python steps, Python plugins, demonstration H
and the gRPC bridge. The native module is built for CPython 3.12's ABI, so
another interpreter (for example Python 3.13 on Fedora 41) cannot load it:
the interpreter is bundled. Packages: NumPy 2.2.6 (results as arrays),
protobuf 6.33.5 and grpcio 1.78.0 (the automation client and bridge),
typing_extensions 4.15.0 (needed by grpcio).

## 8. Help and demonstration assets

The help's entry point for browsers did not exist (the in-app viewer reads
`index.json` and `search.json`); `help/index.html` with an offline search
was added. The demonstrations depend only on the example projects
(`demos/manifest.json`); demonstration G is marked partial in its profile.

## 9. External system dependencies

The families in section 3; see [the compatibility baseline](LINUX_COMPATIBILITY.md).

## 10. Licensing and redistribution

See [the license audit](LICENSE_AUDIT.md). Points that shaped the
package: Qt and several libraries are LGPL (shipped as unmodified shared
libraries, replaceable); Python's `readline` and `gdbm` modules link
GPL-licensed libraries and are left out; libgomp and NumPy's libgfortran
carry the GCC Runtime Library Exception; the CUDA runtime is redistributable
under the CUDA EULA; Resonant's own code has no license chosen yet.

## 11. Known packaging risks

- **C library baseline**: built on Ubuntu 24.04, the package needs glibc
  2.38 or newer; distributions with older glibc are refused by the launcher.
- **libstdc++ from the system**: the package needs a libstdc++ from GCC 13
  or newer (present wherever glibc 2.38 is).
- **SoapySDR** on the build machine is built from source in `/usr/local`; a
  release machine should use the distribution's package for reproducibility.
- **OpenSSL configuration**: the bundled OpenSSL looks for its configuration
  and certificates where Debian keeps them; HTTPS from Python scripts may
  need `SSL_CERT_FILE` on other distributions (Resonant itself makes no
  network connection).
- **Native Wayland** was not exercised (containers and the build machine run
  X11); on Wayland sessions the launcher prefers XWayland when it exists.
- **Size**: 95 MB compressed, 254 MB extracted (ICU's data 30 MB, NumPy 25
  MB, grpcio 17 MB, Qt Gui 8 MB).

## 12. Recommended architecture (implemented)

CMake install rules into a bundle layout, then a project bundler (readelf,
the loader, patchelf) with an explicit system-library policy, relative
DT_RPATH everywhere, `qt.conf`, a bundled and isolated Python, a curated
component table for licenses, a POSIX launcher with pre-load checks,
diagnostics in the application, and validation of the final ZIP, also in
clean containers. Details in [the packaging decisions](PACKAGING_DECISIONS.md).
