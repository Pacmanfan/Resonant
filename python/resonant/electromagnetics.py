"""Antennas and arrays: impedance, patterns, steering, tapers."""
from ._native import native


def _id(x):
    return x.id if hasattr(x, "id") else x


def antenna(project, antenna) -> dict:
    """Induced-EMF impedance, return loss, directivity and gain of an analytical antenna."""
    return native.em_antenna(project, _id(antenna))


def pattern(project, obj, resolution_deg: float = 2.0) -> dict:
    """Normalized far-field pattern: {'magnitude': ndarray [theta][phi], 'step_deg', 'directivity', 'model'}."""
    return native.em_pattern(project, _id(obj), resolution_deg)


def pattern_metrics(project, obj, resolution_deg: float = 1.0) -> dict:
    """Directivity (dBi), main lobe, sidelobe level and beamwidth."""
    return native.run_action(project, "em.pattern", {"object": _id(obj), "resolutionDeg": resolution_deg})


def steer(project, array, theta_deg: float, phi_deg: float, method: str = "phase") -> dict:
    return native.run_action(project, "em.steer", {"array": _id(array), "thetaDeg": theta_deg, "phiDeg": phi_deg, "method": method})


def taper(project, array, sidelobe_db: float = 30.0) -> dict:
    """Dolph-Chebyshev amplitudes for a linear array."""
    return native.run_action(project, "em.taper", {"array": _id(array), "sidelobeDb": sidelobe_db})
