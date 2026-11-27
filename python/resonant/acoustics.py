"""Acoustic fields, focusing and settings (analytical free field or the
Kirchhoff reflector model; the full-wave solver is in ``simulation``)."""
from ._native import native


def _id(x):
    return x.id if hasattr(x, "id") else x


def settings(project) -> dict:
    return project.settings("acoustics")


def set_frequency(project, frequency: float) -> None:
    project.set_settings("acoustics", {"frequencyHz": frequency})


def field(project, region, model: str = "auto") -> dict:
    """Complex pressure on a region: dict with 'pressure' (complex ndarray [w][v][u], Pa),
    the grid ('origin', 'axis_u', 'axis_v', 'axis_w', 'shape'), 'max_pressure', 'assumptions'."""
    return native.acoustic_field(project, _id(region), model)


def pressure(project, points, model: str = "auto"):
    """Complex pressure (Pa) at points [(x, y, z), ...] as a NumPy array."""
    return native.acoustic_pressure(project, [list(p) for p in points], model)


def focus(project, array, target=None, point=None, control: str = "phase") -> dict:
    """Focus an array on a target object or a point by phase or time delay (changes the array)."""
    params = {"array": _id(array), "control": control}
    if point is not None:
        params["point"] = list(point)
    else:
        params["target"] = _id(target)
    return native.run_action(project, "acoustics.focus", params)
