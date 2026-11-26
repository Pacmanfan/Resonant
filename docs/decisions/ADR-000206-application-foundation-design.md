# ADR-000206: Application foundation — object state and components, commands, document controller, project format 1

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/application_foundation.md` RES-APP-001..020
**Request:** REQ-CHAT-20261126-002
**Builds on:** ADR-000204, ADR-000205

## Decisions

1. **One editable state per object.** A scene object has a fixed identity
   (id, type) and one `ObjectState` (name, local transform, visibility,
   geometry, material reference, component). Undo, duplication and
   persistence all work on that state, and one command
   (`SetObjectStateCommand`) covers rename, move, rotate, scale, geometry
   edits, material assignment and visibility.
2. **Domain data are immutable components.** What makes an object an
   emitter, an array or a probe is a `Component` attached to it, registered
   by its domain module in a `ComponentRegistry`. A component is replaced,
   never mutated, so a state snapshot is cheap and safe. The scene module
   knows no physics domain.
3. **Geometry kinds are data.** A shape is a kind id plus named parameters;
   kinds (parameters, validation, tessellation) are entries in a
   `GeometryRegistry`, so a module adds shapes without changing `Geometry`.
4. **Explicit registration, no static-initializer tricks.** Registries are
   filled by named functions called when the registry is created, because
   static libraries drop unreferenced self-registering objects.
5. **Commands on one stack.** Scene and material edits are `Command`
   objects on a `CommandStack` that also tracks the saved state. A failed
   command leaves no trace.
6. **A document controller in the application.** `DocumentController` owns
   the project, the command stack, the selection and the job manager, and
   is the only path from user actions to core commands and the only source
   of change notifications for the panels. Selection is application state,
   not scene state.
7. **The renderer draws draw items, not scene objects.** `SceneRenderBridge`
   derives display meshes from engineering geometry (cached per object) and
   hands the renderer meshes, matrices and colours. Picking is a CPU ray
   cast against those meshes in the core, so it is testable without OpenGL.
8. **Jobs use standard threads in the core.** `JobManager` has worker
   threads, explicit states, progress and cooperative cancellation; the
   application forwards job events to its own thread.
9. **Project format 1.** One JSON document with a format tag, a schema
   version, the application version, the unit conventions, materials, a
   nested scene, simulation configurations and an `extensions` object that
   is preserved untouched. Loading validates everything before returning a
   project; saving writes a temporary file and renames it. Migration is a
   chain of steps keyed by source version; there are none yet. Large
   numerical data will be stored beside the JSON file by a later format
   version, not inside it.
10. **The application is a library plus a small executable.**
    `resonant_app` (windows, panels, controllers) is a static library so
    the tests drive the real user interface; `resonant` is `main.cpp`.
11. **Sources are globbed by both builds** (amends ADR-000204 decision 2:
    there is no source list that could drift, so no consistency check is
    needed). The CMake build is verified by the `cmake` test suite
    (`tools/verify_cmake_build.sh`) in tier t2.
12. **moc is kept away from the standard library headers.** Qt 6.4's moc
    cannot parse the C++20 syntax in libstdc++ 13; both builds stop passing
    it the compiler's own include directories / predefined macros.
13. **Display units are an application concern.** `DisplayUnits` is the one
    place where displayed numbers become SI values and back.

## Deviations from the request

* The request names CMake only; qmake is built as well, on the operator's
  instruction (ADR-000204).
* The request refers to "geometry/materials" and "physics architecture"
  planning documents. None existed; `docs/architecture/overview.md` and
  ADR-000205 are the approved architecture this milestone implements.
* Move, rotate and scale are done numerically in the Properties panel.
  There are no manipulator handles in the viewport yet.
* Geometry import, groups created from a selection, drag-and-drop in the
  scene tree and viewport gizmos are not part of this milestone.
