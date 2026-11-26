# Request: # PROMPT 6 — RESONANT: V0.1 APPLICATION FOUNDATION IMPLEMENTATION

You are imple

**Request ID:** REQ-CHAT-20261126-002
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:11:33-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 6 — RESONANT: V0.1 APPLICATION FOUNDATION IMPLEMENTATION

You are implementing **RESONANT — Wave Field Engineering Studio**, a serious scientific and engineering application for modeling, simulating, visualizing, and eventually manipulating physical wave phenomena.

This is the first substantive implementation milestone.

The architecture and project requirements should already have been documented in the preceding planning phase. Before writing code, inspect those documents and the existing repository. Treat the approved architecture and recorded decisions as authoritative unless you identify a concrete technical problem.

## 1. Mission

Build a functional, extensible desktop application foundation using:

- C++20
- Qt 6
- CMake
- OpenGL 4.5 Core
- Eigen where needed
- GoogleTest for C++ tests

Python integration, advanced numerical solvers, and hardware control are not part of this milestone.

The objective is to establish a robust application that can host the future physics, geometry, simulation, and experimental subsystems.

Do not build a mockup. The application must compile, launch, interact correctly, save projects, and pass its tests.

## 2. Initial repository and environment audit

Before modifying anything:

1. Inspect the complete repository structure.
2. Read the project vision, architecture, geometry/materials, physics architecture, roadmap, and decisions documents.
3. Inspect the installed compiler, CMake, Qt 6, OpenGL drivers, and available dependencies.
4. Identify the target operating system and build environment.
5. Review existing code and determine whether any implementation already exists.
6. Record dependency versions and licensing considerations.

Produce a short implementation plan and identify any architectural conflicts before proceeding.

Do not silently change previously approved architectural decisions.

## 3. Application structure

Implement the foundational application using clear subsystem boundaries.

The application should have distinct modules for:

- Application lifecycle and configuration.
- Main window and user interface.
- Scene management.
- Geometry.
- Materials.
- Rendering.
- Project persistence.
- Simulation configuration and job management.
- Logging and diagnostics.
- Testing.

Use the architecture's approved directory structure. Avoid monolithic classes, global mutable state, and unnecessary coupling.

Keep the core scene and data model independent of Qt wherever practical. Qt should primarily provide the desktop application and user interface.

## 4. Main application window

Create a polished, functional engineering workspace.

The initial layout should include:

**Main viewport**
- Central interactive 3D viewport.
- Perspective camera.
- Grid and coordinate axes.
- Camera orbit, pan, and zoom.
- Selection of scene objects.
- Visual feedback for selected objects.

**Scene hierarchy**
- Tree view of scene objects.
- Object selection.
- Visibility toggles.
- Expandable groups.
- Rename support.
- Context menu for adding and removing objects.

**Properties panel**
- Selected object name and type.
- Position.
- Rotation.
- Scale.
- Geometry parameters where applicable.
- Material assignment.
- Visibility.

**Bottom analysis area**
- Dockable panel.
- Initial tabs for Console, Diagnostics, and Simulation Status.
- Prepare the interface for future plots, field analysis, and measurement results.

**Menus and toolbars**
- File.
- Edit.
- View.
- Scene.
- Geometry.
- Materials.
- Simulation.
- Tools.
- Help.

Implement working actions where applicable. Do not create decorative buttons that do nothing.

Use Qt dock widgets so the user can rearrange the workspace. Persist workspace layout between sessions.

## 5. OpenGL rendering foundation

Implement a real OpenGL 4.5 Core rendering subsystem.

Requirements:

- Use modern OpenGL.
- No fixed-function pipeline.
- No immediate-mode rendering.
- Keep rendering independent from scene data.
- Implement a clear renderer initialization and shutdown lifecycle.
- Handle viewport resizing.
- Support depth testing, camera matrices, and shader management.
- Include a basic lighting model.
- Render a ground grid and coordinate axes.
- Render basic geometry.
- Support object selection highlighting.
- Support camera navigation.

Create reusable rendering components rather than placing all rendering code inside the viewport widget.

The rendering system must eventually support:
- Large simulation datasets.
- Volumetric field visualization.
- Animated wave propagation.
- Isosurfaces.
- Cross sections.
- Scalar and vector field rendering.

Do not implement these advanced features yet. Ensure the rendering architecture can accommodate them.

If OpenGL 4.5 is unavailable on the target environment, report the limitation and propose a documented fallback rather than silently changing the requirement.

## 6. Scene and geometry foundation

Implement a basic scene graph.

Every scene object must have:

- Stable unique identifier.
- Display name.
- Object type.
- Local transform.
- World transform.
- Visibility state.
- Parent/child relationship where appropriate.
- Optional geometry reference.
- Optional material reference.

Implement the initial geometry primitives:

- Box.
- Sphere.
- Cylinder.
- Cone.
- Plane.
- Disc.
- Ellipsoid.

Add parametric geometry support where practical.

Each primitive must have editable parameters and a functioning viewport representation.

Implement object operations:

- Create.
- Select.
- Rename.
- Move.
- Rotate.
- Scale.
- Duplicate.
- Delete.
- Hide/show.

Implement basic undo/redo using a deliberate command or transaction architecture.

Do not conflate the rendered mesh with the underlying engineering geometry. Keep that distinction explicit in the data model.

## 7. Coordinate system and units

Use the established world coordinate convention:

- X: horizontal.
- Y: depth.
- Z: vertical.

Use SI units internally.

The user interface may display convenient units, but conversions must occur at well-defined boundaries.

Establish consistent conventions for:
- Position.
- Rotation.
- Scale.
- Angles.
- Frequency.
- Time.
- Length.
- Material properties.

Avoid implicit unit conversions and ambiguous angle conventions.

## 8. Materials foundation

Implement the material data model and an initial material browser/editor.

Materials must support:

- Stable ID.
- Name.
- Description.
- Physics domain applicability.
- Domain-specific property sets.
- Source/provenance metadata.
- Validity ranges.
- Optional frequency-dependent properties.

Create initial example materials for:
- Air.
- Water.
- Aluminum.
- Steel.
- Copper.
- Glass.
- Acrylic.
- Concrete.
- Rubber.
- Wood.

Do not fabricate authoritative physical constants.

If illustrative values are needed for early demonstrations, clearly mark them as illustrative and identify their limitations.

The material architecture must support separate acoustic, mechanical, and electromagnetic property definitions without forcing unrelated properties into a universal structure.

## 9. Project persistence

Implement native Resonant project save/load.

The project must preserve:

- Scene hierarchy.
- Object IDs.
- Geometry definitions and parameters.
- Object transforms.
- Material definitions and references.
- Visibility.
- Workspace-independent scene configuration.
- Application/project version.
- Unit conventions.
- Future-compatible metadata.

Use JSON for project metadata and configuration, with a strategy for external or binary assets as needed.

Requirements:

- Save project.
- Save As.
- Open project.
- New project.
- Detect unsaved changes.
- Prompt before discarding modifications.
- Handle missing or invalid files gracefully.
- Validate project data before loading.
- Preserve stable IDs.
- Support schema/version migration.

Do not serialize transient OpenGL objects or GUI widget state into the scientific scene model.

## 10. Simulation foundation

Do not implement a real physics solver in this milestone.

However, establish the interfaces needed for future simulation.

Implement:

- Simulation configuration data structure.
- Solver/domain identifiers.
- Simulation job abstraction.
- Job states: queued, running, completed, failed, cancelled.
- Progress reporting.
- Cancellation interface.
- Result metadata.
- Logging and error reporting.

The application must remain responsive during future long-running simulations. Design the job system accordingly, even if the initial implementation only includes a simple demonstrator job.

Do not create fake scientific results or represent a placeholder calculation as a physical simulation.

## 11. Logging and diagnostics

Implement structured application logging.

Capture:
- Startup and shutdown.
- Dependency and graphics initialization.
- Project loading and saving.
- Scene modifications where useful.
- Simulation job lifecycle.
- Errors and exceptions.

Provide a visible diagnostics/console panel.

Errors should be informative and actionable.

Avoid silently swallowing exceptions or returning success after failed operations.

## 12. Testing

Use GoogleTest.

At minimum, test:

- Scene object creation.
- Stable IDs.
- Transform calculations.
- Parent/child relationships.
- Geometry parameter validation.
- Material creation and validation.
- Project serialization and deserialization.
- Invalid project handling.
- Version compatibility behavior.
- Simulation job state transitions.
- Undo/redo behavior.

Separate tests for core data models from GUI and rendering tests.

The core data model should be testable without launching the GUI or requiring an OpenGL context.

## 13. User experience and quality

The application should look and behave like a professional engineering tool.

Avoid:
- Generic placeholder interfaces.
- Unnecessary decorative elements.
- Oversized controls.
- Excessive dialogs.
- Hardcoded demonstration scenes presented as real projects.
- Features that appear functional but are not implemented.

Use consistent terminology, sensible defaults, keyboard shortcuts for common operations, and clear feedback for user actions.

The interface should be designed for technical work involving complex scenes, many emitters, geometry, materials, and eventually large simulation datasets.

## 14. Build and developer experience

Provide:

- A clean CMake configuration.
- Clear separation of application and core libraries.
- Reproducible dependency configuration.
- Debug and Release build support.
- A documented build procedure.
- A documented test procedure.
- Useful compiler warnings.
- No unnecessary compiler-warning suppression.

Do not introduce a dependency merely because it is convenient. Document why each dependency is needed and its licensing implications.

Do not make the project dependent on a particular developer's machine paths.

## 15. Documentation

Create or update:

- README.md
- BUILDING.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md
- Any necessary module-level documentation.

Document the implemented architecture and any deviations from the approved design.

IMPLEMENTATION_STATUS.md must distinguish clearly between:
- Implemented and tested.
- Implemented but not fully tested.
- Partially implemented.
- Not implemented.
- Deferred intentionally.

## 16. Implementation sequence

Work incrementally in this order:

**Phase A — Build foundation**
- CMake project.
- Core library.
- Application executable.
- Dependency setup.
- Basic test infrastructure.

**Phase B — Application shell**
- Main window.
- Dockable panels.
- Menus and actions.
- Workspace persistence.

**Phase C — Rendering**
- OpenGL initialization.
- Shader management.
- Camera.
- Grid and axes.
- Basic geometry rendering.

**Phase D — Scene model**
- Scene graph.
- Stable object IDs.
- Transforms.
- Geometry primitives.
- Selection and object operations.
- Undo/redo.

**Phase E — Materials**
- Material data structures.
- Initial library.
- Material editor.
- Assignment to scene objects.

**Phase F — Persistence**
- Project serialization.
- Save/load.
- Validation.
- Version handling.

**Phase G — Simulation infrastructure**
- Job abstraction.
- State transitions.
- Progress and cancellation.
- Diagnostics.

**Phase H — Verification**
- Full test suite.
- Clean build.
- Manual application testing.
- Documentation.
- Final implementation report.

Do not attempt to implement every phase in one enormous source file or one unreviewable change.

## 17. Acceptance criteria

This milestone is complete only when all of the following work:

1. The application builds from a clean build directory.
2. The application launches successfully.
3. The main workspace is functional.
4. The user can create a new project.
5. The user can create and manipulate primitive geometry.
6. Scene hierarchy and properties remain synchronized.
7. Geometry renders correctly in the 3D viewport.
8. The user can create and assign materials.
9. Project save/load preserves the scene accurately.
10. Unsaved changes are handled correctly.
11. Undo/redo works for supported scene operations.
12. Diagnostics report meaningful application information.
13. The simulation job infrastructure can execute, report, and cancel a demonstrator job without pretending to perform real physics.
14. Automated tests pass.
15. Documentation accurately reflects the implementation.

## 18. Final report

When finished, provide a concise but substantive report containing:

- What was implemented.
- Repository structure and major modules.
- Important architectural decisions.
- Dependencies and versions.
- Build instructions.
- Test results.
- Manual verification results.
- Known limitations.
- Deferred features.
- Any deviations from the approved architecture.
- Recommended next implementation milestone.

Include actual command output or test evidence where practical.

Do not claim something works merely because its code was written.

## 19. Critical development rules

- Do not implement physics solvers prematurely.
- Do not compromise the modular architecture for short-term convenience.
- Do not mix rendering code with scientific calculations.
- Do not make Qt a mandatory dependency of the core data model.
- Do not introduce fake scientific results.
- Do not silently substitute placeholder implementations for requested functionality.
- Do not add hardware output in this milestone.
- Do not build unnecessary abstractions without a clear future use.
- Keep code readable, testable, and maintainable.
- Preserve existing approved documentation and decisions.
- Record significant architectural decisions in DECISIONS.md.

**The goal is a real, working engineering application foundation—not a prototype screenshot.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: BUILD (confidence high)
  - validation: PASS
  - classifier_reason: override FEATURE accepted (text alone suggests BUILD: build/compile/link failure in the request statement)
  - evidence: 'implementing' [head]; 'milestone' [body]; 'Build a ' [head]; 'integration' [head]; 'milestone' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Second ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Builds on REQ-CHAT-20261126-001 (project vision). Open conflict to resolve before processing: this ticket specifies CMake and GoogleTest, while the repository was bootstrapped earlier in the same session with build.system=qmake and the user chose Qt Test; it also refers to planning documents (architecture, geometry/materials, physics architecture, roadmap, decisions) that do not exist 

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Second ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Builds on REQ-CHAT-20261126-001 (project vision). Open conflict to resolve before processing: this ticket specifies CMake and GoogleTest, while the repository was bootstrapped earlier in the same session with build.system=qmake and the user chose Qt Test; it also refers to planning documents (architecture, geometry/materials, physics architecture, roadmap, decisions) that do not exist in the repository yet.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000002
- Evidence: pending
