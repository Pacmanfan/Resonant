"""Focus an ultrasonic phased array on a point and measure the focal spot.

Builds a 16-element, 40 kHz linear array, focuses it by phase on a target
6 cm in front of it, calculates the pressure on a plane through the focus,
and reports the peak pressure and the -6 dB width of the focal spot. In the
console the array, target and plane are added to the open project as one
undoable step.
"""
import os
import tempfile

import numpy as np

import resonant
from resonant import app

OUT = os.environ.get("RESONANT_EXAMPLE_OUTPUT", os.path.join(tempfile.gettempdir(), "resonant-examples"))
os.makedirs(OUT, exist_ok=True)

p = project if "project" in globals() else resonant.Project("Focused array")  # noqa: F821 - the console's copy
array = resonant.scene.add_array(p, "Focus array", count=16, spacing=0.0043, frequency=40000.0)
target = resonant.scene.add_target(p, "Focus", position=(0.0, 0.0, 0.06))
# The plane spans x = -4..4 cm and z = 2..10 cm (it starts 2 cm in front of the array,
# away from the elements' near field).
plane = resonant.scene.add_region(p, "Focal plane", size=(0.08, 0.01, 0.08), samples=(81, 81), position=(0.0, 0.0, 0.06))

before = abs(resonant.acoustics.pressure(p, [(0.0, 0.0, 0.06)])[0])
resonant.acoustics.focus(p, array, target)
after = abs(resonant.acoustics.pressure(p, [(0.0, 0.0, 0.06)])[0])

field = resonant.acoustics.field(p, plane)
peak = resonant.analysis.peak(field)
_, v, u = peak["index"]                     # pressure[w][v][u]: v along z, u along x
print(f"|p| at the focus: {before:.2f} Pa unfocused, {after:.2f} Pa focused ({after / before:.1f} x)")
print(f"plane maximum {peak['max_pressure']:.2f} Pa at x = {(-0.04 + u * 0.001) * 100:.1f} cm, z = {(0.02 + v * 0.001) * 100:.1f} cm")
xs = np.linspace(-0.04, 0.04, 401)
line = np.abs(resonant.acoustics.pressure(p, [(x, 0.0, 0.06) for x in xs]))
print(f"-6 dB focal width at z = 6 cm: {resonant.analysis.width_6db(line, xs[1] - xs[0]) * 1000:.1f} mm")
resonant.io.export_field_csv(field, os.path.join(OUT, "focal-plane.csv"))
print("field written to", os.path.join(OUT, "focal-plane.csv"))

if app.running_in_console():
    app.apply(p, "Focused 16-element array")
