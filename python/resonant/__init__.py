"""Resonant Python API (API version 1.0).

Python orchestrates Resonant's C++ engines; it never re-implements their
physics. Projects, scenes, materials, simulations, optimization, experiments
definitions, workflows and reports are reachable from here::

    import resonant
    project = resonant.Project("Demo")
    array = resonant.scene.add_array(project, "Array", count=8, spacing=0.0043)
    target = resonant.scene.add_target(project, "Focus", position=(0.01, 0, 0.06))
    resonant.acoustics.focus(project, array, target)
    print(resonant.acoustics.pressure(project, [(0.01, 0, 0.06)]))

Compatibility: within API major version 1, functions and fields are only
added, never removed or changed in meaning; ``API_VERSION`` and
``resonant.require(major, minor)`` let scripts state what they need.
Objects hold their project: an ObjectRef stays usable while its object
exists and raises ``resonant.Error`` once it was removed.
"""
from ._native import native as _native

API_VERSION = "1.0"
if _native.api_version().split(".")[0] != API_VERSION.split(".")[0]:
    raise ImportError(f"the native module has API {_native.api_version()}, this package needs {API_VERSION}")

Error = _native.Error
Project = _native.Project
ObjectRef = _native.ObjectRef
CORE_VERSION = _native.core_version()

from . import project, scene, geometry, materials, acoustics, mechanics, electromagnetics  # noqa: E402
from . import simulation, optimization, experiments, measurements, automation  # noqa: E402
from . import analysis, reporting, io, plugins  # noqa: E402


def require(major: int, minor: int = 0) -> None:
    """Raise unless this API is at least ``major.minor`` within the same major version."""
    have = tuple(int(x) for x in API_VERSION.split("."))
    if have[0] != major or have[1] < minor:
        raise Error(f"this script needs Resonant API {major}.{minor}; this is {API_VERSION}")


__all__ = ["API_VERSION", "CORE_VERSION", "Error", "Project", "ObjectRef", "require", "project", "scene", "geometry",
           "materials", "acoustics", "mechanics", "electromagnetics", "simulation", "optimization", "experiments",
           "measurements", "automation", "analysis", "reporting", "io", "plugins"]
