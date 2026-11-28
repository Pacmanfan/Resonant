---
id: opt.functional-structures
title: Functional Structures
category: Optimization
order: 80
status: available
summary: Engineered objects whose behavior Resonant computes from their geometry - parameters, analyses, sweeps, staleness and explained candidates.
keywords: [functional structure, geometry, parameter, cavity, resonator, horn, band gap, sweep, sensitivity, stale]
related: [opt.studio, opt.objectives, opt.constraints, opt.metamaterials, ui.panels]
---

A **functional structure** describes an engineered object: the scene objects
it consists of, the materials of its regions, its parameters, the analysis
that computes what it does, and what you want it to do. Resonant never
attaches a behavior to a shape: every number comes from the analysis's
physical model, and the model is named with every result.

Open one of the example projects in `examples/functional` (a microwave
cavity, a two-mirror optical resonator, an acoustic horn, a mechanical
resonator, a photonic unit cell, a coupled slab) or `examples/resonance` (a
Helmholtz resonator, coupled cantilevers, coupled Helmholtz resonators, a
heated cavity) or `examples/metamaterials` (see
[Metamaterials](metamaterials.md)) and choose
**Panels > Functional Structures**.

## The panel

| Part | What it does |
|---|---|
| **Structure** | the project's structures; the line below names the analysis and its category |
| **Parameters** | name, value, unit, range and kind. Type a new value to change the design: a bound parameter changes its scene object (one step you can undo), a free one the structure; derived values are computed and cannot be typed |
| **Model**, **Analyze** | the analysis's models (closed form, full-wave, finite elements, ...) with their fidelity; Analyze runs the chosen model in the background |
| **Metrics** | every metric with its unit; hover a name for its definition |
| **Modes** | resonances with their labels (TE101, TEM00 q, in-phase, ...) where the model labels them; their **nature** (a natural mode, a peak of a driven response, or a resonance identified from a simulated signal), frequency, Q (∞ for a lossless model) and the Q of each loss mechanism |
| **Curves and sweep** | the result's curves (a port response, a beam radius, a horn's resistance, a stack's transmission) - choosing one only redraws; **Sweep** evaluates a parameter range in the background and plots the chosen metric against it |
| **Model and assumptions** | the model, the behavior it established (resonant, broadband, mixed), the mechanism, validity, assumptions, warnings, constraints and objectives |

The status line says whether the result shown still describes the design.
When you change a parameter, a material or the structure, it reports **stale**
with what changed; undo the change and it is current again.

## Designing for a function

A structure carries **objectives** (a metric to minimize, maximize, meet or
keep within a range) and **constraints** (expressions of parameters and
metrics, such as a minimum wall for injection molding or a maximum
temperature). Sweeps rank their feasible points and explain the best one.
For a search, call `FunctionalStructureService.OptimizeStructure` from Python
or the API: the optimization framework proposes candidates, each evaluated by
the analysis on its own copy of the design, and `ExplainCandidates` says for
the best ones which parameters changed, which objectives improved or worsened,
which constraints are active, the physical mechanism and the limits of the
model. To adopt a candidate, set its parameter values.

## Limits

Structures are created through Python, the API or the example projects; the
panel inspects, edits parameters, analyzes and sweeps. The analyses cover
rectangular cavities, paraxial two-mirror resonators, Webster horns, the
mechanical studies, two-layer cells, periodic rods, the coupled slab and the
effective-medium analyses; forces are not computed.
