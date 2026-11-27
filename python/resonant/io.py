"""Data export and import (CSV, JSON)."""
import csv
import json


def export_field_csv(field: dict, path: str) -> None:
    """x, y, z, re, im, |p| for every sample of a field result."""
    import numpy as np
    p = field["pressure"]
    o = np.array(field["origin"])
    au, av, aw = (np.array(field[k]) for k in ("axis_u", "axis_v", "axis_w"))
    nw, nv, nu = p.shape
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["x", "y", "z", "re", "im", "magnitude"])
        for iw in range(nw):
            for iv in range(nv):
                for iu in range(nu):
                    x = o + (au * iu / max(nu - 1, 1)) + (av * iv / max(nv - 1, 1)) + (aw * iw / max(nw - 1, 1))
                    v = p[iw, iv, iu]
                    w.writerow([*x.tolist(), v.real, v.imag, abs(v)])


def read_csv(path: str):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def write_json(path: str, value) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(value, f, indent=2)
