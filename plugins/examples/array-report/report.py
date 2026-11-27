"""Example Python plugin: an analysis extension (plugin API 1). It reads the
project and returns a table; it changes nothing."""
import math

import resonant

C_SOUND = None  # taken from the project's acoustic medium


def _spacing(positions):
    best = math.inf
    for i in range(len(positions)):
        for j in range(i + 1, len(positions)):
            best = min(best, math.dist(positions[i], positions[j]))
    return best


def analyse(project):
    rows = []
    acoustic = project.settings("acoustics")
    speed = acoustic["medium"]["soundSpeed"]
    for obj in project.objects():
        c = obj.component
        if obj.component_type == "acoustic.array":
            f = c["element"]["frequencyHz"]
            spacing = c["spacingX"] if c["layout"] in ("linear", "rectangularPlanar") else (_spacing(c["customPositions"]) if c["customPositions"] else float("nan"))
            wavelength = speed / f
            count = c["countX"] * (c["countY"] if c["layout"] == "rectangularPlanar" else 1) if c["layout"] != "custom" else len(c["customPositions"])
            rows.append({"object": obj.name, "domain": "acoustic", "elements": count, "spacingWavelengths": spacing / wavelength,
                         "gratingLobeRisk": bool(spacing > wavelength / 2), "model": "geometry only"})
        elif obj.component_type == "em.array":
            wavelength = 299792458.0 / c["frequencyHz"]
            metrics = resonant.electromagnetics.pattern_metrics(project, obj, 2.0)
            rows.append({"object": obj.name, "domain": "electromagnetic", "elements": c["countX"] * c["countY"],
                         "spacingWavelengths": c["spacingX"] / wavelength, "gratingLobeRisk": bool(c["spacingX"] > wavelength / 2),
                         "directivityDbi": metrics["directivityDbi"], "sidelobeDb": metrics["sidelobeDb"], "model": metrics["model"]})
    return {"arrays": rows, "note": "spacing above half a wavelength allows grating lobes when the beam is steered"}


def register(api):
    api.add_analysis("array-report", "Array report", analyse)
