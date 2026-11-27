# Distribution: the Self-Contained Linux ZIP

**Owner of:** `RES-DIST-001..015`
**Source:** REQ-CHAT-20261126-013 (Prompt 16, "Self-Contained Linux ZIP Distribution")
**Refines:** `automation.md` RES-HELP-012 (the help is packaged with the application), `automation_api.md` RES-API-PB-003 (the headless host and the bridge ship with the application)
**Decisions:** ADR-000218
**Documents:** `docs/release/PACKAGING_AUDIT.md`, `docs/release/PACKAGING_DECISIONS.md`, `docs/release/LINUX_COMPATIBILITY.md`, `docs/release/LICENSE_AUDIT.md`, `docs/release/LINUX_ZIP_DISTRIBUTION.md`, `docs/release/RELEASE_REPORT.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

The acceptance criteria of the request (PKG-001..028) are mapped to these
requirements in the table at the end.

### RES-DIST-001 — One versioned ZIP from one command

`tools/release/build_linux_zip.sh` builds the Release configuration with
CMake in its own tree, installs it into a clean staging directory, bundles
the runtime, validates it and writes `dist/RESONANT-Linux-x86_64-<version>.zip`
and `dist/RESONANT-Linux-x86_64-<version>.sha256`. The archive holds one
top-level directory `RESONANT-Linux-x86_64-<version>/`. The same steps are the
CMake target `package_linux_zip` of a tree configured with
`RESONANT_BUNDLE_LAYOUT=ON`. A Debug tree is refused. No sources, object files,
static libraries, CMake files, tests or caches are packaged.
*Verification:* `ReleaseTools.test_*` (python/tests/test_release_tools.py); `tools/release/validate_bundle.py` on the final ZIP (evidence).

### RES-DIST-002 — The runtime is bundled by an explicit policy

Every ELF file in the package (executables, the Python interpreter and its
extension modules, Qt plugins, bundled libraries) has its dependencies
resolved by the bundler. A dependency is either bundled into `lib/` or is on
the policy's list of system libraries (`tools/release/bundle_policy.json`):
the C library family, the C++ and GCC runtime, the OpenGL/EGL/GLX driver
interface, the X11/xcb core, fontconfig and FreeType, ALSA, zlib and expat.
Each system entry states why it is not bundled. A dependency that is neither
fails the build. Qt plugins are chosen by name, not copied wholesale.
*Verification:* `ReleaseTools.test_policy_*`; the bundle validation's dependency check (evidence).

### RES-DIST-003 — Relocatable

Every ELF file in the package finds its bundled libraries through a relative
`$ORIGIN` search path (DT_RPATH, so a user's `LD_LIBRARY_PATH` cannot
substitute foreign Qt or Python libraries); Qt finds its plugins through a
relative `bin/qt.conf`. No file of the package names the staging, build or
source directory, and no symbolic link points outside the package. The
package runs from any directory, renamed, from a path with spaces, and from
any current directory.
*Verification:* `ReleaseTools.test_rpath_*`; the bundle validation's relocation checks (evidence).

### RES-DIST-004 — One place decides where things are

`ResourcePaths` distinguishes the executable's directory, the layout
(bundle, installation, build tree), the read-only resources (help, examples,
demonstrations, example plugins, the Python package, the bundled Python
runtime, the automation descriptor set) and the user's writable directories
(configuration, data, cache, documents), which are never inside the
installation. A bundle is recognised by its `resonant-runtime.json` beside
`bin/`. Environment variables (`RESONANT_*`) still override each path.
*Verification:* `RuntimePathsTest.*` (tests/app/test_runtime_paths.cpp).

### RES-DIST-005 — The launcher

`resonant` (and `resonant.sh`, which runs it) finds its own directory
(following symbolic links to it), clears the variables that would load a
foreign Qt or Python (`QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`,
`PYTHONHOME`, `PYTHONPATH`), checks the architecture, the C library version,
the graphical session and that every system library resolves, and only then
starts `bin/resonant` with the arguments unchanged, preserving its exit
status. A failed check names what is missing and which distribution package
provides it, and exits non-zero. `--software-gl` selects Mesa's software
renderer explicitly; it is never chosen silently.
*Verification:* `ReleaseTools.test_launcher_*`; the clean-machine checks (evidence).

### RES-DIST-006 — Diagnostics without telemetry

`resonant --diagnostics [--report FILE]` reports the version, build, system,
C library, Qt runtime and platform plugin, the libraries actually loaded and
where they came from, the OpenGL context obtained, Python, the help, compute
backends, plugins, the user directories and the package's integrity.
`resonant --check-runtime` runs the checks without opening the main window
and exits non-zero when one fails. Reports replace the home directory by `~`
and never print environment values, credentials or project contents.
Nothing is uploaded.
*Verification:* `RuntimePathsTest.TheDiagnosticReport*`; `resonant --check-runtime` from the package (evidence).

### RES-DIST-007 — Graphics

The viewport needs OpenGL 4.5 core. The package bundles no OpenGL library or
GPU driver: it uses the system's (Mesa, NVIDIA, AMD, Intel through libglvnd).
On X11 the xcb platform with the GLX and EGL integrations is used; on Wayland
the Wayland platform with the xcb platform as fall-back. An insufficient
OpenGL version is reported by `--check-runtime` with the version obtained.
*Verification:* `--check-runtime` on the development machine's GPU and under Mesa's software renderer (evidence).

### RES-DIST-008 — Python is bundled and isolated

The package contains the Python runtime the native module was built for
(interpreter, standard library without its test suite, Tk, IDLE and the
GPL-licensed readline and gdbm modules), NumPy, protobuf, grpcio, the
`resonant` and `resonant_api` packages and the native module. The console,
the script editor, plugins and the gRPC bridge use this interpreter with
`PYTHONHOME` and `PYTHONPATH` set to the package, no user site directory and
no bytecode written; bytecode is precompiled. `RESONANT_PYTHON` still selects
another interpreter explicitly.
*Verification:* `RuntimePathsTest.TheBundledPythonIsIsolated`; the bundle validation's Python check (evidence).

### RES-DIST-009 — The offline help is complete

The package's help is built without draft mode (a missing screenshot fails
the release). Besides the in-app help it has `help/index.html`: the
contents and a search over the same index for any browser, offline. Release
validation checks that every page, image, style sheet and script named by a
page exists in the package, that no page needs a remote resource, and that
`resonant --check-help` passes from the package.
*Verification:* `HelpBrowserIndex.*` (python/tests/test_release_tools.py); `tools/release/validate_help.py` (evidence).

### RES-DIST-010 — Examples and demonstrations work from the package

Every example project in the package opens in the packaged application
(`resonant --check-projects <folder>`), demonstrations run from the package
with their outputs in the user's data folder, and compute backends and
plugins are discovered from the package.
*Verification:* the bundle validation's runtime checks (evidence).

### RES-DIST-011 — Every bundled component is accounted for

`tools/release/third_party.json` names every component the package contains
(bundled libraries by distribution package, Python packages, compiled-in
libraries) with its version, license, source and linking. The bundler copies
each component's license files to `licenses/third_party/` and writes
`THIRD_PARTY_NOTICES.txt`; a component missing from the table, or whose
license is not on the table's list of accepted licenses, fails the build.
*Verification:* `ReleaseTools.test_license_*`; `tools/release/validate_licenses.py` (evidence).

### RES-DIST-012 — The final ZIP is validated

`tools/release/validate_bundle.py` extracts the final ZIP with `unzip` and
checks: the checksum; executable bits; ELF architecture and search paths;
every dependency resolves inside the package or to an allowed system
library, also for every Qt plugin; no forbidden files, absolute paths,
escaping links or private data; the help; and, under a display, the
launcher's checks, the main window, every example project, every
demonstration, Python, relocation, a read-only package and a sanitized environment (`env -i` with only the
display, home and runtime directory).
*Verification:* the validation report in evidence.

### RES-DIST-013 — Clean-machine testing

`tools/release/clean_machine_test.sh` runs the package in containers of named
distributions that have only the declared system packages and a virtual
display, and records the distribution, version, architecture, graphics and
results. What a container cannot show (a real desktop session, GPU drivers)
is stated, not assumed.
*Verification:* `docs/release/RELEASE_REPORT.md` (evidence).

### RES-DIST-014 — Reproducible to the extent practical

The package's `resonant-runtime.json` records the commit, version, build
configuration, compiler, CMake, Qt, Python, packaging tools, bundled
component versions, the build time (`SOURCE_DATE_EPOCH`, the commit's time,
by default) and the architecture. ZIP entries are sorted and carry that
time. Remaining nondeterminism is documented.
*Verification:* `ReleaseTools.test_zip_*`.

### RES-DIST-015 — Release documentation

`docs/release/LINUX_ZIP_DISTRIBUTION.md` (users), `README.txt` in the package
(users of the ZIP), `PACKAGING_AUDIT.md`, `PACKAGING_DECISIONS.md`,
`LINUX_COMPATIBILITY.md`, `LICENSE_AUDIT.md` and `RELEASE_REPORT.md`.
*Verification:* `scripts/doc_integrity.py`.

## Acceptance criteria

| Criterion | Requirement |
|---|---|
| PKG-001, PKG-002, PKG-026 | RES-DIST-001 |
| PKG-003, PKG-004, PKG-005, PKG-006 | RES-DIST-002 |
| PKG-007, PKG-008, PKG-019, PKG-020 | RES-DIST-003 |
| PKG-018 | RES-DIST-004 |
| PKG-009 | RES-DIST-005 |
| PKG-010 | RES-DIST-007 |
| PKG-011, PKG-012 | RES-DIST-009 |
| PKG-013, PKG-014, PKG-015, PKG-017 | RES-DIST-010 |
| PKG-016 | RES-DIST-008 |
| PKG-021, PKG-024, PKG-027, PKG-028 | RES-DIST-012 |
| PKG-022 | RES-DIST-013 |
| PKG-023 | RES-DIST-011 |
| PKG-025 | RES-DIST-015 |

The status of each criterion for a release is in `docs/release/RELEASE_REPORT.md`.
