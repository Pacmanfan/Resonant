"""Geometry kinds and their parameters."""
from ._native import native


def kinds(category: str = None):
    return [k for k in native.geometry_kinds() if category is None or k["category"] == category]


def kind(kind_id: str) -> dict:
    for k in native.geometry_kinds():
        if k["id"] == kind_id:
            return k
    raise KeyError(kind_id)
