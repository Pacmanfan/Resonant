# Demonstration Readiness

**Status:** the audit of REQ-CHAT-20261128-001 (RES-REL-004, ADR-000229), 2026-11-28.
The flagship scenarios are defined here with what each needs. They are built by
REQ-CHAT-20261128-011 once the physics they depend on exists, and never before.

## What the demonstration system provides

| Need | Status | Where |
|---|---|---|
| Reproducible setup | yes | profiles and sequences start from a clean state; the run manifest records the project's SHA-256 and the versions |
| Deterministic configuration | yes | solver settings recorded; seeded randomness; the tests run every demonstration |
| Controlled excitation | yes for the existing domains | sequence steps set sources, drives and arrays through application commands |
| Observable quantities | yes | `run` steps record results; explain steps read them |
| Synchronized visualization | partly | fixed cameras and layouts; no time-synchronized playback of a transient with plots |
| Simulation against measurement | partly | demonstration G (labelled synthetic data); the laboratory (REQ-CHAT-20261126-023) is not yet used by a demonstration |
| Energy accounting | not in a demonstration yet | the accounting panel exists (REQ-CHAT-20261126-022) |
| Exported numbers | partly | panels export CSV; a run manifest does not yet collect the exported files of a run |
| Screenshots | yes | `capture` steps with annotated copies |
| Interaction traces | partly | the manifest records every step with its time and outcome; no input-event trace |
| Video recording | no, by design | external recorders at a fixed window size (`VIDEO_PRODUCTION_GUIDE.md`); no frame capture at a fixed rate |
| Application output distinguished from post-production | yes in the manifest | everything in the manifest is application output; graphics added later are not in it |

To support 60–90 second flagship videos, the system needs:
- a fixed-rate frame capture of the window, so videos can be assembled
  reproducibly, each frame tied to the manifest;
- a timeline that plays transients with their plots in step;
- a run folder that gathers every export of a run;
- a caption overlay for each step that says what is computed, idealized or
  synthetic.

These belong to REQ-CHAT-20261128-011.

## The flagship scenarios

### 1. Electric bell

*Bell excitation → acoustic field → Helmholtz resonance → piezoelectric cantilever → electrical output.*

| Stage | Exists | Needed |
|---|---|---|
| A bell's modes, with its geometry | yes since REQ-CHAT-20261128-004: a layered solid of revolution on the bell's meridian (ADR-000232) | the bell's profile and material (Prompt 30) |
| The sound the bell radiates | for baffled plates (Rayleigh, one-way) | radiation from a curved vibrating body (REQ-CHAT-20261128-004, -005) |
| Helmholtz resonance | yes: lumped resonators (REQ-CHAT-20261126-018) | the resonator driven by the computed field |
| Pressure loading the cantilever | a plane wave's pressure on a piezoelectric plate in a duct (REQ-CHAT-20261128-005); the full-wave load on a structure is still planned | a resonator-coupled load path (Prompt 30) |
| Piezoelectric conversion | the lumped cantilever and the 3D piezoelectric continuum (REQ-CHAT-20261128-005, ADR-000233) | - |
| An energy ledger across the chain | yes: conversion chains with every stage and join checked (REQ-CHAT-20261128-005) | the bell's own stages (Prompt 30) |

**Not ready.** Every stage after the excitation needs physics from phases 3 and 4.

### 2. Energy-focusing metamaterial

*Geometry-driven wave interaction, focusing and energy redistribution.*

| Stage | Exists | Needed |
|---|---|---|
| A structure that focuses waves | phased arrays and reflectors (A–C); 1D metamaterial analyses; acoustic FDTD with objects (validated in 2D) | a periodic or graded structure in the full-wave solver, validated where it is shown (REQ-CHAT-20261128-004 or -006) |
| The field and its focus | yes: FDTD planes and probes | 3D views (REQ-CHAT-20261128-008) |
| Energy redistribution | no ledger for full-wave fields | an accounting adapter for the full-wave solvers (REQ-CHAT-20261128-005) |

**Partly ready.** A 2D full-wave focusing scenario can be built honestly today,
labelled 2D. The energy redistribution needs a full-wave ledger.

### 3. Resonant pendulum

*Coupled dynamic behaviour, resonance, energy transfer, dissipation.*

| Stage | Exists | Needed |
|---|---|---|
| Coupled oscillators with transient integration and element energy flows | yes: lumped networks (REQ-CHAT-20261126-018, -022), coupled resonators and torsional flywheel examples | a pendulum element (the gravity restoring term, nonlinear in the angle) or a stated small-angle linearization |
| Resonance and energy transfer | yes: driven response, beating between coupled modes, ledgers with element flows | - |
| Dissipation | yes: dampers in the ledger | - |
| Synchronized motion and plots | no | animation of the scene objects from a transient, and a timeline (REQ-CHAT-20261128-008, -011) |

**Nearest to ready.** The physics exists, with a small-angle linearization or a
pendulum element to add. The visualization of the motion is missing.

## Rules the scenarios keep

- **Computed and measured:** every number shown is computed in the run. A
  measurement appears only from a measured file or a live acquisition, never
  synthesized to look like one. Synthetic data is labelled synthetic on screen
  and in the manifest.
- **Model limits:** each scenario's profile states the models' limits: 2D,
  lumped, small angle, idealized interfaces.
- **No missing physics:** a stage that has no model is not shown as if it had
  one.
