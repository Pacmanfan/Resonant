# Milestone 27 Gap Analysis and Phase Plan

**Status:** the baseline audit of REQ-CHAT-20261128-001 (RES-REL-001, ADR-000229), 2026-11-28.

**Sources reviewed:**
- AGENTS.md, ADR-000227 and ADR-000228, WORK-000024 and -025, VER-000024 and -025;
- `IMPLEMENTATION_STATUS.md` (every Missing and Partial row);
- the demo system (`demos/`, `src/core/demo`, `docs/demos/`), the automation
  framework (ADR-000215, -216) and the release tooling (ADR-000218,
  `docs/release/`).

**Status classes** (as the milestone requires, for every capability):

| Class | Meaning |
|---|---|
| V | implemented and verified |
| L | implemented with limitations |
| P | architecturally prepared, not implemented |
| X | dependent on unavailable external software |
| N | not supported |

## 1. Scientific capabilities

| Capability | Now | Gap | Phase |
|---|---|---|---|
| Harmonic acoustics, arrays, reflectors (Kirchhoff) | V | objects do not scatter the harmonic field; waveforms other than sinusoids; lens fields need full wave | - |
| Acoustic FDTD | L | boundaries, materials and objects tested in 2D only; no elastic solids; no eigenmodes | -004 (3D validation where structures need it) |
| Structural mechanics | L | hexahedra too stiff in bending; no shells, layered or composite bodies; structured meshes only | -004: addressed by 20-node layered solids on curved surfaces with explicit interfaces (ADR-000232); scene meshes stay structured |
| Lamination theory, coated beam and plate modes | V | not a shell solution (ADR-000228) | -004 keeps it as the reference |
| Piezoelectric conversion | L | lumped first-mode cantilever only; the engine's piezoelectric coupling is planned | -005: a 3D piezoelectric continuum (electrodes, terminals, static, modal, harmonic) with the lumped model as reference (ADR-000233); the engine's participant still planned |
| Thermoelectric, magnetostrictive, photovoltaic | N | listed as mechanisms, not modeled | -005: standalone models (a 1D couple with Thomson heat; a linear rod and coil; a single-diode cell); not yet analyses of architectures |
| Electromagnetics (analytical, Yee, layered) | V/L | patch, horn, helix are estimates only | - |
| Optical response | N | the electromagnetic domain is RF; no optical materials | -005 (the photovoltaic input only) |

## 2. Multiphysics integration

| Item | Now | Gap | Phase |
|---|---|---|---|
| Coupled engine | L | five participants (layered EM, finite-volume thermal, solid-column mechanics, cavity mode, lumped thermal); two geometries (layered slab, cavity) | -004, -005 |
| Planned couplings (acoustic and structure, Lorentz, radiation pressure, magnetic loss, piezoelectric, acoustic heating, fluid) | P | registered, refused by validation | -005 (piezoelectric, the acoustic load on a structure) |
| Energy accounting across domains | V/L | no momentum for the participants; no full-wave ledgers; no ledgers for the missing conversions | -005 |
| A GUI for physical systems and ledgers | P | planned (`COUPLED_ENGINE_ROADMAP.md`) | -008 |

## 3. Material representation

| Item | Now | Gap | Phase |
|---|---|---|---|
| Library properties | V | scalar per key; frequency tables; no tensors, no temperature laws in the library (the engine has laws) | -006 |
| Architecture regions, anisotropic sets, orientation | V | anisotropy used by lamination and through-thickness conduction only | -006 |
| Property fields | L | one axis; scalar; idealized or realizable; no imported or measured fields; no coordinate systems or tensor fields | -006: tensor material fields with frames, bases, analytic or sampled forms and sources (ADR-000234); read by the 3D solids |
| Microstructure | L | voxel RVEs with grains and phases (multiscale); descriptive records on regions | -006: boundary, porosity, inclusion and defect statistics; a grain-boundary resistance in the homogenized conductivity |

## 4. Interfaces and boundary conditions

| Item | Now | Gap | Phase |
|---|---|---|---|
| Interface records with models and provenance | V | - | - |
| Contact resistance (thermal, electrical) in the solvers | V | - | - |
| Finite normal and tangential stiffness, slip, friction, partial debonding, failure criteria | P | recorded and refused | -004 |
| Acoustic boundaries in harmonic solvers | N | objects do not affect the field | - (out of the milestone's scope) |

## 5. Atomistic and multiscale integration

| Item | Now | Gap | Phase |
|---|---|---|---|
| Representations, derivations, lineage, voxel RVE homogenization | V | elastic RVE homogenization missing | -006 |
| Built-in Lennard-Jones reference calculations | V | - | - |
| External engines (LAMMPS, GROMACS, Quantum ESPRESSO ...) | X | none installed; detection only | -006: adapters with applicability (in-house static lattice runs; LAMMPS decks and parser, run only when found; GROMACS applicability only) |
| The atomistic-to-continuum data bridge (conditions, statistics, units, frames) | P | partly in `multiscale::Derivation` | -006: transferred-property records with orders, frames, conditions, uncertainty and provenance, as computed material fields |

## 6. GUI and visualization

| Item | Now | Gap | Phase |
|---|---|---|---|
| Viewport with fields on planes | V | no isosurfaces or volumes; no 3D property fields or regions; no stress or energy-density overlays | -008 |
| Matter panel | V | read-only (edits through the service and files) | -008 |
| CAD | L | STL and OBJ export of meshes for printability; no STEP; no import | -008 (export) |
| Viewport handles, grouping from the UI | N | known since V0.1 | - |

## 7. Testing and reliability

| Item | Now | Gap | Phase |
|---|---|---|---|
| Jobs/RunningJobIsCancelledPromptly | L | flaky under load; fixed: the race was wait() returning before listeners heard the final state, plus out-of-order notifications (ADR-000231) | -003 |
| C++ gRPC transport | X | `libgrpc++-dev` and `protobuf-compiler-grpc` are not installed (checked 2026-11-28 with dpkg); gRPC is served by the grpcio bridge (ADR-000216) | -003 (needs the owner's system install) |
| GPU comparisons | X | skipped (the GPU's memory) | - |
| Hardware (ALSA, SDR) | L | null device; discovery only | - |

## 8. Packaging and deployment

| Item | Now | Gap | Phase |
|---|---|---|---|
| Linux ZIP, launcher, offline help, licences, clean-container tests | V | REQ-CHAT-20261126-013, ADR-000218 | -010 refreshes it for the new content |
| AGENTS.md Deployment section, the `package` lane | P | placeholder text; a protected artifact | -002 (human review) |
| Distribution baseline | L | glibc 2.38+ only; x86-64 | -010 documents it |
| Resonant's own licence | N | an owner decision | -001 prepared it; decided 2026-11-28: the MIT License (REQ-CHAT-20261128-181) |
| LGPL source publication | L | the notice points to Ubuntu's archive; publishing the sources next to a release is open | -010 |

## 9. Demonstration automation and video production

The demonstration system has:
- Demo Studio: profiles, sequences, presenter notes, presenter mode, narration
  pauses;
- unattended runs (`--run-demo`) with run manifests (versions, the project's
  SHA-256, every step's outcome and time);
- screenshots with annotated copies;
- eight demonstrations (A–H), run from a clean state by the tests and in clean
  containers.

There is no in-application video recording: by design, recording is external
(`VIDEO_PRODUCTION_GUIDE.md`). No demonstration was added after
REQ-CHAT-20261126-010, so 21 of 38 documented features have none. The flagship
scenarios are assessed in `docs/demos/DEMONSTRATION_READINESS.md`.

## Phase plan

| Phase | Request | Parts | Depends on |
|---|---|---|---|
| 1 | REQ-CHAT-20261128-001 | I, X, the audit of XI | - |
| 1b | REQ-CHAT-20261128-002 | IX.1 (process change) | the owner's review |
| 2 | REQ-CHAT-20261128-003 | VIII.1, VIII.2 | the owner's gRPC install |
| 3 | REQ-CHAT-20261128-004 | III.1, III.2 | - |
| 4 | REQ-CHAT-20261128-005 | II | 3 (structures for piezoelectric plates) |
| 5 | REQ-CHAT-20261128-006 | III.3, IV | - |
| 6 | REQ-CHAT-20261128-007 | V | 3–5 |
| 7 | REQ-CHAT-20261128-008 | VI | 3, 5 |
| 8 | REQ-CHAT-20261128-009 | VII | 4 |
| 9 | REQ-CHAT-20261128-010 | IX.2, IX.3 | 1b |
| 10 | REQ-CHAT-20261128-011 | VIII.3, XI, XII | all |

Part VIII.3 (test coverage) applies to every phase. Its last request audits the
coverage across the milestone.

## Stale status rows corrected

The following rows of `IMPLEMENTATION_STATUS.md` were out of date and are
corrected by this request:
- the Shape–Function–Energy rows on resonance, unit cells and multiscale
  adapters (delivered by REQ-CHAT-20261126-018, -019, -020);
- the binary-distribution row (REQ-CHAT-20261126-013).
