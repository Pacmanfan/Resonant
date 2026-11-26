# ADR-000204: Technology stack, dual build system (CMake and qmake), vendored GoogleTest, Qt Designer forms

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/product.md` RES-PRODUCT-011, RES-PRODUCT-013, RES-PRODUCT-014
**Request:** REQ-CHAT-20261126-001; operator instructions of 2026-11-26 ("add qmake support in addition to cmake support", "prefer to make all Qt UIs use a .ui file")

## Context

The vision asks for a native desktop engineering application with a 3D
viewport, correct physics and long-term maintainability. The milestone
requests name C++20, Qt 6, CMake, OpenGL 4.5 Core, Eigen and GoogleTest.
The repository's development-control system was bootstrapped with the qmake
build adapter, and the operator asked for qmake support in addition to
CMake, and for Qt user interfaces to be defined in `.ui` files.

Environment audited on 2026-11-26 (Ubuntu 24.04, x86-64): GCC 13.3, CMake
3.28.3, Qt 6.4.2 (qmake6), Mesa 25.2.8 offering OpenGL 4.6 Core, Eigen
3.4.0, nlohmann-json 3.11.3 as distribution packages. GoogleTest is not
installed and the session cannot install packages.

## Decision

1. **Language and toolkit.** C++20; Qt 6 (Widgets, OpenGL, OpenGLWidgets)
   for the desktop application; OpenGL 4.5 Core for rendering.
2. **Two build systems, kept equivalent.** The source tree builds with
   CMake (`CMakeLists.txt`) and with qmake (`project.pro`). Both build the
   same targets from the same sources: the `resonant_core` static library,
   the `resonant_render` static library, the `resonant` application and the
   test binaries. Both collect a target's sources by globbing its
   directory, so there is no source list that could differ between them
   (amended by ADR-000206, which replaced the consistency check first
   planned here).
3. **The development-control system builds through qmake.** `devsys.json`
   keeps `build.system: qmake` (root project `project.pro`, tree
   `build/debug`), which also gives the test orchestrator its affected-test
   selection from the qmake project graph. CMake is verified by the `cmake` test suite in tier
   t2 (`tools/verify_cmake_build.sh`): it builds a separate tree
   (`build/cmake-debug`) and runs CTest.
4. **GoogleTest is vendored.** `third_party/googletest` holds the
   GoogleTest 1.17.0 sources (BSD-3-Clause), compiled by both build
   systems. This makes the tests reproducible without network access or a
   system package, and identical under CMake and qmake. Test binaries write
   a JUnit report where `$PROJECT_TEST_JUNIT` points, which is the
   orchestrator's interface. (This supersedes the bootstrap answer "Qt
   Test": the milestone requests name GoogleTest, and the core library
   must be testable without Qt.)
5. **Dependencies are few and explicit.** Eigen 3.4 (MPL-2.0, header-only,
   system package) for numerics; nlohmann-json 3.11.3 (MIT, single header,
   vendored in `third_party/nlohmann`) for project files; Qt 6 (LGPL-3.0,
   dynamically linked). Each further dependency needs its own decision
   record stating why it is needed and its license.
6. **Qt user interfaces are Qt Designer forms.** Every window, dock panel
   and dialog is laid out in a `.ui` file compiled by `uic` (CMake
   `AUTOUIC`, qmake `FORMS`). C++ code wires signals, fills models and
   builds only what a form cannot express (dynamically generated parameter
   editors, the OpenGL viewport's contents).

## Consequences

* Two build descriptions must be maintained; globbing keeps their source
  sets equal, and the `cmake` suite builds and tests the CMake tree.
* The vendored GoogleTest adds about 1.5 MB of third-party source.
* The completion gate's build is the qmake build; a CMake-only breakage is
  caught by the `cmake` suite of the gate's t2 run.

## Rejected

* CMake only — contradicts the operator's instruction.
* GoogleTest through CMake `FetchContent` — needs the network at configure
  time and gives qmake nothing.
* Qt Test for the core tests — would make Qt a dependency of testing the
  Qt-free core.
