"""Measurement records of a project and comparison of measured with simulated values."""
import math

from . import experiments


def records(project, experiment_id: str = None):
    out = experiments.workspace(project).get("measurements", [])
    return [m for m in out if experiment_id is None or m.get("experimentId") == experiment_id]


def compare(measured, simulated) -> dict:
    """Error metrics of two equal-length sequences of real values on the same points:
    rms error, largest absolute error and the rms relative to the measured rms. Only
    compatible quantities in the same unit may be compared."""
    if len(measured) != len(simulated) or not measured:
        raise ValueError("measured and simulated values must have the same, non-zero length")
    diff = [m - s for m, s in zip(measured, simulated)]
    rms = math.sqrt(sum(d * d for d in diff) / len(diff))
    ref = math.sqrt(sum(m * m for m in measured) / len(measured))
    return {"rmsError": rms, "maxError": max(abs(d) for d in diff), "relativeRms": rms / ref if ref > 0 else float("inf"), "points": len(diff)}
