---
id: gs.installation
title: Installation and first launch
category: Getting Started
order: 20
status: available
summary: Build Resonant from source with CMake or qmake, choose the optional components (CUDA, VOLK, HDF5, Python module), and start it for the first time.
keywords: [install, build, cmake, qmake, dependencies, cuda, first launch, command line]
related: [trouble.installation, trouble.dependencies, trouble.build-runtime, py.installation, gs.new-project]
---

Resonant is built from its source tree; `make install` (qmake) or `cmake --install`
puts the application, its help, demonstrations, examples, example plugins and
Python package under an installation prefix. There is no binary installer or
distribution package yet. Two build systems, **CMake** and **qmake**, build the same
application from the same sources; use whichever you prefer.

## What you need

| Dependency | Version used | Why |
|---|---|---|
| C++20 compiler | GCC 13.3 | the language standard of the project |
| CMake, or qmake from Qt 6 (`qmake6`) | CMake 3.21 or later | build system |
| Qt 6: Core, Gui, Widgets, OpenGL, OpenGLWidgets (Test for the tests) | 6.4.2 | the desktop application |
| Eigen | 3.4.0 | vectors, matrices, linear algebra |
| OpenGL | 4.5 Core at run time | the 3D viewport |
| pkg-config | any | lets qmake find Eigen |
| Python 3 interpreter | any | generates this help during the build (standard library only) |

nlohmann-json and GoogleTest come with the source tree. On Ubuntu 24.04:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    qt6-base-dev libqt6opengl6-dev libeigen3-dev libgl1-mesa-dev xvfb
```

`xvfb` is optional: it lets the application tests run without a display.

## Build with CMake

```bash
cmake -S . -B build/cmake-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-release
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build. Further options:
`-DRESONANT_BUILD_APP=OFF` builds only the core library and its tests (no
Qt needed), `-DRESONANT_BUILD_TESTS=OFF` skips the tests. The application
is `src/app/resonant` inside the build directory.

## Build with qmake

```bash
mkdir -p build/debug && cd build/debug
qmake6 ../../project.pro        # add CONFIG+=release for a Release build
make -j"$(nproc)"
```

The application is `build/debug/src/main/resonant`. After adding or
removing a source file, run `qmake6 -r ../../project.pro` again in the
build directory (without `-r` only the top-level Makefile is regenerated).

## Optional components

Both builds look for these by themselves and build without them. The
configure step reports what it found, for example
`Resonant acceleration: VOLK ON|OFF, CUDA ON|OFF` and
`Resonant measurement libraries: HDF5 ON|OFF, ALSA ON|OFF, SoapySDR ON|OFF`.

| Component | Adds | Without it | Turn off (CMake / qmake) |
|---|---|---|---|
| CUDA toolkit | a GPU backend for the acoustic FDTD and the Yee solvers (double precision, NVIDIA compute capability 7.5 or newer) | the CPU runs every solver; choosing the GPU in a panel is refused with the reason | `-DRESONANT_WITH_CUDA=OFF` / `CONFIG+=no_cuda` |
| VOLK (`libvolk-dev`) | SIMD kernels for display conversions and measured sample streams | plain loops, same results | `-DRESONANT_WITH_VOLK=OFF` / `CONFIG+=no_volk` |
| HDF5 (`libhdf5-dev`) | measurement data stored as HDF5 files next to the project | Resonant's own binary format | `-DRESONANT_WITH_HDF5=OFF` / `CONFIG+=no_hdf5` |
| ALSA (`libasound2-dev`) | the audio-interface driver for experiments | simulated instruments only | `-DRESONANT_WITH_ALSA=OFF` / `CONFIG+=no_alsa` |
| SoapySDR | discovery of software-defined radios | no SDR discovery | `-DRESONANT_WITH_SOAPYSDR=OFF` / `CONFIG+=no_soapysdr` |
| Python 3 development headers and pybind11 | the Python API module | the application works without it | `-DRESONANT_WITH_PYTHON=OFF` / `CONFIG+=no_python` |

For CUDA, `RESONANT_CUDA_ARCHITECTURES` (CMake, default
`75-real;86-real;89-real;120`) or `RESONANT_CUDA_ARCHS` and `CUDA_DIR`
(qmake) select the GPU architectures and the toolkit. Setting up Python is
described in [Python installation and configuration](../python/installation.md).

> **Note** The CUDA backend is implemented and compiled, but it has not yet been validated on a GPU. The CPU backend is the reference; every result records where it ran.

## First launch

Start the application, optionally with a project file:

```bash
build/cmake-release/src/app/resonant examples/mechanics/b-vibrating-plate.resonant
```

You see the main window with the viewport in the middle, the **Scene**
panel on the left, **Properties** and the domain panels on the right and
the **Analysis** panel below ([Interface overview](interface.md)).
Without a project file the window opens an empty project called
*Untitled* that already contains the example material library.

Look at two tabs of the **Analysis** panel once:

- **Console** shows the startup log: version, operating system, Qt platform plugin, dependency versions and the compute capabilities found.
- **Diagnostics** lists the environment, the graphics driver, the project and, when CUDA is built in, the CUDA runtime, driver and each device with its memory.

**Help > About Resonant** shows the version (0.1.0) and the graphics
context in use.

If OpenGL 4.5 Core cannot be created, Resonant says so in a message box
titled *3D graphics are unavailable*. The viewport stays empty, but
projects can still be opened, edited through the panels and saved: there
is no silent fallback.

## Command-line options

| Option | Effect |
|---|---|
| `project` (first argument) | open this project file |
| `--help`, `--version` | print the options or the version |
| `--screenshot <file>` | save an image of the main window once it is shown, after the requested run if there is one, then exit |
| `--run-field` | run the field simulation of the project's first field region |
| `--run-full-wave` | run the acoustic full-wave solver on the first field region |
| `--run-mechanics <analysis>` | `modal`, `harmonic`, `transient` or `static` on the first structure |
| `--run-em <what>` | `field`, `pattern` or `full-wave` for the first antenna and region |
| `--run-experiment <id>` | run an experiment (`first` for the first one) of a saved project |
| `--authorize-output <operator>` | authorize that experiment's physical outputs, as this operator |
| `--run-optimization <id>` | run an optimization problem (`first` for the first one) |

## Common mistakes

- Running qmake without `-r` after adding files: new sources are not compiled.
- Missing `pkg-config`: qmake cannot find Eigen.
- Expecting the GPU to be used automatically: the backend is chosen per run in the full-wave panels, and a CUDA choice that cannot run is refused, never replaced.
- Starting the application on a remote session without OpenGL 4.5: use a machine with a suitable driver, or Mesa's software renderer.

## Limitations

- Only Linux has been built and run. Nothing platform-specific is used on purpose, but Windows and macOS builds are untested.
- The viewport has been tested with Mesa's software renderer (llvmpipe), not yet with a hardware GPU driver.
- There is no binary installer or distribution package: run the application from its build directory, or install it under a prefix with `make install` / `cmake --install` (the help then works from `<prefix>/share/resonant/help`).

See [Installation problems](../troubleshooting/installation.md) and
[Missing dependencies](../troubleshooting/dependencies.md) when a step fails.
