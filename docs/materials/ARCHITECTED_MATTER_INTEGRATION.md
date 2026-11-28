# Architected Matter: Integration

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-008, -013, -014, -018, ADR-000228).

## Multiscale (scale-to-scale transfer)

A region's composition can be resolved in three ways:
- through its stated method (`matter/Mixing`);
- by pointing its microstructure at a multiscale representation, whose
  derivations and lineage give the property (RES-MSC);
- by stating a value on the region, with its provenance (measured on the
  batch, computed, homogenized).

Every effective value carries its method, the rigorous bounds and its validity.
Maxwell Garnett beyond a 0.3 inclusion fraction and Gibson–Ashby above a 0.3
relative density are computed but not valid, and the analyses refuse them.
Units are SI throughout. Orientations are stated in the stack's frame, so a
rotated anisotropic set is transformed before use: the through-thickness
conductivity is `R diag(k) R^T`, and the ply angles are those of lamination
theory.

## The coupled engine

`coupling::stackToPhysicalSystem` turns a stack into a layered-slab physical
system:
- one region per layer, bound to the stack's object, on the region's library
  material (or the matrix of its composition);
- graded layers as sublayers;
- every value that is not the library's as a constant law, carrying its
  provenance (`homogenized` for compositions) and its source;
- thermal contact resistances on the thermal participant's interfaces.

The default domain is a thermal one with convective faces (h = 10 W/(m² K)),
stated in the notes. The engine runs the result like any other system.

The capability `coupled-engine` refuses:
- interfaces without a thermal model;
- electrical interface models under an electromagnetic domain;
- patterns;
- domains other than thermal and electromagnetic.

`MatterService.CreatePhysicalSystemFromStack` adds the system to the project in
one undo step.

## Inverse design

The optimizer's adapter `matter` (`coupling/MatterAdapter`) takes:

| Variable | Binding |
|---|---|
| layer thickness | `matter/layer/<stack>/<i>/thickness` |
| in-plane angle (anisotropic layers) | `matter/layer/<stack>/<i>/angle` |
| electrode coverage | `matter/layer/<stack>/<i>/coverage` |
| material of a region (categorical, among stated choices) | `matter/region/<id>/material` |
| interface parameter | `matter/interface/<id>/<model>/<key>` |

Its quantities are the configured analyses' metrics (`<analysis>.<metric>`), plus
`manufacturing.violations` and `manufacturing.feasible`. With `"manufacturing":
"reject"`, an architecture that violates its constraints is an invalid design.
With `"constrain"` (the default) it is a constraint quantity. Every evaluation
warns that an optimized architecture is a prediction, not a validated design.

The example `g-thickness-tradeoff` sets harvested power against the bending
stiffness the load-bearing function needs. These two objectives conflict. Power
against mass does not: at a fixed base acceleration and tip mass, a lighter,
softer beam harvests more. NSGA-II returns a Pareto set without collapsing it
into one score (`MatterOptimization.CompetingObjectivesGiveAParetoSet`).

## The laboratory

- An architecture is a laboratory model link of kind `matter-architecture`, at
  its revision.
- The laboratory model `matter.stack-metric` predicts a stack analysis's metric
  for each value of an axis option (a heat flux, a frequency). Its parameters
  are the layer thicknesses, the interface parameters and the values stated on
  regions. Measurements therefore calibrate them by the laboratory's rules,
  validated on independent data.
- Manufacturing records list the measured datasets of what was made.

## Accounting

Each evaluated stage of a transformation pathway has a ledger checked by the
accounting framework. For the cantilever, the base's work goes into the load
and the damping. For absorption, the incident wave splits into heat and the
reflected and transmitted waves. A chain efficiency is given only when every
stage is modeled, and a stage reporting more output than input is rejected.
