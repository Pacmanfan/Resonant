"""Structural analyses (finite elements)."""
from ._native import native


def _id(x):
    return x.id if hasattr(x, "id") else x


def modal(project, structure=None, modes: int = 6) -> dict:
    """Natural frequencies: {'frequencies': [Hz, ...], 'method': ...}."""
    return native.mechanics_modal(project, _id(structure) if structure is not None else "", modes)


def harmonic(project, structure=None, f_min: float = 10.0, f_max: float = 1000.0, points: int = 200) -> dict:
    """Forced response at the structure's sensors and targets: peak frequency and displacement per point."""
    params = {"fMin": f_min, "fMax": f_max, "points": points}
    if structure is not None:
        params["structure"] = _id(structure)
    return native.run_action(project, "mechanics.harmonic", params)
