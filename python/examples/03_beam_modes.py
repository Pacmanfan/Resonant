"""Natural frequencies of an aluminium cantilever for three thicknesses.

The first bending frequency of a cantilever grows linearly with its
thickness (f1 = 0.5596 / L² · sqrt(E I / ρ A) for Euler-Bernoulli beams);
the finite-element modal analysis shows the same trend.
"""
import resonant

print(f"{'thickness':>10}  {'f1 (Hz)':>9}  {'f2 (Hz)':>9}  {'f3 (Hz)':>9}")
for thickness in (0.003, 0.005, 0.008):
    p = resonant.Project("Cantilever")
    resonant.scene.add_structure(p, "Beam", parameters={"width": 0.5, "depth": 0.03, "height": thickness},
                                 supports=["clamped", "free", "free", "free", "free", "free"])
    f = resonant.mechanics.modal(p, modes=3)["frequencies"]
    print(f"{thickness * 1000:8.1f} mm  {f[0]:9.2f}  {f[1]:9.2f}  {f[2]:9.2f}")
