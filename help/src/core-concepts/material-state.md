---
id: concepts.material-state
title: Material State and Nonlinear Matter
category: Core Concepts
order: 33
status: available
summary: Materials that remember what happened to them - plasticity, damage, viscoelasticity, melting and magnetic hysteresis - systems that evolve in time steps, records of their history, and evolved states committed to a material with their lineage.
keywords: [material state, plasticity, yield, damage, hysteresis, Preisach, phase change, melting, latent heat, viscoelastic, evolution, history, nonlinear]
related: [concepts.materials, concepts.multiscale, opt.functional-structures]
---

A library material says what a material *is*. **Material State**
(**Panels > Material State**) shows what it *becomes*: a steel that yields
and keeps a residual strain, a laminate that cracks and loses stiffness, a
wax that melts, a ferrite that remembers its last field.

| Model | What it remembers | What you see |
|---|---|---|
| J2 plasticity | plastic strain, hardening, back stress | stress-strain loops, residual strain, the Bauschinger shift |
| Scalar damage | the largest strain reached | softening, secant unloading, dissipated energy |
| Damaged cantilever | damage per element | bending frequencies falling cycle by cycle |
| Standard linear solid | the viscous strain | relaxation, loss per cycle |
| Melting slab | enthalpy and liquid fraction per cell | the melt front against the Stefan solution |
| Preisach hysteresis | the relay states | major and minor loops, loss per cycle and core power |
| Coupled evolution | the temperature field | properties, stress and thickness changing step by step |

Open a project in `examples/materialstate`, choose a **Study** and **Run**.
Or choose a **Model** and a loading and **Try** it. The plot shows the
response curve. For melting and evolutions, a **Profile** with its
**Snapshot** shows the state along the slab. The table lists the history.

## Records and commits

**Save record** keeps the run's history in the project, and you can undo
it. **Commit** writes the evolved values (for example the damaged modulus)
into a chosen library material:

- The provenance is *computed*, with a derivation that leads back to the
  record ([Material Scales and Lineage](multiscale-materials.md)).
- Materials derived from the changed one become stale.
- One undo reverts it.

## Reading a result

- **Energy:** the work done equals the stored energy plus the dissipated
  energy, and the residual says how well. A loop's area is its loss.
- **Reliable or not:** a run says why it may not be. For example, a
  softening law localizes in a structure and depends on the mesh.
- **Model card:** the card lists the assumptions, the excluded effects and
  the validity range.

## Limits

Small strain only. These models are listed as **not implemented** and are
never run:

- crack propagation;
- fatigue;
- finite-strain plasticity;
- hyperelasticity;
- nonlinear dielectrics, optics and acoustics;
- reaction kinetics.

In a coupled evolution, only the temperature-dependent laws evolve. Plastic
and damage states run in their own studies.
