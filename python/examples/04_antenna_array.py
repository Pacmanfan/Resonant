"""A half-wave dipole, then a 10-element array: taper and steering.

Prints the dipole's induced-EMF impedance and directivity, then the array's
directivity and sidelobe level uniform, with a 30 dB Dolph-Chebyshev taper,
and steered 30 degrees off broadside.
"""
import resonant

p = resonant.Project("Antennas")
dipole = resonant.scene.add_antenna(p, "Dipole", parameters={"length": 0.15, "wireRadius": 0.001})
r = resonant.electromagnetics.antenna(p, dipole)
print(f"dipole: Z = {r['inputResistance']:.1f} {r['inputReactance']:+.1f}j ohm, directivity {r['directivityDbi']:.2f} dBi")

array = resonant.scene.add_em_array(p, "Array", count=10, spacing=0.15)
for label, change in (("uniform", None), ("30 dB taper", lambda: resonant.electromagnetics.taper(p, array, 30.0)),
                      ("steered 30°", lambda: resonant.electromagnetics.steer(p, array, 30.0, 0.0))):
    if change:
        change()
    m = resonant.electromagnetics.pattern_metrics(p, array)
    print(f"array {label:12}: directivity {m['directivityDbi']:.2f} dBi, sidelobes {m['sidelobeDb']:.1f} dB")
