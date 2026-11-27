"""Example Python plugin: a geometry generator (plugin API 1).

It uses only the public resonant API: the array it adds is an ordinary
acoustic array with a custom layout, evaluated by Resonant's own models."""
import math

import resonant


def generate(project, count=32, radius=0.03, frequency=40000.0, name="Spiral array"):
    """Element i at r = R sqrt((i + 0.5) / N), angle i * golden angle."""
    count = int(count)
    if count < 2 or radius <= 0:
        raise ValueError("count must be at least 2 and radius positive")
    golden = math.pi * (3.0 - math.sqrt(5.0))
    positions = []
    for i in range(count):
        r = radius * math.sqrt((i + 0.5) / count)
        positions.append([r * math.cos(i * golden), r * math.sin(i * golden), 0.0])
    array = resonant.scene.add_array(project, name, layout="custom", count=count, frequency=frequency, customPositions=positions)
    return {"id": array.id, "elements": count, "aperture": 2 * radius}


def register(api):
    api.add_geometry_generator("fermat-spiral-array", "Fermat-spiral acoustic array", generate,
                               parameters={"count": {"default": 32, "unit": ""}, "radius": {"default": 0.03, "unit": "m"},
                                           "frequency": {"default": 40000.0, "unit": "Hz"}})
