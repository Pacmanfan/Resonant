# ADR-000218: Linux ZIP distribution — CMake install into a bundle layout, a project bundler (readelf, the dynamic loader, patchelf) with an explicit system-library policy, DT_RPATH, a bundled and isolated Python, a curated component table for licenses, and validation of the final ZIP

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/distribution.md` RES-DIST-001..015
**Request:** REQ-CHAT-20261126-013
**Builds on:** ADR-000204 (CMake build beside qmake), ADR-000215 (installed help and data, Python worker), ADR-000216 (headless host and gRPC bridge)

## Context

Resonant had to be delivered as one ZIP that runs on a Linux system without
Qt, Python packages or development libraries. The audit
(`docs/release/PACKAGING_AUDIT.md`) found: an installation layout already
existed in CMake (`bin/`, `share/resonant/`) but nothing collected the runtime;
about 120 shared libraries are reachable from the application, its Qt
plugins and Python's extension modules; Python ran as whatever `python3` was
on `PATH`, which need not match the version the native module was built for;
the gRPC bridge was started as `python3` directly; the help was built in draft
mode; no license information for bundled components existed. The packaging
tools evaluated are compared in `docs/release/PACKAGING_DECISIONS.md`.

## Decisions

1. **CMake installs; the project bundler completes the runtime.** The
   application, the headless host, the native module, the help, examples,
   demonstrations and Python packages are installed by CMake install rules
   with `RESONANT_BUNDLE_LAYOUT=ON` (data at the top level of the package
   instead of `share/resonant`). `tools/release/package.py` then resolves
   every ELF file's dependencies (readelf for `DT_NEEDED`, the system dynamic
   loader for resolution), copies what the policy says to bundle, selects Qt
   plugins by name and writes search paths with patchelf. linuxdeploy and its
   Qt plugin are not installed here (and would be downloaded binaries);
   linuxdeployqt refuses a system newer than the oldest supported Ubuntu LTS
   and targets Qt 5; CMake's `file(GET_RUNTIME_DEPENDENCIES)` resolves but has
   no notion of Qt plugins, Python or a system-library policy; Qt 6.4 has no
   Linux deployment API (added in 6.5).
2. **An explicit system-library policy.** `tools/release/bundle_policy.json`
   lists what is never bundled, each family with its reason: the C library
   family (ABI of the system), libstdc++ and libgcc_s (Mesa's drivers load the
   system's; an older bundled copy breaks them), the OpenGL/EGL/GLX/glvnd and
   DRM driver interface, the X11/xcb core libraries (shared with the GL
   driver), fontconfig and FreeType (configuration and caches of the system),
   ALSA (its plugins and configuration live in the system) and zlib/expat
   (present everywhere, stable). Everything else reachable is bundled; an
   unclassified dependency fails the build.
3. **DT_RPATH, not RUNPATH.** Every bundled ELF file gets a relative
   `$ORIGIN` DT_RPATH, which the loader consults before `LD_LIBRARY_PATH`, so a
   user's environment cannot substitute another Qt; the launcher therefore
   does not set `LD_LIBRARY_PATH`, and programs Resonant starts (a browser,
   `xdg-open`) do not inherit the package's libraries. Qt finds its plugins
   through `bin/qt.conf` (relative), so `bin/resonant` also works without the
   launcher.
4. **Python is bundled and isolated.** The interpreter the native module was
   built for (3.12), its standard library without tests, Tk, IDLE and the
   GPL-licensed readline and gdbm modules, NumPy, protobuf and grpcio are
   copied into `python/`. `PythonEnvironment` (the only place that decides how
   Python is started) gives the console, plugins and the gRPC bridge that
   interpreter with `PYTHONHOME`, a `PYTHONPATH` of the package only, no user
   site directory and no bytecode written (it is precompiled). Alternatives:
   the system's Python (version not under our control: the native module
   would fail on 3.13), a downloaded standalone Python (not reproducible
   offline here).
5. **The bundle is recognised by its runtime manifest.** `resonant-runtime.json`
   at the package root marks a bundle and records the build (commit, versions,
   tools, time). `ResourcePaths` finds every read-only resource relative to
   the executable and keeps user data in the XDG directories.
6. **Diagnostics in the application, checks in the launcher.** The launcher
   (POSIX shell) checks what must hold before the executable can load
   (architecture, C library, display, system libraries through the dynamic
   loader's list mode) and names the missing package; `--check-runtime` and
   `--diagnostics` in the application check what needs Qt (platform plugin,
   OpenGL 4.5 core context, help, Python, compute backends, plugins, package
   integrity). No telemetry.
7. **Licenses from a curated table.** Debian copyright files list every
   license in a source package (build scripts, tests), so they do not say
   what the library is under. `tools/release/third_party.json` records each
   component's license, version source, origin and linking by hand; the
   bundler copies the component's license files and fails on a component that
   is missing from the table or whose license is not accepted. Resonant's own
   code has no license chosen yet; the package says so.
8. **The final ZIP is what is validated.** Validation extracts the ZIP with
   `unzip` and checks it statically and by running it (relocated, renamed,
   from a path with spaces, in a sanitized environment, and in clean
   containers with only the declared system packages).

## Consequences

- The package runs on x86_64 systems with glibc 2.39 or newer (the build
  system's); older distributions need a build on an older baseline, which this
  machine cannot provide. `docs/release/LINUX_COMPATIBILITY.md` states it.
- The ZIP is about three times larger than the application alone: Qt, ICU,
  HDF5's network stack, Python and NumPy dominate.
- The qmake build is unchanged; the ZIP is made from the CMake build.
- The framework's `package` test lane and the `Deployment` section of
  `AGENTS.md` still hold placeholders; wiring them to these scripts is a
  process change (protected artifacts).
