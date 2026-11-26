# Application Foundation (V0.1)

**Owner of:** `RES-APP-001..020`
**Source:** REQ-CHAT-20261126-002 ("Prompt 6 — V0.1 application foundation")
**Refines:** `product.md` RES-PRODUCT-002, -007, -012, -013
**Decisions:** ADR-000204, ADR-000205, ADR-000206
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

The foundation is a working desktop application that hosts the later
physics, simulation and experiment subsystems. It contains no physics
solver.

### RES-APP-001 — Builds, launches and is modular

Resonant builds from a clean tree with CMake and with qmake (ADR-000204),
in Debug and Release, with compiler warnings enabled and none suppressed for
Resonant's own code, and launches. Subsystems are separate modules: core
(foundation, scene, geometry, materials, commands, project, simulation),
render, app. The core data model does not depend on Qt or OpenGL.
*Verification:* suites `core`, `app`, `cmake` (`tools/verify_cmake_build.sh`), `layering` (`tools/check_layering.py`).

### RES-APP-002 — Workspace

The main window has a central 3D viewport; dockable Scene, Properties and
Analysis panels that the user can rearrange, float and hide; File, Edit,
View, Scene, Geometry, Materials, Simulation, Tools and Help menus; and
toolbars. Every action does something: there are no decorative controls.
The layout, view options and display unit persist between sessions and can
be reset. User interfaces are Qt Designer forms.
*Verification:* `MainWindowTest.EveryMenuActionExistsAndTheThreeDocksAreThere`,
`NoActionIsADecoration`, `WorkspaceLayoutPersistsBetweenSessions`.

### RES-APP-003 — Viewport

The viewport renders with OpenGL 4.5 Core (no fixed function, no immediate
mode): a perspective camera with orbit, pan and zoom; a ground grid and
coordinate axes; lit geometry; a highlight on selected objects. Clicking an
object selects it. When OpenGL 4.5 Core is unavailable the application says
so and keeps working without the viewport; it never falls back silently.
*Verification:* `RenderTest.*`, `Camera.*`,
`MainWindowTest.ClickingInTheViewportSelectsTheObjectUnderTheCursor`,
`ViewportShowsGeometryAndHighlightsTheSelection`, `CameraNavigationAndViewActions`.

### RES-APP-004 — Rendering is a separate subsystem

Rendering is built from reusable components (shader library, GPU meshes,
layers) with an explicit initialize / render / shutdown lifecycle, handles
resizing, and knows nothing about scene objects: the application converts
the scene into draw items. New visualizations are added as layers.
*Verification:* `RenderTest.ShutdownAndReinitialize`, `ResizeChangesTheViewport`,
`MeshCacheDropsMeshesThatAreNoLongerDrawn`; suite `layering` (`tools/check_layering.py`).

### RES-APP-005 — Scene graph

Every scene object has a stable unique id, a name, a type, a local
transform, a world transform derived from its ancestors, a visibility
state inherited by descendants, a parent and children, and optionally an
engineering geometry, a material reference and a domain component.
*Verification:* `Scene.*`.

### RES-APP-006 — Geometry primitives

Box, sphere, cylinder, cone, plane, disc and ellipsoid exist as parametric
engineering geometry with editable, validated parameters in SI units. A
display mesh is derived from the geometry and is never the definition of
the shape.
*Verification:* `Geometry.*`,
`MainWindowTest.CreatingEachPrimitiveUpdatesSceneTreePropertiesAndSelection`,
`GeometryParametersAreEditableAndValidated`.

### RES-APP-007 — Object operations

Create, select, rename, move, rotate, scale, duplicate, delete, hide/show,
and move between groups. The scene tree, the properties panel and the
viewport always show the same state.
*Verification:* `MainWindowTest.RenameHideDuplicateDeleteAndUndoRedo`,
`GroupsNestObjectsAndDeletingAGroupDeletesItsChildren`,
`PropertiesPanelEditsTransformInDisplayUnitsAndStoresSi`.

### RES-APP-008 — Undo and redo

Every scene and material edit is an undoable command on one stack; a
failed command changes nothing and is not recorded; the stack knows whether
the project differs from its last saved state.
*Verification:* `CommandStack.*`.

### RES-APP-009 — Coordinates and units

World axes: X horizontal, Y depth, Z vertical; right-handed. SI units
internally. Rotations are intrinsic Z-Y-X Euler angles in radians. The user
interface may show other units (length in m, cm, mm or in; angles in
degrees); conversion happens only in the application's display-unit
boundary, never implicitly in the core.
*Verification:* `Units.*`, `Transform.*`,
`MainWindowTest.PropertiesPanelEditsTransformInDisplayUnitsAndStoresSi`.

### RES-APP-010 — Material model

A material has a stable id, name, description, and one property set per
physics domain it applies to (acoustic, mechanical, electromagnetic; each
domain defines its own properties). Each value records its provenance
(measured, manufacturer-supplied, literature-derived, theoretical,
illustrative), a source note, an optional validity range and optional
frequency-dependent values. Unphysical values are refused.
*Verification:* `Material.*`, `MaterialLibrary.*`.

### RES-APP-011 — Example materials are not authoritative

The initial library has air, water, aluminum, steel, copper, glass, acrylic,
concrete, rubber and wood. Every value in it is marked illustrative and
says it is not authoritative.
*Verification:* `MaterialLibrary.ExampleLibraryHasTheTenMaterialsMarkedIllustrative`.

### RES-APP-012 — Material browser and assignment

The user can browse, create, duplicate, edit and delete materials and
assign a material to objects. Deleting a material clears its assignments in
the same undo step.
*Verification:* `MainWindowTest.MaterialsCanBeCreatedEditedAndAssigned`.

### RES-APP-013 — Project file

A project is one JSON document preserving the scene hierarchy, object ids,
geometry definitions and parameters, transforms, material definitions and
references, visibility, the application and format version, the unit
conventions and data of features this build does not know ("extensions").
No GPU object and no window state is stored in it.
*Verification:* `ProjectFormat.RoundTripPreservesEverything`,
`DocumentStatesFormatVersionAndUnits`,
`UnknownTopLevelDataInExtensionsSurvivesARoundTrip`,
`MainWindowTest.SaveAndReopenPreservesTheScene`.

### RES-APP-014 — Loading is validated and safe

A project is validated completely before it replaces the open one. Missing,
malformed and invalid files are reported with the reason and leave the
current project untouched. Saving is atomic. A reference to a missing
material is repaired with a warning.
*Verification:* `ProjectFormat.InvalidDocumentsAreRejectedWithTheReason`,
`MissingFileIsReportedNotThrown`, `MalformedJsonFileIsRejected`,
`UnwritableLocationIsReportedAndLeavesNoFile`,
`MissingMaterialReferenceIsRepairedWithAWarning`,
`MainWindowTest.OpeningABadFileFailsCleanlyAndKeepsTheCurrentProject`.

### RES-APP-015 — Format versions

The file states its schema version. Older versions are migrated step by
step; a file from a newer version is refused with an explanation.
*Verification:* `Migration.*`,
`ProjectFormat.NewerFileVersionIsRefusedWithAnExplanation`.

### RES-APP-016 — New, open, save, unsaved changes

New, Open, Save and Save As work. Unsaved changes are detected, shown in
the title, and the user is asked before they would be discarded (new, open,
close), with Save, Discard and Cancel each honoured.
*Verification:* `MainWindowTest.UnsavedChangesArePromptedForAndEachAnswerIsHonoured`.

### RES-APP-017 — Simulation infrastructure without physics

A simulation configuration structure, solver and domain identifiers, a job
abstraction with states queued, running, completed, failed and cancelled,
progress reporting, cancellation and result metadata exist. Jobs run off
the user-interface thread. The only job in this milestone is a
demonstrator that performs no calculation and says so.
*Verification:* `Jobs.*`, `Simulation.*`,
`MainWindowTest.DemonstratorJobRunsReportsProgressAndCancels`.

### RES-APP-018 — Logging and diagnostics

Structured logging (time, level, category, message) records startup and
shutdown, dependency versions, graphics initialization, project load and
save, scene modifications, job lifecycle and errors. A Console panel shows
the log; a Diagnostics panel shows the environment and the project. Failed
operations are reported, never swallowed.
*Verification:* `Log.*`, `MainWindowTest.DiagnosticsReportTheEnvironmentAndTheProject`.

### RES-APP-019 — Tests

GoogleTest suites cover the core without a display or an OpenGL context,
and the rendering and application layers separately on a display.
*Verification:* suite `core` (tier t1), suite `app` (tier t2).

### RES-APP-020 — Honest status

`IMPLEMENTATION_STATUS.md` states for every capability whether it is
implemented and tested, implemented but not fully tested, partial, not
implemented, or deferred. Nothing appears functional that is not.
