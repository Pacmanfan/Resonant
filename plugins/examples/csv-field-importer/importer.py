"""Example Python plugin: a measurement importer (plugin API 1).

Imported data are labelled with their file; the comparison uses compatible
quantities only (pressure magnitude in Pa at the same points)."""
import csv
import os

import resonant


def import_field(path, project):
    with open(path, newline="", encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f) if not r.get("x", "").startswith("#")]
    if not rows:
        raise ValueError(f"{path}: no data rows")
    points = [(float(r["x"]), float(r["y"]), float(r["z"])) for r in rows]
    measured = [float(r["magnitude"]) for r in rows]
    simulated = [abs(v) for v in resonant.acoustics.pressure(project, points)]
    comparison = resonant.measurements.compare(measured, simulated)
    return {"source": os.path.basename(path), "label": "imported measurement (from file)", "points": len(points),
            "model": "acoustic.freefield.analytical", "comparison": comparison}


def register(api):
    api.add_importer("csv-field", "CSV pressure magnitudes", [".csv"], import_field)
