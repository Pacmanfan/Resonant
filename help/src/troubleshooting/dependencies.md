---
id: trouble.dependencies
title: Missing dependencies
category: Troubleshooting
order: 30
status: available
summary: Which optional libraries Resonant uses, how to see whether a build has them, and what changes when one is missing.
keywords: [dependencies, VOLK, CUDA, HDF5, ALSA, SoapySDR, pybind11]
related: [trouble.installation, trouble.devices, sim.resources, py.installation, trouble.python]
---

Resonant needs only a C++20 compiler, CMake or qmake, Qt 6, Eigen and
OpenGL ([Installation problems](installation.md)). Everything
below is **optional**: both builds look for it, use it when it is found and
build without it otherwise. A missing optional library never changes a
result silently; the feature that needs it is either absent or refuses with
the reason.

## Does my build have it?

The configure step prints three lines:

```text
Resonant acceleration: VOLK ON|OFF, CUDA ON|OFF
Resonant measurement libraries: HDF5 ON|OFF, ALSA ON|OFF, SoapySDR ON|OFF
Resonant Python API module: ON|OFF
```

In the running application, **Analysis > Diagnostics > Compute** lists
*VOLK vector kernels* and *CUDA* (or *not in this build*) and every CUDA
device with its free memory and whether it can be used. The **Devices** tab
of the **Experimental Hardware** panel lists each hardware driver as *built*
or *not built*.

## The optional libraries

| Library | Install (Ubuntu) | Used for | Without it |
|---|---|---|---|
| VOLK 3.1 | `libvolk-dev` | SIMD kernels for single-precision data: field display conversions, measured sample streams | plain loops do the same work |
| CUDA toolkit 13.5 | from NVIDIA | the GPU backend of the acoustic and electromagnetic full-wave solvers | only *CPU (reference, double precision)* can run |
| HDF5 1.10 | `libhdf5-dev` | measurement data in `.h5` files next to the project | measurements use Resonant's binary format (`.rmd` folders) |
| ALSA 1.2 | `libasound2-dev` | the audio-interface driver (capture and playback) | no audio interfaces are discovered |
| SoapySDR 0.8 | from its project | discovery and capabilities of software-defined radios | no SDRs are discovered |
| Python 3 headers and pybind11 | `python3-dev`, `pybind11-dev` | the Python API module | no Python API; the application itself does not need it |

## Turning a library off

| Library | CMake | qmake |
|---|---|---|
| VOLK | `-DRESONANT_WITH_VOLK=OFF` | `CONFIG+=no_volk` |
| CUDA | `-DRESONANT_WITH_CUDA=OFF` | `CONFIG+=no_cuda` |
| HDF5 | `-DRESONANT_WITH_HDF5=OFF` | `CONFIG+=no_hdf5` |
| ALSA | `-DRESONANT_WITH_ALSA=OFF` | `CONFIG+=no_alsa` |
| SoapySDR | `-DRESONANT_WITH_SOAPYSDR=OFF` | `CONFIG+=no_soapysdr` |
| Python module | `-DRESONANT_WITH_PYTHON=OFF` | `CONFIG+=no_python` |

After installing a library, configure again (CMake: a new tree, or delete
the cache; qmake: `qmake6 -r`) so that it is found.

## Symptoms of a missing library

**CUDA chosen, but not built.** The preflight of the **Full-Wave Solver**
panel, or the **Plan** of **Electromagnetics > Fields**, is refused:

```text
CUDA was chosen, but this build has no CUDA support (no CUDA toolkit was
found when it was configured). Choose the CPU, or free the device; the
backend is never switched automatically.
```

Choose *CPU (reference, double precision)* under **Compute**. The CPU and
CUDA backends give the same numbers to rounding, so nothing is lost but time.
A build with CUDA also needs, at run time, an NVIDIA GPU of compute
capability 7.5 or newer, a driver that supports the CUDA runtime the build
used, and free GPU memory for the solver state plus 64 MiB.

> **Warning** The CUDA backend is implemented and compiled but has not yet
> been validated on a GPU. Compare a CUDA result with a CPU result before
> relying on it.

**An audio interface or SDR is not discovered.** Check that the driver is
*built* on the **Devices** tab; see
[Device connection failures](devices.md).

**Measurement data cannot be read** after moving a project between builds.
A build reads measurement data only in its own storage format: a build with
HDF5 reads `.h5` data sets and a build without HDF5 reads `.rmd` folders.
Opening a project recorded with the other format shows, for its data sets,
`the measurement data set <experiment>/<id>.h5 is missing from ...` or
`... is not a readable HDF5 file`. Open such a project with a build of the
same kind as the one that recorded it ([Data formats](../reference/data-formats.md)).

## Licences

Qt (LGPL-3.0), VOLK (LGPL-3.0), HDF5 (BSD-style), ALSA (LGPL-2.1) and
SoapySDR (BSL-1.0) are linked dynamically; the CUDA runtime is linked
statically. A distribution that includes them ships their libraries and
licence texts.

## Related topics

- [Installation problems](installation.md)
- [Device connection failures](devices.md)
- [Resource estimation](../simulation/resources.md)
- [Python installation and configuration](../python/installation.md)
