# Request: # PROMPT 16 — RESONANT Self-Contained Linux ZIP Distribution
## Complete Runtime

**Request ID:** REQ-CHAT-20261126-013
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T21:47:07-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 16 — RESONANT Self-Contained Linux ZIP Distribution
## Complete Runtime Packaging, Dependency Bundling, Offline Help, Resources, Launchers, and Clean-System Release Validation

You are working on **RESONANT — Wave Field Engineering Studio**, a scientific and engineering application for modeling, simulating, visualizing, optimizing, and manipulating acoustic, mechanical, and electromagnetic wave phenomena.

Your task is to audit, implement, and validate a complete **self-contained Linux ZIP distribution**.

This is a release-engineering milestone. The objective is to produce a downloadable ZIP archive that contains everything necessary to run RESONANT on a supported Linux system, without requiring the user to install Qt, application libraries, Python packages, or other development dependencies.

The user should be able to:

1. Download the ZIP.
2. Extract it into a directory of their choice.
3. Run the included launcher.
4. Use the application with its bundled resources, offline documentation, examples, and demo projects.

No conventional installation process should be necessary.

Do not merely create a ZIP of the existing build directory. Implement a proper, reproducible, relocatable application distribution.

---

# 1. Primary deliverable

Produce a ZIP archive with a structure conceptually similar to:

```text
RESONANT-Linux-x86_64-VERSION/
├── resonant
├── resonant.sh
├── README.txt
├── LICENSE
├── THIRD_PARTY_NOTICES.txt
├── VERSION
├── CHANGELOG.txt
│
├── bin/
│   └── resonant
│
├── lib/
│   ├── application shared libraries
│   ├── bundled third-party shared libraries
│   └── Qt shared libraries
│
├── plugins/
│   └── Qt platform, image, and other required plugins
│
├── resources/
│   ├── icons/
│   ├── fonts/
│   ├── shaders/
│   ├── themes/
│   ├── application data/
│   └── other runtime assets/
│
├── help/
│   ├── index.html
│   ├── pages/
│   ├── css/
│   ├── js/
│   ├── images/
│   └── search/
│
├── examples/
│   ├── projects/
│   ├── geometry/
│   ├── materials/
│   └── simulation/
│
├── demos/
│   ├── profiles/
│   ├── projects/
│   ├── sequences/
│   ├── screenshots/
│   └── manifests/
│
├── python/
│   └── bundled Python components, if required
│
├── config/
│   └── default application configuration
│
├── diagnostics/
│   └── packaging and runtime diagnostic utilities
│
└── licenses/
    ├── application/
    └── third_party/
```

This is a proposed layout, not an instruction to create empty directories. Include only components that actually exist or are required.

The final ZIP must contain the complete runnable distribution, not just the executable and a collection of development artifacts.

The directory must remain relocatable. The user must be able to extract it to different locations without editing configuration files.

---

# 2. Begin with a comprehensive packaging audit

Before changing anything, inspect the actual repository and existing build system.

Identify:

- Project root and current source layout.
- CMake configuration and build targets.
- Release and Debug build configurations.
- Executable names and locations.
- All shared libraries built by the project.
- Static libraries and dynamically loaded modules.
- Qt version and components in use.
- Qt platform plugins.
- Qt image format plugins.
- Qt SQL, network, multimedia, or other plugins if used.
- OpenGL requirements.
- Third-party dependencies.
- Python runtime and extension modules, if required.
- Pybind11 integration.
- Python scripts and runtime packages.
- Solver plugins and dynamically loaded solver libraries.
- Geometry and CAD libraries.
- HDF5 and related dependencies.
- FFTW, Eigen, OpenMP, and other numerical dependencies.
- Gmsh and OpenCASCADE dependencies, if used.
- External executables invoked by the application.
- Shader files and GPU-related assets.
- Fonts, icons, themes, and images.
- HTML help system and its complete dependency tree.
- Example projects and demo assets.
- Configuration files.
- Files currently loaded through hardcoded development paths.
- Runtime dependencies discovered only when a particular feature is activated.

Create:

`docs/release/PACKAGING_AUDIT.md`

The audit must contain:

1. Current build configuration.
2. Executable and shared-library inventory.
3. Runtime dependency inventory.
4. Qt component and plugin inventory.
5. Dynamic loading mechanisms.
6. Runtime resource inventory.
7. Python dependency inventory.
8. Help and demo asset inventory.
9. External system dependencies.
10. Licensing and redistribution concerns.
11. Known packaging risks.
12. Recommended packaging architecture.

Do not assume that an executable's `ldd` output captures every dependency. Inspect dynamically loaded libraries, Qt plugins, optional modules, external executables, and feature-specific runtime paths.

Use tools such as:

- `ldd`
- `readelf`
- `objdump`
- `patchelf`
- `lddtree` where available
- `qtpaths` or `qmake` for Qt installation information
- CMake dependency inspection
- Runtime loader diagnostics
- Qt plugin diagnostics
- `strace` where useful

Document the actual dependency graph.

---

# 3. Select and implement the packaging toolchain

Evaluate current Linux packaging tools and select the most appropriate solution.

Candidates include:

- `linuxdeploy`
- `linuxdeploy-plugin-qt`
- `linuxdeployqt`
- `appimagetool` and related tooling where useful for dependency collection or validation
- CMake install and runtime dependency collection mechanisms
- `patchelf`
- Custom packaging scripts where required

Do not select a tool simply because it is familiar.

Evaluate:

- Maintenance status.
- Compatibility with the project's Qt version.
- Compatibility with the target Linux distribution baseline.
- Ability to collect Qt plugins.
- Ability to bundle non-Qt shared libraries.
- RPATH and relocation handling.
- Python extension compatibility.
- OpenGL requirements.
- Reproducibility.
- Licensing.
- Suitability for producing an ordinary ZIP distribution.

**Important:** The deliverable is a ZIP archive, not an AppImage.

AppImage tooling may be used as a dependency-collection or diagnostic component if appropriate, but do not substitute an AppImage for the requested ZIP.

Record the selected toolchain and reasoning in:

`docs/release/PACKAGING_DECISIONS.md`

The packaging process must be automated and repeatable.

---

# 4. Build a genuine relocatable runtime

The packaged application must not depend on the development environment.

Implement a proper runtime directory structure using CMake install rules and packaging scripts.

Requirements:

- Release executable.
- Required application shared libraries.
- Required third-party shared libraries.
- Required Qt runtime libraries.
- Required Qt plugins.
- Runtime resources.
- Solver and extension modules.
- Required Python runtime components.
- Offline help and demo resources.

Use relative runtime paths wherever possible.

For example, configure ELF RPATH/RUNPATH so that the executable can locate its bundled libraries relative to its own location.

The exact layout may differ from the example above, but the result must be relocatable.

Do not rely on:

- The developer's `LD_LIBRARY_PATH`.
- The developer's `PATH`.
- A globally installed Qt SDK.
- The original build directory.
- Absolute paths into the source repository.
- Environment variables configured specifically on the development machine.
- Symlinks pointing outside the distribution directory.

Inspect the final ELF files and verify their runtime paths.

Ensure that bundled libraries do not accidentally override essential system libraries in unsafe ways.

---

# 5. Qt runtime and plugin packaging

Qt packaging must be complete.

Determine the exact Qt components used by the application and include all required runtime components.

At minimum, investigate:

- Qt Core.
- Qt GUI.
- Qt Widgets.
- Qt Network.
- Qt OpenGL.
- Qt OpenGL Widgets.
- Qt Concurrent.
- Qt XML.
- Qt Print Support.
- Other components actually used by the application.

Include required plugins, such as:

- `platforms/libqxcb.so`
- `platforms/libqwayland*.so`, if supported and required.
- Image format plugins.
- Icon engine plugins.
- TLS plugins, if required.
- SQL drivers, if required.
- Other plugins discovered through runtime auditing.

Do not copy every Qt plugin indiscriminately.

Include only appropriate plugins and their dependencies.

Ensure Qt plugin discovery works from the extracted distribution.

Use a packaged Qt configuration file or appropriate launcher configuration where necessary.

The application must work without a globally installed Qt development environment.

Test the actual platform plugin and its shared-library dependencies.

---

# 6. OpenGL and graphics compatibility

RESONANT uses an OpenGL-based scientific visualization viewport.

The package must include the application-side graphics components but must not blindly bundle proprietary GPU drivers or replace system graphics interfaces.

Investigate:

- OpenGL runtime requirements.
- Minimum OpenGL version.
- Required OpenGL extensions.
- Mesa compatibility.
- NVIDIA proprietary driver compatibility.
- AMD driver compatibility.
- Intel graphics compatibility.
- X11 and Wayland behavior.
- Qt platform plugin requirements.

Do not bundle kernel-specific graphics drivers.

Do not package a random `libGL.so` or graphics-driver stack without understanding the consequences.

The launcher must perform a basic graphics environment check and provide a useful diagnostic if the system cannot satisfy the application's OpenGL requirements.

If software rendering is supported, make it an explicit fallback option rather than silently degrading performance.

Document:

- Minimum supported OpenGL version.
- Recommended graphics hardware.
- Known driver limitations.
- Any unavoidable system-level requirements.

Test the application on more than one graphics configuration where available.

---

# 7. Python runtime packaging

Audit the actual Python integration.

Determine whether Python is:

- Required for application startup.
- Required only for scripting.
- Required only for optional plugins.
- Used in-process through pybind11.
- Used through a separate worker process.

The distribution must behave correctly in the absence of a system Python installation if Python functionality is intended to be bundled.

If Python is required:

- Include a compatible runtime.
- Include required standard-library components.
- Include required extension modules.
- Include required Python packages.
- Include the `resonant` Python package.
- Include scripts and default Python resources.
- Ensure Python module discovery works from the extracted directory.
- Ensure native extension modules locate their bundled shared libraries.
- Ensure Python cannot accidentally import incompatible modules from the developer's environment.

If Python is optional, the core application must still start and clearly report that optional scripting functionality is unavailable when the runtime is absent.

Do not silently depend on whichever Python installation happens to exist on the user's machine.

Document the chosen strategy.

---

# 8. Offline HTML help system

The entire offline help system must be included.

This is not optional.

Audit the application's help implementation and collect every required file.

Include:

- Main help entry point.
- HTML pages.
- Navigation.
- Search index.
- CSS.
- JavaScript.
- Fonts.
- Images.
- Diagrams.
- Screenshots.
- Context-sensitive help references.
- Any generated documentation assets.

The help system must work without an internet connection.

Verify:

- Help opens from the packaged application.
- Navigation works.
- Search works.
- Internal links work.
- Images load.
- CSS and JavaScript load.
- Context-sensitive help works.
- No links accidentally point into the developer's source tree.
- No mandatory remote resources are required.

The application must locate help relative to its packaged installation, not a hardcoded development path.

Include a standalone `help/index.html` entry point that users can also open in a browser.

If help generation is part of the build process, integrate it into the release packaging pipeline.

Do not package stale generated documentation.

Add an automated help validation step that checks internal links and required assets.

---

# 9. Resources, examples, and demo content

Include all resources required to make the distributed application useful immediately.

Audit and package:

- Application icons.
- Fonts.
- Themes.
- OpenGL shaders.
- Material libraries.
- Geometry libraries.
- Solver data.
- Default configuration.
- Example projects.
- Example simulation files.
- Example imported geometry.
- Demo projects.
- Demo profiles.
- Demo sequences.
- Demo screenshots.
- Demo manifests.
- Any required lookup tables or reference data.

Do not include developer-only assets, temporary files, build caches, local datasets, private test data, or large unrelated artifacts.

The package should include a small but meaningful collection of verified example projects.

Each included project must actually open in the packaged application.

Run the demos from the packaged application, not from the development build.

If a demo references external data, either include that data or correct the demo so that it is self-contained.

Do not ship screenshots or demo results that misrepresent the current application.

---

# 10. Resource discovery and application path handling

This is a critical part of the work.

Audit all resource-loading code for absolute paths and development-machine assumptions.

Create a unified resource discovery mechanism.

The application must distinguish:

1. Executable location.
2. Application installation/resource root.
3. Bundled read-only resources.
4. User configuration directory.
5. User data directory.
6. User cache directory.
7. User-generated projects and exports.

Use appropriate Linux conventions for user-writable data, such as:

- `~/.config/RESONANT/`
- `~/.local/share/RESONANT/`
- `~/.cache/RESONANT/`

The exact naming can follow existing project conventions.

Bundled resources must remain read-only from the application's perspective.

Never write user preferences, projects, caches, logs, or temporary data into the extracted application directory unless the user explicitly requests it.

The application must work when extracted to:

- A directory in the user's home folder.
- A directory on a secondary drive.
- A directory with spaces in its path.
- A removable drive, subject to filesystem and runtime limitations.

Ensure all resource discovery is consistent across:

- Main application.
- Help system.
- Python.
- Solver modules.
- Plugins.
- Demo system.
- File import/export.
- Shader loading.

Add tests for relocatable resource discovery.

---

# 11. Launcher and user experience

Provide a top-level launcher:

`RESONANT-Linux-x86_64-VERSION/resonant`

This should be the simplest way to start the application.

Also provide:

`resonant.sh`

The launcher must:

1. Determine its own location.
2. Resolve the application directory.
3. Configure required bundled-library paths using safe relative paths.
4. Configure Qt plugin discovery.
5. Configure resource paths.
6. Configure bundled Python paths if applicable.
7. Perform basic system compatibility checks.
8. Launch the application.
9. Preserve the application's exit code.

The launcher must not require the user to edit environment variables.

Do not rely on the current working directory.

The user should be able to launch the application from a terminal, file manager, or another working directory.

Support command-line argument forwarding.

Support a useful diagnostic mode, for example:

```bash
./resonant --diagnostics
```

This should report:

- Application version.
- Build identifier.
- Linux architecture.
- Qt runtime version.
- Bundled library locations.
- OpenGL environment.
- Python availability.
- Help resource availability.
- Solver/plugin discovery.
- Important runtime dependency status.

Also provide:

```bash
./resonant --check-runtime
```

This should perform compatibility checks without starting the full GUI where possible.

Provide a useful error message if required dependencies are missing.

Do not display a misleading success message if startup validation fails.

---

# 12. System compatibility and distribution baseline

A self-contained ZIP cannot eliminate all Linux ABI and kernel dependencies.

Define a deliberate compatibility baseline.

Investigate:

- Target CPU architecture.
- Minimum glibc version.
- Required system libraries.
- X11 dependencies.
- Wayland dependencies.
- OpenGL dependencies.
- Audio or device dependencies if actually required.
- Kernel interfaces.
- Filesystem assumptions.

For the initial release, prioritize x86_64 Linux unless the existing project explicitly supports another architecture.

Do not build against an unnecessarily recent distribution if doing so prevents the package from running on older supported systems.

Prefer building on the oldest practical supported baseline.

Do not bundle glibc or other fundamental system runtime components as a casual workaround.

Produce a compatibility document:

`docs/release/LINUX_COMPATIBILITY.md`

It must clearly distinguish:

- Bundled dependencies.
- Required system dependencies.
- Optional system integrations.
- Unsupported environments.
- Recommended environments.

The README must communicate this accurately.

---

# 13. Dependency licensing and redistribution

Audit the licenses of all bundled components.

Include:

- Qt license information.
- Third-party library licenses.
- Python and package licenses, if bundled.
- Numerical library licenses.
- CAD/geometry library licenses.
- Solver licenses.
- Plugin licenses.
- Font licenses.
- Image and documentation asset licenses.

Pay particular attention to LGPL/GPL obligations and any commercial or restricted redistribution conditions.

Do not include a dependency if its redistribution rights are unclear.

Create:

- `THIRD_PARTY_NOTICES.txt`
- `licenses/`
- `docs/release/LICENSE_AUDIT.md`

Record:

- Component name.
- Version.
- License.
- Source.
- Whether it is statically or dynamically linked.
- Whether it is bundled.
- Required attribution or redistribution notices.

Do not assume that because a library is installed on the development machine it can be redistributed.

---

# 14. CMake and release build integration

Integrate packaging into the existing CMake build system.

Provide a clear release workflow, such as:

```bash
cmake -S . -B build-release \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-release --parallel

cmake --install build-release \
    --prefix /some/staging/location
```

The actual commands should match the implemented project configuration.

Add dedicated packaging scripts or CMake targets.

For example:

```bash
cmake --build build-release --target package_linux_zip
```

The final command should produce a ZIP archive in a predictable release directory.

Suggested output:

```text
dist/
    RESONANT-Linux-x86_64-VERSION.zip
    RESONANT-Linux-x86_64-VERSION.sha256
```

The build process must:

1. Build the Release application.
2. Install into a clean staging directory.
3. Collect required shared libraries.
4. Collect Qt runtime and plugins.
5. Collect Python runtime components if needed.
6. Collect resources.
7. Collect help and demo content.
8. Configure relative runtime paths.
9. Generate version information.
10. Generate notices and license files.
11. Run package validation.
12. Create the ZIP.
13. Generate a SHA-256 checksum.

Do not package Debug libraries or development-only files.

Do not include source code, object files, CMake caches, test binaries, temporary files, or compiler artifacts unless explicitly needed by the runtime.

Make the process repeatable.

---

# 15. ZIP archive quality

The ZIP itself is part of the product.

Requirements:

- Preserve executable permission bits.
- Preserve directory structure.
- Avoid absolute paths.
- Avoid unnecessary symlinks.
- Avoid symlinks that escape the package.
- Avoid duplicate library versions.
- Avoid redundant build artifacts.
- Include a top-level directory.
- Use a predictable archive filename.
- Include version and architecture in the filename.
- Generate a SHA-256 checksum.
- Ensure the archive can be extracted by common Linux archive utilities.

Test extraction using at least one standard ZIP utility.

Verify that the extracted executable remains executable.

Verify that the application runs from the extracted location.

Test relocation by extracting the same ZIP into a different directory and launching it again.

The package must not contain references to the original staging directory.

---

# 16. Automated packaging validation

A packaging pipeline is not complete until it validates the actual final ZIP.

Create automated validation scripts.

Suggested locations:

```text
scripts/release/
    build_linux_zip.sh
    validate_linux_bundle.sh
    validate_help.sh
    validate_licenses.sh
    inspect_dependencies.sh
    generate_checksum.sh
```

Adapt names to the existing project conventions.

The validation must include:

### Executable and dependency checks

- Executable exists.
- Executable is executable.
- ELF architecture is correct.
- All required shared libraries resolve.
- No unexpected dependencies resolve from the development environment.
- RPATH/RUNPATH is correct.
- Qt plugins resolve.
- No development-directory paths remain.
- Required resources exist.

### Runtime checks

- Launcher executes.
- Application starts.
- Qt platform plugin loads.
- Main window opens.
- OpenGL viewport initializes.
- Core UI resources load.
- Solver discovery works.
- Material libraries load.
- Python integration behaves as documented.
- Help opens.
- Example projects open.
- Demo system works.

### Relocation checks

- Extract to a temporary directory.
- Run from a different current working directory.
- Extract to a path containing spaces.
- Rename the extracted directory.
- Run again.

### Clean environment checks

Test with a sanitized environment that removes developer-specific variables such as:

- `LD_LIBRARY_PATH`
- `LD_PRELOAD`
- `PYTHONPATH`
- `PYTHONHOME`
- Qt development environment variables.

Do not remove environment variables that are genuinely required by the operating system or display session.

The goal is to prove that the bundle does not accidentally depend on the development environment.

### Help checks

- Main index exists.
- All required pages exist.
- Internal links resolve.
- Images load.
- Search index exists.
- No required internet connection.

### Package integrity checks

- ZIP extracts successfully.
- Checksum matches.
- No missing required files.
- No unexpected build artifacts.
- No broken external symlinks.
- No accidental private or local data.

---

# 17. Clean-machine testing

This is one of the most important acceptance requirements.

A package that runs on the developer's machine is not sufficient evidence.

Test the final ZIP in a clean Linux environment.

Preferred approaches:

- A clean virtual machine.
- A fresh Linux installation.
- A container where appropriate for non-GUI dependency checks.
- A separate machine with no Qt development environment.

Test the package without installing the application's development dependencies.

At minimum, test:

1. Extraction.
2. Launch.
3. Main window.
4. OpenGL viewport.
5. Help.
6. Example project loading.
7. Basic simulation workflow.
8. Python functionality if bundled.
9. Exit and restart.
10. Relocation.

Containers may be useful for dependency checks, but they do not fully substitute for testing a real desktop graphics session.

Record the actual Linux distribution, version, architecture, graphics environment, and results.

If a clean-machine test cannot be performed, report that limitation explicitly. Do not claim it passed.

---

# 18. Runtime diagnostics and failure reporting

The application should fail informatively.

Examples:

- Missing Qt platform plugin.
- Unsupported OpenGL version.
- Missing shared library.
- Incompatible Python runtime.
- Missing solver module.
- Missing resource directory.
- Corrupted or incomplete package.
- Insufficient memory or storage where detectable.

Provide actionable error messages.

Where possible, offer a diagnostic report that the user can save.

Do not include credentials, private project contents, or sensitive user data in diagnostic reports.

Do not implement telemetry or automatic uploads.

The user must control whether diagnostic information is shared.

---

# 19. Release documentation

Create:

`docs/release/LINUX_ZIP_DISTRIBUTION.md`

This must explain:

- What the ZIP contains.
- Supported Linux architecture.
- Minimum compatibility baseline.
- How to download and verify the archive.
- How to extract it.
- How to launch RESONANT.
- How to launch from a terminal.
- How to run diagnostics.
- How to access offline help.
- Where user configuration is stored.
- Where user data is stored.
- How to update to a newer version.
- How to remove the application.
- Known limitations.
- Graphics requirements.
- Troubleshooting.

Also provide a concise root-level `README.txt` specifically for ZIP users.

A new user should not need to read developer documentation to run the application.

---

# 20. Release build reproducibility

The package should be reproducible to the extent practical.

Record:

- Git commit.
- Application version.
- Build configuration.
- Compiler version.
- CMake version.
- Qt version.
- Packaging tool versions.
- Dependency versions.
- Build timestamp.
- Target architecture.

Use a consistent staging directory and clean build process.

Avoid including machine-specific timestamps or absolute build paths in runtime resources.

Do not sacrifice runtime reliability for byte-identical ZIP output, but document any unavoidable nondeterminism.

---

# 21. Implementation phases

Execute this work in the following order.

## Phase A — Audit and decisions

- Inspect the current repository.
- Inventory runtime dependencies.
- Audit resources and help.
- Audit dynamic loading.
- Audit licensing.
- Select the packaging architecture.
- Document decisions.

Do not begin by blindly copying shared libraries.

## Phase B — Runtime path consolidation

- Fix hardcoded development paths.
- Implement unified resource discovery.
- Separate bundled resources from user-writable data.
- Fix Python and plugin discovery.
- Add path-related tests.

## Phase C — CMake installation and staging

- Add proper installation rules.
- Create a clean staging tree.
- Install executable and required libraries.
- Install resources, help, demos, and examples.

## Phase D — Dependency bundling

- Collect Qt runtime.
- Collect required plugins.
- Collect third-party libraries.
- Configure relative RPATH/RUNPATH.
- Include Python runtime if required.
- Verify dynamic loading.

## Phase E — Launcher and diagnostics

- Implement launcher.
- Add runtime compatibility checks.
- Add diagnostic mode.
- Add actionable failure reporting.

## Phase F — ZIP creation

- Create release directory.
- Include notices and documentation.
- Generate checksum.
- Preserve permissions.
- Create final ZIP.

## Phase G — Validation

- Inspect ELF dependencies.
- Validate help.
- Validate resources.
- Run automated checks.
- Test relocation.
- Test sanitized environment.
- Test clean Linux environment.
- Exercise real application workflows.

## Phase H — Release handoff

Produce final documentation and a concise release report.

---

# 22. Required acceptance criteria

The milestone is complete only when all applicable criteria below are satisfied.

| ID | Acceptance criterion |
|---|---|
| PKG-001 | A versioned x86_64 Linux ZIP is generated |
| PKG-002 | ZIP contains the runnable application executable |
| PKG-003 | All required application shared libraries are bundled |
| PKG-004 | Required Qt runtime libraries are bundled |
| PKG-005 | Required Qt plugins are bundled |
| PKG-006 | Third-party runtime dependencies are resolved |
| PKG-007 | Runtime paths are relocatable |
| PKG-008 | No development-directory paths are required |
| PKG-009 | Launcher works without manual environment configuration |
| PKG-010 | OpenGL viewport starts on a supported system |
| PKG-011 | Offline HTML help is fully included and functional |
| PKG-012 | Help navigation and search work offline |
| PKG-013 | Required shaders, fonts, icons, and resources are included |
| PKG-014 | Example projects open |
| PKG-015 | Demo projects and sequences work |
| PKG-016 | Python functionality works as documented |
| PKG-017 | Solver and plugin discovery works |
| PKG-018 | User data is stored outside the application bundle |
| PKG-019 | ZIP can be extracted and run from a different directory |
| PKG-020 | ZIP can be run from a path containing spaces |
| PKG-021 | Sanitized environment test passes |
| PKG-022 | Clean Linux environment test passes |
| PKG-023 | Licensing and redistribution notices are included |
| PKG-024 | SHA-256 checksum is generated and verified |
| PKG-025 | Release documentation is complete |
| PKG-026 | Packaging pipeline is automated |
| PKG-027 | No private development data is included |
| PKG-028 | Final package passes automated validation |

Mark each criterion as PASS, FAIL, BLOCKED, or NOT APPLICABLE.

Do not mark an untested item as PASS.

---

# 23. Important engineering constraints

1. Do not replace the application architecture to solve a packaging problem.
2. Do not remove functionality simply because it is inconvenient to package.
3. Do not silently disable Python, solvers, help, or demos to make the ZIP smaller.
4. Do not assume that `ldd` alone proves dependency completeness.
5. Do not bundle incompatible graphics drivers.
6. Do not bundle glibc as an improvised compatibility solution.
7. Do not rely on absolute development paths.
8. Do not package Debug builds.
9. Do not include local test datasets or private files.
10. Do not claim clean-system compatibility without testing.
11. Do not create a ZIP that merely works on the developer's computer.
12. Preserve the existing application architecture and functionality.
13. Keep the packaging pipeline maintainable and suitable for future releases.
14. Prefer CMake installation rules and standard packaging tools over a collection of fragile manual copy commands.

If a dependency cannot legally or technically be bundled, document the exact reason and provide a clear system prerequisite.

If a packaging issue exposes an architectural problem, fix the underlying problem rather than adding a fragile workaround.

---

# 24. Final deliverables

At completion, provide:

1. Working CMake release packaging integration.
2. Automated Linux ZIP packaging script.
3. Runtime launcher.
4. Runtime diagnostic utility or mode.
5. Complete relocatable distribution directory.
6. Versioned ZIP archive.
7. SHA-256 checksum.
8. Offline HTML help included in the ZIP.
9. Example and demo content included in the ZIP.
10. Third-party notices and license directory.
11. `PACKAGING_AUDIT.md`.
12. `PACKAGING_DECISIONS.md`.
13. `LINUX_COMPATIBILITY.md`.
14. `LICENSE_AUDIT.md`.
15. `LINUX_ZIP_DISTRIBUTION.md`.
16. Automated packaging validation scripts.
17. Test results and clean-environment validation report.
18. Final list of known limitations and unresolved issues.

The final report must include:

- Exact ZIP output path.
- ZIP file size.
- SHA-256 checksum.
- Application version and Git commit.
- Linux build environment.
- Packaging toolchain.
- Bundled dependency summary.
- Required system dependency summary.
- Validation results.
- Clean-machine test results.
- Any blocked acceptance criteria.

**The final objective is simple: a user should be able to download one ZIP, extract it, launch RESONANT, open the offline help, load an example project, and begin using the application without setting up a development environment.**

Treat that experience as the definition of a successful release package.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: PERFORMANCE (confidence high)
  - validation: PASS
  - classifier_reason: override FEATURE accepted (text alone suggests PERFORMANCE: performance/resource problem in the request statement: optimizing)
  - evidence: 'implement' [body]; 'milestone' [body]; 'objective' [body]; 'should be able to' [body]; 'create' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this as a ticket and process in order' (queued after Prompt 15, REQ-CHAT-20261126-012)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this as a ticket and process in order' (queued after Prompt 15, REQ-CHAT-20261126-012)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000015
- Evidence: pending
