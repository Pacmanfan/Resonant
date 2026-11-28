---
id: concepts.architected-matter
title: Architected Matter: Organizing Materials for a Function
category: Core Concepts
order: 34
status: available
summary: Describe how the matter of a part is organized - regions, layers, graded properties, orientations and the interfaces between them - and let the solvers say what that organization does, what may fail and whether it can be made.
keywords: [architected matter, functional region, layer stack, interface, graded material, orientation, anisotropy, piezoelectric, contact resistance, delamination, manufacturability, energy converter]
related: [concepts.materials, concepts.multiscale, concepts.accounting, exp.laboratory]
---

A part is more than a material and a shape. A beam with a bonded piezoelectric
plate, a turbine wall with a ceramic coating, or a plate graded from metal to
ceramic each works because of how its matter is **organized**. The **Matter**
panel (**Panels > Matter**) holds that organization as an *architecture*. Open a
project in `examples/matter` to see one.

## What an architecture holds

- **Regions:** each region is part of a scene object and has a role (substrate,
  transduction, electrode, barrier ...). It is made of a library material, a
  composition with a stated mixing rule, or a graded profile. It may also have
  an orientation and direction-dependent properties.
- **Layer stacks:** the layers on an object, from the substrate outwards, each
  with its thickness. The panel draws them to scale as an *idealized model*.
- **Interfaces:** what happens where two regions meet:
  - perfect bonding;
  - a thermal or electrical contact resistance;
  - a bond strength.

  An interface is never assumed perfect. If it says nothing about heat, the
  thermal analysis refuses the stack and tells you why.
- **Processes:** how each region is made, with the limits you state (minimum
  layer thickness, process temperature ...).

## Analyze, explore, check

- **Analyze:**
  - **lamination theory:** stiffness and thermal curvature;
  - **bending modes:** the coated structure against the bare one;
  - **the piezoelectric cantilever:** harvested power and its energy balance;
  - **conduction through the thickness**, including contact resistances;
  - **the layered electromagnetic solver;**
  - **the three-dimensional layered solid:** modes, or a static response
    with interface tractions and initiation indices, and steady conduction.
    The stack lies on a plane, cylinder, sphere or revolved profile given in
    the options. Each interface's model is used as declared: perfect bonding,
    springs, sliding, friction or a contact resistance.

  Each result lists its assumptions and warnings. An analysis that cannot
  represent part of the stack says so instead of guessing.
- **What can it do?** lists what the architecture can do and what may fail:
  - a film that sits on the neutral surface;
  - a missing electrode;
  - dielectric loss;
  - a mismatch stress above the bond strength.

  Findings are kept apart from hypotheses, and nothing is changed.
- **Mechanisms** lists, for a desired conversion (say thermal to
  electrical), what Resonant models and what it does not (there is no
  thermoelectric model, for example).
- **Check manufacturability** compares the layers with their processes. That
  includes the stress built in when a film cools from its deposition
  temperature.

## Alternatives

**Duplicate as alternative** copies an architecture to change, and **Compare**
puts the alternatives side by side without forming a single score. **Create
physical system** sends a stack to the Coupled Energy–Matter Engine. Every change
is one undo step.

## Limits

- Curved layered bodies (a bell's profile) are analyzed in three dimensions, linearly; interface damage evolution is not modelled.
- Piezoelectric conversion is a single-mode cantilever model.
- The example data are representative handbook values, labelled as such.
- An optimized or explored architecture is a prediction until it is measured.
