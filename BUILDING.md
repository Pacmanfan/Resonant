# Building and Testing Resonant

Resonant builds with **CMake** and with **qmake**. Both build the same
targets from the same sources (each target globs its directory), so use
whichever fits your workflow. After adding or removing a source file,
re-run the configure step (`cmake` does it by itself; for qmake run
`qmake6` again).

## Requirements

| Dependency | Version used | License | Why |
|---|---|---|---|
| C++20 compiler | GCC 13.3 | — | language standard of the project |
| CMake | 3.21 or later (3.28 used) | BSD-3-Clause | build system |
| qmake | Qt 6 (`qmake6`) | — | alternative build system |
| Qt 6: Core, Gui, Widgets, OpenGL, OpenGLWidgets, Test | 6.4.2 | LGPL-3.0 (dynamically linked) | desktop application and OpenGL integration; Test only for the application tests |
| Eigen | 3.4.0 | MPL-2.0 | vectors, matrices, numerical linear algebra |
| OpenGL | 4.5 Core at run time | — | 3D viewport |
| nlohmann-json | 3.11.3, vendored in `third_party/nlohmann` | MIT | project files |
| GoogleTest | 1.17.0, vendored in `third_party/googletest` | BSD-3-Clause | tests only |
| pkg-config | any | GPL-2.0 (build tool only) | lets qmake find Eigen |
| xvfb (optional) | any | MIT/X11 | runs the application tests without a display |

On Ubuntu 24.04:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    qt6-base-dev libqt6opengl6-dev libeigen3-dev libgl1-mesa-dev xvfb
```

Nothing in the build refers to a path on a particular machine.

## CMake

```bash
cmake -S . -B build/cmake-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake-debug

cmake -S . -B build/cmake-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-release
```

Options: `-DRESONANT_BUILD_APP=OFF` builds only the core library and its
tests (no Qt needed); `-DRESONANT_BUILD_TESTS=OFF` skips the tests.

The application is `build/cmake-debug/src/app/resonant`.

## qmake

```bash
mkdir -p build/debug && cd build/debug
qmake6 ../../project.pro                 # add CONFIG+=release for a Release build
make -j"$(nproc)"
```

The application is `build/debug/src/main/resonant`. The repository's
development-control system uses this build
(`python3 scripts/devsys.py build build`, `... build fresh`).

## Tests

```bash
# CMake tree
ctest --test-dir build/cmake-debug --output-on-failure

# or the binaries directly (either tree)
build/debug/tests/core/resonant_core_tests
xvfb-run -a -s "-screen 0 1600x1000x24" build/debug/tests/app/resonant_app_tests
```

* `resonant_core_tests` — the core library. Needs no display and no OpenGL.
* `resonant_app_tests` — camera mathematics, rendering into an offscreen
  framebuffer, and the real main window. Needs a display with OpenGL 4.5
  Core; `xvfb-run` with Mesa's software renderer is enough. On a desktop
  session it can run without `xvfb-run`, but it opens windows.

Both accept the usual GoogleTest options, for example
`--gtest_filter=Scene.*`.

Through the development-control system (builds with qmake, selects the
suites a change affects, records the run):

```bash
python3 scripts/run_tests.py --tier t2 --build      # everything a work item must pass
python3 scripts/run_tests.py --list                 # suites, groups and tiers
bash tools/verify_cmake_build.sh                    # CMake build + CTest (suite "cmake")
python3 tools/check_layering.py                     # layering rules (suite "layering")
```

## Warnings

Resonant's own code is compiled with `-Wall -Wextra -Wpedantic -Wshadow
-Wnon-virtual-dtor` and builds without warnings. Third-party code is
compiled without them. No warning is suppressed in Resonant's code.

## Known build notes

* Qt 6.4's `moc` cannot parse the C++20 syntax of libstdc++ 13 headers.
  Both builds therefore do not give `moc` the compiler's own include
  directories or predefined macros (`CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF`;
  `MOC_INCLUDEPATH` in `warnings.pri`). This affects only `moc`.
* `resonant --screenshot <file> [project]` saves an image of the main
  window and exits; it is used to check the application on a virtual
  display.
