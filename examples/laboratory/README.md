# Laboratory examples

Open these in **Panels > Laboratory** (`exp.laboratory`) after saving a copy: imported
data is kept next to the project file. All datasets in `data/` are synthetic,
generated with the parameters stated in their headers.

| Project | What it shows |
|---|---|
| `a-resonator-frf.resonant` | a resonator's frequency response: mass, stiffness and damping calibrated on one run, validated on another |
| `b-ring-down.resonant` | a tuning fork's free decay: 440 Hz and its Q against a prediction |
| `c-thermal-slab.resonant` | a heated sheet's mid-plane temperature against the coupled engine: the conductivity calibrated and applied as a new material revision |
| `d-dielectric-reflection.resonant` | a sheet on a metal plate: permittivity and loss tangent from S11, and what happens when the thickness is freed too |
| `e-stress-strain.resonant` | a tensile coupon: yield stress and hardening of the J2 model |
| `f-energy-balance.resonant` | a heated block's energy balance against its measurement uncertainty, with an unmeasured pathway |
| `g-invalid-data.resonant` | files with problems: how the import report lists them |

Every example is run by `LaboratoryExamples.*`.
