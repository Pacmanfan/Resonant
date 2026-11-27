# Packaging Decisions: the Linux ZIP

Why the Linux ZIP is built the way it is (REQ-CHAT-20261126-013, ADR-000218,
RES-DIST-*). The facts behind these choices are in
[the packaging audit](PACKAGING_AUDIT.md).

## The toolchain

| Candidate | Maintained | Qt 6.4 | Non-Qt libraries | RPATH | Python | Fit for a ZIP | Verdict |
|---|---|---|---|---|---|---|---|
| linuxdeploy + linuxdeploy-plugin-qt | yes | yes | yes | yes | through separate plugins (conda or pip based) | produces an AppDir that could be zipped | not installed here; it is distributed as prebuilt AppImages, so the build would depend on binaries downloaded at build time; its exclusion list is built into the tool |
| linuxdeployqt | continuous builds only (build 107 installed here) | Qt 5 first; Qt 6 partly | yes | yes | no | AppDir | installed here, and refuses to run: "The host system is too new" (glibc newer than 2.35); overriding that check defeats its purpose |
| appimagetool | yes | n/a | n/a | n/a | n/a | makes AppImages, not ZIPs | not needed (the deliverable is a ZIP) |
| Qt's deployment API (`qt_generate_deploy_app_script`) | yes | Linux support from Qt 6.5 | partly | yes | no | yes | not available for Qt 6.4 |
| CMake `file(GET_RUNTIME_DEPENDENCIES)` | yes | no Qt plugin knowledge | yes | no | no | yes | resolves, but the plugin selection, the system policy and Python would still be custom |
| **CMake install rules + a project bundler (readelf, the loader, patchelf)** | ours | yes | yes | DT_RPATH by patchelf | yes, explicitly | yes | **chosen**: standard tools only, no downloads, one policy file, testable, fails on anything unclassified |

Tools used and their roles: CMake (`cmake --install --strip` into the
bundle layout), `readelf` (DT_NEEDED, search paths, symbol versions), the
loader's cache (`ldconfig -p`) and trace mode (resolution checks), `patchelf`
0.18 (`--force-rpath --set-rpath`), `dpkg-query` (which distribution
package and version each library comes from), Python's `zipfile`
(deterministic archive), `unzip` and `sha256sum` (validation).

## Layout

```text
RESONANT-Linux-x86_64-<version>/
  resonant, resonant.sh            launcher (POSIX sh)
  README.txt LICENSE THIRD_PARTY_NOTICES.txt VERSION CHANGELOG.txt
  resonant-runtime.json            marks the bundle; build record (RES-DIST-014)
  bin/resonant bin/resonant-automation bin/qt.conf
  lib/                             bundled libraries
  lib/qt6/plugins/<type>/          selected Qt plugins
  help/                            index.html (browsers), index.json, search.json, html/
  examples/ demos/ plugins/examples/
  python/resonant python/resonant_api python/examples python/templates
  python/runtime/                  bin/python3, lib/python3.12
  python/site-packages/            NumPy, protobuf, grpcio, typing_extensions
  automation/resonant_api.desc
  diagnostics/                     FILES.sha256, system-libraries.txt, README.txt
  licenses/application licenses/third_party/<component> licenses/common
```

Data sits at the top (not under `share/resonant`) so the folder is easy to
browse; `RESONANT_BUNDLE_LAYOUT=ON` sets the CMake install destinations.
Qt's plugins live under `lib/qt6/plugins` so that `plugins/` stays
Resonant's own.

## Search paths: DT_RPATH, not RUNPATH, and no LD_LIBRARY_PATH

The loader consults DT_RPATH before `LD_LIBRARY_PATH` and DT_RUNPATH after
it. With DT_RPATH (`$ORIGIN`-relative, one per file's depth) a user's
`LD_LIBRARY_PATH` that names another Qt cannot replace the package's, and the
launcher does not need to set `LD_LIBRARY_PATH`, which programs Resonant
starts would otherwise inherit. Wheels keep their own `$ORIGIN` entries and
are left unmodified when they need nothing of the package.

## What stays with the system

The policy (`tools/release/bundle_policy.json`) names families, each with a
reason, distribution packages for the launcher's hints, and patterns; the
reasons are in [the compatibility baseline](LINUX_COMPATIBILITY.md). The
rule "bundle everything else, fail on what cannot be found" keeps the
package complete without copying the graphics stack, the C library or the
C++ runtime. libstdc++ is deliberately not bundled: Mesa's drivers are
loaded into the process and need the system's.

## Graphics

No OpenGL library or driver is bundled. The launcher chooses
`QT_QPA_PLATFORM="xcb;wayland"` when an X server (also XWayland) exists and
`"wayland;xcb"` otherwise; `--software-gl` selects Mesa's llvmpipe through
`LIBGL_ALWAYS_SOFTWARE`, `GALLIUM_DRIVER` and libglvnd's vendor selection.
`--check-runtime` creates an OpenGL 4.5 core context on an offscreen surface
and reports what it got.

## Python

Bundled (the native module's ABI fixes the version), copied from the build
machine's interpreter with the standard library minus tests, Tk, IDLE,
pip's wheels, the embedding library and the GPL-linked `readline` and
`gdbm` modules. Third-party packages are copied file by file from their
installed RECORD, and the bundler fails if a bundled package requires one
that is not bundled. Bytecode is precompiled with hash-based invalidation,
so nothing is written into the package at run time
(`PYTHONDONTWRITEBYTECODE`). `PythonEnvironment` gives every Python process
`PYTHONHOME`, a `PYTHONPATH` of the package only and no user site
directory.

## Launcher and diagnostics

A POSIX shell launcher, because it must work before anything of the package
can load: it finds itself through links, checks the architecture, the C
library version (from the runtime manifest), the graphical session and
every system library of the program and the X11 plugin (the loader's trace
mode lists all missing libraries and too old symbol versions without
running anything), names the missing distribution packages, clears
`QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `PYTHONHOME`, `PYTHONPATH`
and `exec`s the program. What needs Qt (OpenGL, the help, Python, plugins,
the package's integrity) is checked by the program (`--check-runtime`,
`--diagnostics`).

## Licenses

A hand-curated table (`tools/release/third_party.json`): component, the
license it is distributed under, source, linking, and how bundled files map
to it (Debian source package, Python distribution, file name). Debian's
copyright files are copied, but they cannot say which license applies to
the library binary (they list every file of the source package). An
unlisted component or a license outside the accepted list fails the build.

## Reproducibility

The archive's entries are sorted and carry one timestamp
(`SOURCE_DATE_EPOCH`, by default the commit's time); permissions are
normalized (0755 for executables, 0644 otherwise). `resonant-runtime.json`
records the commit (with `-dirty` when built from uncommitted changes), the
compiler, CMake, Qt, Python, protobuf and CUDA versions, the packaging tools'
versions, every bundled component's version and the minimum glibc.
Remaining nondeterminism: the compiled programs themselves (no
reproducible-build flags beyond `-ffile-prefix-map`), and the distribution
packages present on the build machine at packaging time.

## Not done, and why

- No AppImage (the deliverable is a ZIP), no Flatpak, no distribution packages.
- No build on an older baseline: this machine provides only Ubuntu 24.04's
  toolchain; a container build on an older distribution would need its own
  Qt 6 build.
- No desktop-menu integration: a `.desktop` file needs an absolute path; the
  README explains starting from the folder and linking the launcher.
- The framework's `package` test lane and the `Deployment` section of
  `AGENTS.md` are protected process artifacts; wiring them to these scripts
  is a separate process change.
