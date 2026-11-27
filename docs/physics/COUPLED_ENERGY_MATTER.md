# Coupled Energy–Matter Interaction

**Status:** planned (RES-SFE-006..008, ADR-000219). Implemented today: sound
radiated by a vibrating plate, one-way (`docs/mechanics/FLUID_STRUCTURE_COUPLING.md`).
The software structure is in [the coupled multiphysics
architecture](../architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md); this
page is about the physics.

## What crosses between domains

| Quantity | From → to | Physical content | Typical model |
|---|---|---|---|
| Surface velocity / pressure | mechanics ↔ acoustics | fluid–structure interaction at a wetted surface | normal velocity continuity, pressure loading |
| Absorbed power density | electromagnetics → thermal | Joule and dielectric loss | q = ½ σ|E|² + ½ ω ε₀ ε″ |E|² (time-averaged) |
| Acoustic absorption heat | acoustics → thermal | viscous and thermal losses, absorber materials | q = 2 α I (plane-wave estimate) or volumetric loss models |
| Temperature | thermal → mechanics, EM, acoustics | expansion, property changes | ε_th = α_T ΔT; ε(T), c(T), E(T) |
| Thermal stress / strain | thermal → mechanics | constrained expansion | σ = C : (ε − α_T ΔT I) |
| Displacement | mechanics → geometry → any | deformation changes boundaries | geometry update and re-setup |
| Radiation force / pressure | acoustics or EM → mechanics | momentum flux of waves | acoustic radiation force from the time-averaged momentum flux; Maxwell stress tensor |
| Charge, current | circuits ↔ EM, piezoelectric ↔ mechanics | transduction | constitutive coupling (piezoelectric equations), port models |
| Material state | any → materials | phase, damage, microstructure | state evolution laws ([material state](../materials/MATERIAL_STATE_ARCHITECTURE.md)) |

Every row is a physical model with assumptions; a coupling is valid only
inside them (for example the plane-wave absorption estimate fails near
sources and in reverberant fields).

## Coupling strength decides the scheme

- **One-way** when the receiver's response does not measurably change the
  source: vibration radiating into air; heating too small to detune.
- **Two-way** when it does: a membrane in water (added mass and radiation
  damping), a resonator detuned by its own heating, a structure driven at
  resonance whose stiffness changes with temperature.
- A coupling run states which it assumed; a one-way result of a strongly
  coupled problem is reported with that limitation, as the plate radiation
  already is.

## Time scales

Wave phenomena (µs–ms), heat conduction (ms–hours) and material evolution
(seconds to years) differ by orders of magnitude. The usual treatment, and
the planned one: harmonic or periodic wave solutions averaged over a period
feed slow processes (heat), which update properties between wave solutions.
Fully time-resolved coupling is reserved for problems where the time scales
meet (radiation force on light structures, transient heating by short
pulses).

## Energy and momentum

Every coupled result carries its accounting ([energy and
momentum](ENERGY_AND_MOMENTUM_ACCOUNTING.md)): energy that leaves one
domain must appear in another or in a stated loss, within the residual the
transfer operators allow.

## Validation path

1. Analytical one-way cases (uniform absorption in a slab → temperature rise
   → free expansion).
2. Benchmarks with published results (thermoelastic damping of a beam;
   microwave heating of a dielectric load).
3. Convergence studies in mesh, grid, time step and iteration tolerance.
4. Experiment: temperature and frequency drift of a resonator measured with
   the experiment framework (stage 12 of the roadmap).
