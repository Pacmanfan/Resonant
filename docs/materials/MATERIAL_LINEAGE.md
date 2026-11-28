# Material Lineage

**Status:** current (REQ-CHAT-20261126-020). **Requirements:** RES-MSC-002, RES-MSC-003, RES-MSC-004, RES-MSC-011.

## Provenance

| Provenance | Meaning |
|---|---|
| measured, manufacturer, literature, theoretical, illustrative | as before |
| homogenized | from an organized structure or a microstructure (ADR-000223, the RVE methods) |
| computed | from a lower-scale model calculated here (the atomistic methods) |
| database | from a materials database named in the source |
| fitted | fitted or calibrated to data named in the source |
| surrogate | predicted by a surrogate model named in the source |

Contract values 6–10 (`MATERIAL_PROVENANCE_*`). A property value may also
carry `derivation` (the id of its derivation record) and `uncertainty` (a
standard uncertainty in the property's unit); both are omitted when absent,
so older files read and write unchanged.

## The derivation record

`multiscale::Derivation`, in the project's multiscale model: id; method,
model card, solver and version; the source representation and its revision;
source and target scales; the material it was assigned to; every property
(value, unit, component, uncertainty); inputs; boundary conditions;
conditions (temperature, pressure); convergence (size study, residuals,
energy consistency or cutoff sensitivity); resolution; sampling
(realizations, seeds); the constituents' content keys; creation time; the
calculation (result and job) and the parent derivation it replaced.

## Relationships and navigation

`material:M -derived-from-> representation:R`,
`derivation:D -computed-from-> representation:R`,
`material:C -constituent-of-> representation:R`,
`representation:R -describes-> material:M`.
`MultiscaleService.GetMaterialLineage` returns every property of a material
with its provenance, uncertainty, derivation record, source representation
and the chain below it: the representation's constituents, their own
derived properties and representations, recursively.

## Staleness

A derived property is stale, with the reason, when its source
representation was removed or its revision changed, when a constituent was
removed or its content changed, or when the value no longer equals the
derived one (overridden by hand). A derivation result is stale on the same
conditions and cannot be assigned until derived again.

## Model cards

`multiscale::modelCards()`: one per representation kind and per method -
the system represented, scale, assumptions, methods or equations, required
parameters, excluded effects, valid conditions and how to read the results.
