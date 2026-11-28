# Deployment

**Status:** the deployment process (RES-REL-005, ADR-000230; a process change,
REQ-CHAT-20261128-002, approved by the owner on 2026-11-28). The ZIP itself is described in
`LINUX_ZIP_DISTRIBUTION.md`, its decisions in `PACKAGING_DECISIONS.md` and
ADR-000218, its licences in `LICENSE_AUDIT.md` and `LICENSING_DECISION.md`.

## The one mechanism

Resonant is deployed as one self-contained Linux ZIP. It is built from a clean
Release tree by:

```bash
tools/release/build_linux_zip.sh            # -> dist/RESONANT-Linux-x86_64-<version>.zip and .sha256
```

The build:
1. configures `build/release` with the bundle layout (no tests);
2. builds it;
3. installs into `tmp/release` and bundles the runtime by the policy in
   `tools/release/bundle_policy.json`:
   - **Bundled:** Qt libraries and the plugins the application loads
     (platforms, image formats, XCB integration), the C++ dependencies, the
     Python interpreter and packages.
   - **Left to the system:** the C library, the GPU driver, X11 and Wayland.
4. writes the offline HTML help, the example projects, the demonstration
   profiles and sequences, the reference data, the licences and the manifests;
5. checks the tree and writes the ZIP and its checksum.

Runtime dependencies are discovered from the built ELF files
(`tools/release/inspect_dependencies.sh`), never listed by hand. A library the
policy neither bundles nor leaves to the system fails the build.

## Verification, once per deployment

The `package` lane of `scripts/test_manifest.json` verifies the artifact. It
never builds and never re-runs the development suites (ADR-000186):

```bash
python3 scripts/run_tests.py --tier package
```

| Group | What it checks | Needs |
|---|---|---|
| `validate` | the checksum; the bundle's layout and closure, licences and notices, help, launcher; start-up and every demonstration under a virtual display (`tools/release/validate_bundle.py`) | unzip, sha256sum, xvfb-run |
| `clean-machine` | the ZIP on clean containers (`resonant-clean/*` images, built once): on bare images the launcher names what is missing and never crashes; on desktop images with only the declared system packages it starts, renders, calculates, loads examples, imports data, saves and reopens, and serves the automation endpoint, offline (`tools/release/clean_machine_test.sh`) | docker |

Both groups must pass, and their run record is kept as evidence (`evidence/VER-*`).
A missing tool fails its group; the lane never substitutes a weaker check.

## Before a release is published

1. The `package` lane passed on the exact ZIP, and its checksum is published
   with it.
2. The owner's licensing decision is recorded (`LICENSING_DECISION.md`; the MIT
   License, decided 2026-11-28).
3. The LGPL corresponding sources are published next to the ZIP, one per name
   and version in `THIRD_PARTY_NOTICES.txt`.
4. `CHANGELOG.md` and `RELEASE_REPORT.md` describe the release.

## What is not done instead

- **No hand-copying.** Libraries and plugins are never copied by hand, and
  the bundle is never edited after `package.py` wrote it. Rebuild instead.
- **No uncommitted builds.** A ZIP is never published from a tree with
  uncommitted changes: `resonant-runtime.json` marks such a build `-dirty`.
- **No system-wide installer.** There is no installer that needs root, and no
  environment configuration is required. The launcher sets everything
  relative to itself.
- **No substitute validation.** Development test runs are not a substitute
  for the package lane, and the package lane is not a substitute for the
  development tests.

## Reproducibility

The archive is deterministic for a given build:
- **Entries:** sorted, with one timestamp (`SOURCE_DATE_EPOCH`, by default the
  commit's time) and normalized permissions.
- **Repeat packaging:** packaging the same build twice gave byte-identical
  ZIPs (`RELEASE_REPORT.md`).
- **Provenance:** `resonant-runtime.json` records the commit (`-dirty` for
  uncommitted changes), the toolchain and library versions, and the minimum
  glibc.
- **Remaining nondeterminism:** the compiled programs (no reproducible-build
  flags beyond `-ffile-prefix-map`), and the distribution packages present on
  the build machine (`PACKAGING_DECISIONS.md`).

## Compatibility baseline

| Item | Baseline |
|---|---|
| Architecture | x86-64 Linux |
| C library | glibc 2.38 or newer (built on Ubuntu 24.04) |
| Display | X11, or Wayland with XWayland (native Wayland untested) |
| Graphics | OpenGL 4.5 core through the system's driver (libglvnd) |

The tested distributions are listed in `LINUX_COMPATIBILITY.md`.
