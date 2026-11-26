# ADR-000205: Layered architecture — Qt-free core, separate rendering, physics domains as modules

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/product.md` RES-PRODUCT-001, RES-PRODUCT-006, RES-PRODUCT-008, RES-PRODUCT-011, RES-PRODUCT-013
**Request:** REQ-CHAT-20261126-001

## Context

The vision requires several wave domains on one architecture, correct
physics before everything else, reproducible results, and visualization
that shows calculated data only. Those properties are lost when physics,
rendering and user interface are mixed.

## Decision

1. **Three layers, dependencies pointing down only.**

   ```text
   src/app     Qt Widgets application: windows, panels (.ui), controllers
      |
   src/render  OpenGL 4.5 rendering components (Qt Gui / OpenGL only)
      |
   src/core    engineering data model and physics: no Qt, no OpenGL
   ```

   `src/core` is standard C++20 plus Eigen and nlohmann-json. It never
   includes a Qt or OpenGL header. `src/render` never includes `src/app`.
   The rules are checkable (`#include` patterns) and are listed in
   `docs/audits/current-state.md`.
2. **Core modules.** `foundation` (ids, units, logging, results), `scene`
   (scene graph), `geometry` (engineering geometry and tessellation),
   `materials`, `commands` (undo/redo), `project` (persistence),
   `simulation` (configuration, jobs, results). Physics domains are added
   beside them as modules of their own (`acoustics`, later `mechanics`,
   `electromagnetics`), each with its own equations and solvers behind
   interfaces the simulation module defines. No universal solver, and no
   universal material structure: each domain has its own property set.
3. **Engineering geometry is not a render mesh.** A scene object refers to
   a parametric geometry definition. A triangle mesh is derived from it for
   display and is never the source of truth; a simulation derives its own
   representation.
4. **Units and coordinates.** SI units internally everywhere. World axes:
   X horizontal, Y depth, Z vertical (right-handed, Z up). Angles are
   radians internally; rotations are stored as intrinsic Z-Y-X Euler angles
   (yaw about Z, then pitch about Y, then roll about X). Conversions to
   display units happen only in the user interface.
5. **Calculated data only.** A renderer draws what a result object holds.
   Each result carries the solver, model and assumptions that produced it,
   and the user interface shows them. An approximation is labeled as one.
6. **Long calculations run off the user-interface thread** as jobs with
   states, progress and cancellation; the core's job system uses standard
   threads, the application observes it.
7. **CPU reference first.** No GPU compute dependency before a validated
   CPU implementation exists (RES-PRODUCT-014).

## Consequences

* The core is testable without a display or an OpenGL context.
* The application needs adapter code between core objects and Qt models.
* A new physics domain is a new module and new property sets, not a change
  to existing ones.
