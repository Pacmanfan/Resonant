# Physical System Model

**Status:** implemented (RES-CEME-005, ADR-000220): `src/core/multiphysics/PhysicalSystem`.

A physical system describes interacting domains on one engineered object:
which regions of which materials, which domains with which solvers, how they
are coupled, how the materials depend on state, the environment, the scheme
and what to observe. It is a JSON document stored in the project (extension
`multiphysics`, `{"format": "resonant.multiphysics", "version": 1, "systems": [...]}`),
validated before every run.

```text
PhysicalSystem
├── geometry       kind ("layered-slab"), lateral area
├── environment    ambient (reference, stress-free) temperature, half-space materials
├── regions[]      id, name, materialId, thickness, cells, objectId (optional scene object)
├── domains[]      id, kind, solver, settings
├── couplings[]    id, model, from (domain id), to (domain id), transfer (optional)
├── materialLaws[] region, property, law (constant / linear / table), validity, provenance
├── scheme         one-way / sequential (passes) / iterative (max iterations, tolerances, relaxation), balance tolerance
└── observations[] id, point
```

## Example

```json
{
  "format": "resonant.physical-system", "version": 1,
  "id": "sys-microwave-heated-slab", "name": "Microwave-heated slab",
  "geometry": {"kind": "layered-slab", "lateralArea": 0.01},
  "environment": {"ambientTemperature": 293.15},
  "regions": [{"id": "slab", "materialId": "mat-ex-lossy-dielectric", "thickness": 0.01, "cells": 50, "objectId": "ex-multiphysics-slab"}],
  "domains": [
    {"id": "em", "kind": "electromagnetic", "solver": "em.layered.transfer-matrix", "settings": {"frequencyHz": 2.45e9, "incidentIntensity": 2e4}},
    {"id": "heat", "kind": "thermal", "solver": "thermal.conduction.fv",
     "settings": {"faces": {"-z": {"kind": "convective", "h": 20}, "+z": {"kind": "convective", "h": 20}}}},
    {"id": "solid", "kind": "mechanical", "solver": "mechanics.fe.solid-column", "settings": {"lateral": "free"}}],
  "couplings": [
    {"id": "absorption", "model": "em.absorption-to-heat", "from": "em", "to": "heat"},
    {"id": "loss-vs-temperature", "model": "thermal.temperature-to-em-properties", "from": "heat", "to": "em"},
    {"id": "expansion", "model": "thermal.expansion", "from": "heat", "to": "solid"},
    {"id": "deformation", "model": "mechanics.deformation-to-geometry", "from": "solid", "to": "em"}],
  "materialLaws": [{"region": "slab", "property": "electromagnetic.lossTangent", "law": "linear",
                    "value": 0.05, "reference": 293.15, "slope": 2e-4, "validity": [273.15, 450]}],
  "scheme": {"kind": "iterative", "maxIterations": 50, "temperatureTolerance": 1e-4}
}
```

(`examples/multiphysics/a-microwave-heated-slab.resonant` holds the full document.)

## Relationships, not a flat list

Couplings name the domains they connect and the physical model that connects
them; laws name the region and property they govern; regions name their
material and, optionally, the scene object they stand for. The engine builds
the execution graph from these relationships.

## Identity and persistence

Every element has a stable id chosen when it is created; materials and scene
objects are referenced by id, so renaming a material, an object or the
system keeps every reference (`MultiphysicsServices.SystemsAreStoredUndoablyAndSurviveSaveLoadAndRenames`).
Storing, replacing and deleting a system are undoable steps. The document has
a version; readers refuse versions they do not know.

## Geometry state

`GeometryState` keeps the reference dimensions (as designed, at the
reference temperature) and the current ones (deformed), with a revision that
increments on every change. A participant records the revisions it solved
with; the result marks a participant whose solution predates the final
revision (`stale`) and says by how much the scheme allowed it to differ.

## Validation

References resolve (materials, domains, regions), ids are unique, every
domain has a participant, coupling models exist and are implemented
(planned and external models are refused with their reason), the models'
domain kinds match the coupled domains, transfer methods fit their
quantities, the scheme is valid and a one-way scheme has no feedback.
`MultiphysicsService.ValidatePhysicalSystem` reports every issue with its field.
