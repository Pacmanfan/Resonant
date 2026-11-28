# Architected Matter Roadmap

**Status:** the phased plan after REQ-CHAT-20261127-001 (ADR-000228). The
request's Phases 0–7 are delivered with the limits in `ARCHITECTED_MATTER.md`.
Phase 8 has extension points only.

| Phase | Delivered | Remaining |
|---|---|---|
| 0 Audit | `ARCHITECTED_MATTER_AUDIT.md` | - |
| 1 Canonical model | regions, compositions, phases, microstructure, orientation, anisotropy, stacks, fields, provenance, persistence | fields in two and three dimensions; tensors in the library itself |
| 2 Interfaces and architecture | interface models, adjacency, patterns, validation, geometry association | interface models in the mechanical solvers (finite stiffness, cohesive growth, friction) |
| 3 Multiscale | effective estimators with bounds; links to representations | elastic RVE homogenization; mean-field (Mori–Tanaka) estimates |
| 4 Coupled engine | stacks as physical systems with laws and contact resistances | patterned and 3D regions; mechanical participants from architectures; piezoelectric coupling in the engine |
| 5 Inverse design | the `matter` adapter: thickness, angle, coverage, material choice, interface parameters, manufacturing | layer ordering and topology; discrete region placement |
| 6 Design environment | the Matter panel: regions, stack view, analyses, manufacturability, exploration, comparison | editing in the panel (today through the service and files); 3D property-field rendering |
| 7 Experiments and manufacturing | laboratory links and the stack-metric model; process checks; records | measured process windows; automatic updates from inspection data |
| 8 Advanced discovery | the mechanism registry; exploration as evidence for assistants | atomistic backends (none installed); microstructure generation; thermoelectric, magnetostrictive and photovoltaic models; reconfigurable matter |

The next steps that would pay off most:

1. **A shell finite-element model of laminated structures.** It would allow the
   bell and other curved converters to be analyzed.
2. **A coupled piezoelectric participant in the engine**, validated against the
   lumped model.
3. **Mechanical interface models in lamination theory.** Partial composite
   action with finite-stiffness bonds is the first.
