"""Sweep the frequency of a focused array: the focal spot narrows.

Focused by phase, the element waves arrive in phase at the focus, so the
focal pressure of point sources is the same at every frequency (the sum of
their amplitudes over distance). What changes is the size of the focal spot:
its -6 dB width is proportional to λ·z / D (wavelength times focal distance
over the aperture; about 1.3 times it here), so it shrinks as the frequency rises - until the element spacing
exceeds half a wavelength and grating lobes appear beside the focus.
Writes a CSV table and, when matplotlib is installed, a plot.
"""
import os
import tempfile

import numpy as np

import resonant

OUT = os.environ.get("RESONANT_EXAMPLE_OUTPUT", os.path.join(tempfile.gettempdir(), "resonant-examples"))
os.makedirs(OUT, exist_ok=True)
Z, COUNT, SPACING = 0.05, 12, 0.0043
xs = np.linspace(-0.04, 0.04, 401)

rows = []
for frequency in (20e3, 30e3, 40e3, 50e3, 60e3):
    p = resonant.Project("Sweep")
    array = resonant.scene.add_array(p, "Array", count=COUNT, spacing=SPACING, frequency=frequency)
    resonant.scene.add_target(p, "Focus", position=(0.0, 0.0, Z), object_id="focus")
    resonant.acoustics.set_frequency(p, frequency)
    resonant.acoustics.focus(p, array, "focus")
    line = np.abs(resonant.acoustics.pressure(p, [(x, 0.0, Z) for x in xs]))
    width = resonant.analysis.width_6db(line, xs[1] - xs[0])
    wavelength = p.settings("acoustics")["medium"]["soundSpeed"] / frequency
    aperture = COUNT * SPACING
    side = line[np.abs(xs) > 1.5 * width].max() / line.max()  # the largest level away from the focal spot
    rows.append({"frequency_kHz": frequency / 1e3, "spacing_wavelengths": round(SPACING / wavelength, 3),
                 "width_mm": round(width * 1e3, 2), "estimate_mm": round(wavelength * Z / aperture * 1e3, 2),
                 "largest_side_level_dB": round(20 * np.log10(side), 1)})
    print(f"{frequency / 1e3:5.0f} kHz  d/λ = {SPACING / wavelength:.2f}  -6 dB width {width * 1e3:5.2f} mm "
          f"(λz/D ≈ {wavelength * Z / aperture * 1e3:5.2f} mm)  largest side level {20 * np.log10(side):6.1f} dB")

resonant.reporting.write_csv(os.path.join(OUT, "frequency-sweep.csv"), rows)
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots()
    ax.plot([r["frequency_kHz"] for r in rows], [r["width_mm"] for r in rows], "o-", label="calculated")
    ax.plot([r["frequency_kHz"] for r in rows], [r["estimate_mm"] for r in rows], "--", label="λ z / D")
    ax.set_xlabel("frequency (kHz)")
    ax.set_ylabel("-6 dB focal width (mm)")
    ax.legend()
    fig.savefig(os.path.join(OUT, "frequency-sweep.png"), dpi=100)
    print("plot written to", os.path.join(OUT, "frequency-sweep.png"))
except ImportError:
    print("(matplotlib is not installed: no plot)")
