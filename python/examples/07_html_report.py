"""Write an HTML report of a field study (with a plot when matplotlib is installed)."""
import os
import tempfile

import resonant

OUT = os.environ.get("RESONANT_EXAMPLE_OUTPUT", os.path.join(tempfile.gettempdir(), "resonant-examples"))
os.makedirs(OUT, exist_ok=True)
p = resonant.Project("Report")
array = resonant.scene.add_array(p, "Array", count=12, spacing=0.0043)
resonant.scene.add_target(p, "Focus", position=(0.0, 0.0, 0.05), object_id="focus")
plane = resonant.scene.add_region(p, "Plane", size=(0.08, 0.01, 0.08), samples=(61, 61), position=(0.0, 0.0, 0.045))
resonant.acoustics.focus(p, array, "focus")
field = resonant.acoustics.field(p, plane)
sections = [("Design", {"elements": 12, "spacing (mm)": 4.3, "frequency (kHz)": 40}),
            ("Result", resonant.analysis.peak(field))]
try:
    resonant.reporting.plot_field(field, os.path.join(OUT, "report-field.png"), "|p| on the plane")
    sections.append(("Field", "<img src='report-field.png' width='480'/>"))
except ImportError:
    sections.append(("Field", "(matplotlib is not installed: no plot)"))
resonant.reporting.write_html(os.path.join(OUT, "report.html"), "Focused array study", sections)
print("report written to", os.path.join(OUT, "report.html"))
