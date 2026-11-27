# WORK-000015: Self-contained Linux ZIP distribution: relocatable bundled runtime, launcher, diagnostics, offline help, licenses and validation

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-16-resonant-self-contai-req-chat-20261126-013.md
**Requirements:** RES-DIST-001, RES-DIST-002, RES-DIST-003, RES-DIST-004, RES-DIST-005, RES-DIST-006, RES-DIST-007, RES-DIST-008, RES-DIST-009, RES-DIST-010, RES-DIST-011, RES-DIST-012, RES-DIST-013, RES-DIST-014, RES-DIST-015
**Decision:** ADR-000218
**Evidence:** VER-000015
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261127T182315-4052689, 20261127T182735-4069645, 20261127T182924-4079637

## Scope

Prompt 16: a self-contained, relocatable x86_64 Linux ZIP made by one command from a Release CMake tree, with its own Qt, libraries and Python, a launcher, diagnostics, the complete offline help, examples and demonstrations, license notices, automated validation of the final ZIP and clean-machine tests (RES-DIST-001..015, acceptance criteria PKG-001..028 in `docs/release/RELEASE_REPORT.md`).

## Implementation

- `src/app/ResourcePaths.{h,cpp}`: one authority for runtime paths: the layout (bundle, installation, build tree), install root, runtime manifest, read-only resources, the bundled Python runtime and site packages, and the user's XDG directories; a bundle (recognised by `resonant-runtime.json`) never looks outside itself.
- `src/app/PythonEnvironment.{h,cpp}`: how every Python process starts (`RESONANT_PYTHON`, else the bundled runtime, else `python3` on `PATH`); the bundled one gets `PYTHONHOME`, a package-only `PYTHONPATH`, no user site, no bytecode writes. `PythonSession.cpp` (console, editor, plugins) and `AutomationServer.cpp` (the gRPC bridge, which ran `python3` directly) use it.
- `src/app/RuntimeDiagnostics.{h,cpp}` and `src/main/main.cpp`: `--check-runtime`, `--diagnostics [--report FILE]` (package, glibc, Qt and where its libraries came from, an OpenGL 4.5 core context on an offscreen surface, Python, help, examples, compute, plugins, user directories, package integrity from `diagnostics/FILES.sha256`, environment by name; home as `~`) and `--check-projects <folder>`.
- `src/app/HelpWindow.{h,cpp}`, `tools/help/build_help.py`: screenshot links are plain image links (a browser shows them; the viewer zooms); the help build writes `index.html` (contents and an offline search over `html/search.js`) and records `draft` and no absolute path in its report.
- CMake: `RESONANT_BUNDLE_LAYOUT` (data at the top of the package, Release only, `-ffile-prefix-map`, no `RESONANT_SOURCE_DIR`), `RESONANT_HELP_DRAFT` (never in a bundle), `resonant-build.json`, the `package_linux_zip` target; install rules use `RESONANT_INSTALL_DATADIR` and skip `__pycache__`. qmake builds are unchanged apart from the new sources.
- `tools/release/`: `build_linux_zip.sh`, `package.py` (stage, Qt plugins by name, Python runtime and packages by RECORD with a requirements check, dependency closure by policy, DT_RPATH with patchelf, precompiled bytecode, licenses, launchers and manifests, tree checks, deterministic ZIP, checksum, validation), `bundle_policy.json`, `third_party.json` (60 components), `validate_bundle.py` (41 checks), `validate_help.py`, `validate_licenses.py`, `clean_machine_test.sh`, `inspect_dependencies.sh`, `generate_checksum.sh`, the launcher (`launcher/resonant`, `resonant.sh`), `README.txt`, `LICENSE.txt`, `licenses/SoapySDR-BSL-1.0.txt`.

## Tests

- `tests/app/test_runtime_paths.cpp` (9): bundle found from its executable wherever it is (spaces, renamed, moved); a bundle never looks outside itself; installation layout; user directories never inside the bundle; the bundled Python is isolated; an explicit interpreter wins and keeps the user's path; diagnostics redact the home folder and environment values; bundle integrity finds changed and missing files; a screenshot link in the help opens it full size.
- `python/tests/test_release_tools.py` (13, run by `PythonApi.ThePackageTestsPassAgainstThisBuildsModule`): system-library classification, reasons and packages; relative search paths; deterministic, sorted ZIP with permissions (extracted with unzip); tree checks (links, development files, machine paths); Python requirements; the license table; the launcher with a stand-in program (argument and exit-status pass-through, `--software-gl`, links, `resonant.sh`, dash; no display, too old glibc, incomplete package; missing system libraries named by family); the built help validates, its browser search ranks topics (Node.js), remote resources and broken links are found.
- The final ZIP: `tools/release/validate_bundle.py` 41 of 41 checks (all 43 projects and 8 demonstrations from the package, sanitized environment, relocation, read-only, nothing written); `tools/release/clean_machine_test.sh`: Ubuntu 24.04, Fedora 41, Fedora 42 bare (refused with the packages named) and desktop images offline (all checks, projects and demonstrations), Ubuntu 22.04 and Debian 12 refused with the glibc versions; `--check-runtime` with Intel Mesa (4.6 core), NVIDIA 595 (4.5 core), llvmpipe (4.5 core).

## Regression impact and documentation

- Development and installed builds keep their layouts: `share/resonant` installations and build trees are found as before (`HelpUiTest.TheHelpWorksFromAnInstalledLayout`, the whole app and core suites); a system `python3` keeps the user's `PYTHONPATH` after the package.
- Help pages now link screenshots as images instead of `zoom:` URLs; the viewer still accepts `zoom:`.
- Documents: `docs/requirements/distribution.md`, ADR-000218, `docs/release/` (PACKAGING_AUDIT, PACKAGING_DECISIONS, LINUX_COMPATIBILITY, LICENSE_AUDIT, LINUX_ZIP_DISTRIBUTION, RELEASE_REPORT, CHANGELOG), the help page `gs.linux-zip` and `gs.installation`, `docs/help/DOCUMENTATION_COVERAGE.md`, `README.md`, `BUILDING.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `docs/README.md`, `docs/architecture/roadmap.md` (M11).
- Owner decisions recorded in the release report: choosing Resonant's own license (the application part of PKG-023) and publishing the LGPL sources with a public release. The framework's `package` lane and `AGENTS.md` `Deployment` section are protected placeholders (a process change).

## Traceability

REQ-CHAT-20261126-013 -> RES-DIST-001, RES-DIST-002, RES-DIST-003, RES-DIST-004, RES-DIST-005, RES-DIST-006, RES-DIST-007, RES-DIST-008, RES-DIST-009, RES-DIST-010, RES-DIST-011, RES-DIST-012, RES-DIST-013, RES-DIST-014, RES-DIST-015 -> WORK-000015 -> evidence (pending)

## Transitions

- 2026-11-27T22:26:35.144586+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T23:32:13.953362+00:00 active -> completed by agent-dev-20261126151713-76f632
