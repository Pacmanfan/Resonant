---
id: trouble.installation
title: Installation problems
category: Troubleshooting
order: 10
status: available
summary: What to check when Resonant does not build, cannot be found after building, or cannot find its help and examples.
keywords: [install, build, cmake, qmake, Qt, Eigen, Linux]
related: [gs.installation, trouble.build-runtime, trouble.dependencies, py.installation]
---

Resonant has no binary installer package in this version. You install it by
building it from its source with **CMake** or **qmake**, then run it from the
build tree or from an installation prefix made with `make install` (qmake) or
`cmake --install`.
Linux (Ubuntu 24.04, GCC 13.3, Qt 6.4.2) is the platform it is built and
tested on. Nothing platform-specific is used on purpose, but Windows and
macOS builds have not been tried.

## What a working build needs

| Requirement | Version | Without it |
|---|---|---|
| C++20 compiler | GCC 13.3 used | the build fails at the first C++20 feature |
| CMake | 3.21 or later | CMake stops at `cmake_minimum_required` |
| Qt 6: Core, Gui, Widgets, OpenGL, OpenGLWidgets | 6.4 or later | CMake stops at `find_package(Qt6 6.4 ...)`; only the core library can be built |
| Eigen | 3.4 | CMake stops at `find_package(Eigen3 3.4 ...)`; qmake finds it through pkg-config |
| Python 3 interpreter | any 3.x | CMake stops: the in-app help is generated with it |
| OpenGL | 4.5 Core at run time | the application starts, the 3D viewport stays empty |

On Ubuntu 24.04 the required packages are:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    qt6-base-dev libqt6opengl6-dev libeigen3-dev libgl1-mesa-dev xvfb
```

nlohmann-json and GoogleTest are part of the source (`third_party/`); you
never install them. Optional libraries (VOLK, CUDA, HDF5, ALSA, SoapySDR,
pybind11) are described in [Missing dependencies](dependencies.md).

## The build stops

1. **Read the first error, not the last.** CMake and the compiler report
   follow-up errors after the real one.
2. **A package was not found.** Install the package named in the table above
   and run the configure step again in a clean tree, for example
   `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release`.
3. **qmake cannot find Eigen.** qmake asks pkg-config for `eigen3`: install
   `pkg-config` and `libeigen3-dev`.
4. **Only the core library is needed** (a server without Qt): configure with
   `-DRESONANT_BUILD_APP=OFF`. No application is built then.
5. **Errors in `moc` about C++20 syntax.** Qt 6.4's `moc` cannot parse the
   C++20 parts of the libstdc++ 13 headers. Both builds already keep the
   compiler's own include directories and macros away from `moc`; if you
   changed the build files, restore that setting.

More build errors (missing new files, the qmake `-r` flag, CUDA) are on
[Build and runtime problems](build-runtime.md#the-build-fails-after-adding-or-removing-a-file).

## I cannot find the program

| Build | The program |
|---|---|
| CMake, tree `build/release` | `build/release/src/app/resonant` |
| CMake, tree `build/cmake-debug` | `build/cmake-debug/src/app/resonant` |
| qmake, tree `build/debug` | `build/debug/src/main/resonant` |
| `cmake --install` to a prefix | `<prefix>/bin/resonant`, shared data in `<prefix>/share/resonant` |

Run `resonant --version` to see which version you have and
`resonant --help` for the command-line options. A project file can be
given as the last argument:
`resonant examples/mechanics/b-vibrating-plate.resonant`.

## Help, examples or demonstrations are not found

The application looks for its shared data (the built help, the example
projects, the demonstrations, the example plugins, the Python package) in
this order:

1. the folders named by the environment variables `RESONANT_HELP_DIR`,
   `RESONANT_RESOURCE_ROOT`, `RESONANT_PYTHON_PACKAGE_DIR` and
   `RESONANT_PYTHON_MODULE_DIR`;
2. an installation: `share/resonant` next to the `bin` folder of the program;
3. a build tree: the folders above the program that hold the generated help,
   and the source tree the build was configured from.

If you copied only the program file somewhere else, none of these are
found. Run it from its build tree, install it with `cmake --install`, or
point the environment variables at the folders.

## Common mistakes

- Building in the source folder instead of a separate build tree. Use
  `build/<name>` and keep one tree per configuration (Debug, Release).
- Expecting a Debug build to be fast: it is about ten times slower than a
  Release build in the full-wave solvers.
- Installing Qt 5 packages. Resonant needs Qt 6 (`qt6-base-dev`, `qmake6`).

## Related topics

- [Installation and first launch](../getting-started/installation.md)
- [Build and runtime problems](build-runtime.md)
- [Missing dependencies](dependencies.md)
- [Python installation and configuration](../python/installation.md)
