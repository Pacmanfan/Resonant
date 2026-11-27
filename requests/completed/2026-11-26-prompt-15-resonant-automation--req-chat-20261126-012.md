# Request: # PROMPT 15 — RESONANT Automation Framework
## Comprehensive Architecture Audit,

**Request ID:** REQ-CHAT-20261126-012
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T21:40:21-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 15 — RESONANT Automation Framework
## Comprehensive Architecture Audit, Protobuf API, Application Services, gRPC, Events, Transactions, and Multi-Client Automation

You are working on RESONANT — Wave Field Engineering Studio.

Your task is to perform a comprehensive audit of the existing application and then design and implement a foundational, production-quality automation framework.

This is a major architectural milestone. Do not treat it as a small API feature or an isolated gRPC server.

The objective is to make RESONANT an automation-native scientific engineering platform in which every meaningful application capability can be accessed programmatically through a formally defined, versioned Google Protocol Buffers interface.

This interface will eventually support:

- The native Qt desktop application.
- Local automation clients.
- Python scripting and scientific workflows.
- gRPC clients.
- Model Context Protocol (MCP) servers and AI agents.
- Remote web interfaces and dashboards.
- External engineering applications.
- Automated experiments and optimization workflows.
- Future hardware control and instrument integration.

The GUI must not contain exclusive implementations of application functionality. The application service layer must own the actual operations, and all interfaces must use it.

**This prompt requires an audit followed by implementation. Do not stop after producing an architecture document.**

---

# PART 1 — FUNDAMENTAL ARCHITECTURAL REQUIREMENTS

## 1.1 Establish the automation-first principle

Adopt the following permanent architectural rule:

Every meaningful application capability must be implemented through an authoritative application service and exposed through a versioned, machine-readable automation interface.

If a user can perform a meaningful operation through the GUI, a corresponding programmatic capability must exist.

This includes, but is not limited to:

- Creating, opening, closing, saving and modifying projects.
- Creating, inspecting, modifying, positioning, rotating, scaling, duplicating, grouping and deleting scene objects.
- Creating and modifying geometry.
- Importing and exporting geometry and other supported files.
- Creating and editing materials.
- Creating, configuring and positioning emitters.
- Creating and configuring emitter arrays.
- Configuring wave manipulators.
- Creating and editing targets and measurement locations.
- Configuring simulation regions and boundary conditions.
- Selecting and configuring solvers.
- Validating models and configurations.
- Starting, monitoring and cancelling simulations.
- Retrieving simulation results.
- Creating field visualizations and cross-sections.
- Configuring visualization properties and camera positions.
- Creating and executing optimization problems.
- Managing experiments and measurements.
- Accessing diagnostics, logs and application status.
- Managing project history, undo and redo.
- Managing user preferences where appropriate.
- Accessing available documentation and demonstration capabilities.

Also expose lower-level operations needed for precise control, not merely high-level workflows.

For example, both of these must be supported:

- `CreateCircularArray`
- `UpdateEmitterPosition`

Likewise, support both:

- `ConfigureSimulationForFocusing`
- `SetSolverBoundaryCondition`

Compound operations must not replace fine-grained control.

## 1.2 One implementation, multiple interfaces

Implement a common application service layer.

The intended architecture is:

    Qt GUI --------------------+
    Python bindings ----------|
    gRPC clients -------------|
    MCP adapter --------------+----> Application Service Layer
    Web interface ------------|               |
    External automation ------+               |
                                               v
                                      Domain Implementations
                                               |
                     +-------------------------+---------------------+
                     |                         |                     |
                 Geometry                 Physics Engines         Storage
                     |                         |                     |
                     +-------------------------+---------------------+

The application service layer is authoritative.

The Qt GUI must call application services rather than directly implementing business logic through widget callbacks or manipulating internal domain objects independently.

The Python API must use the same underlying services.

The gRPC server must translate Protobuf requests into application service calls.

The MCP server must eventually adapt tool calls into the same automation contract, preferably through gRPC.

The web interface must use the same services through an appropriate network-facing adapter.

Do not create separate implementations of the same functionality for each interface.

Do not require gRPC to be running for the native GUI to operate. The application service layer must work in-process independently of network transports.

## 1.3 Preserve physics-domain independence

RESONANT contains separate scientific domains:

- Acoustics.
- Mechanical waves and structural vibration.
- Electromagnetics.

The automation framework must provide shared infrastructure without incorrectly unifying the physics.

Shared concepts may include projects, geometry references, object identities, jobs, events, solver metadata and result references.

Domain-specific physics messages must remain appropriately distinct.

For example, acoustic pressure, mechanical displacement and electromagnetic electric fields must not be represented as interchangeable quantities merely because they can all be visualized.

---

# PART 2 — AUDIT THE EXISTING APPLICATION

Before modifying code, perform a thorough audit.

Inspect:

- Repository structure.
- Existing architecture and architecture documents.
- C++20 application core.
- Qt6 GUI architecture.
- Scene graph and object model.
- Geometry and materials systems.
- Acoustic, mechanical and electromagnetic modules.
- Simulation and solver interfaces.
- Optimization framework.
- Project persistence and serialization.
- HDF5 integration.
- Job and background-processing architecture.
- Undo/redo implementation, if present.
- Python integration and pybind11 bindings.
- Plugin architecture.
- Help and demonstration system.
- Logging and diagnostics.
- Existing tests.
- Build and packaging configuration.

Identify where functionality currently lives.

Specifically determine:

1. Which operations are implemented directly in GUI classes.
2. Which operations already have reusable service interfaces.
3. Which domain classes are tightly coupled to Qt.
4. Which operations manipulate mutable global or singleton state.
5. Which operations are unsafe to invoke from background threads.
6. Which operations lack stable object identifiers.
7. Which operations cannot be serialized or reproduced.
8. Which existing job-management facilities can be reused.
9. Which parts of the application are currently placeholders or experimental.
10. Which functionality is planned but not implemented.

Do not claim an operation is implemented merely because a button or menu entry exists.

Produce:

`docs/reference/AUTOMATION_AUDIT.md`

Include:

- Existing architecture.
- Current automation capabilities.
- GUI-only functionality.
- Existing service boundaries.
- State-management weaknesses.
- Threading and concurrency concerns.
- Persistence limitations.
- Proposed migration strategy.
- Risks and dependencies.
- Recommended implementation order.

Also create:

`docs/architecture/AUTOMATION_ARCHITECTURE.md`

This document must describe the intended architecture and its relationship to existing application modules.

Create or update:

`docs/architecture/AUTOMATION_DECISIONS.md`

Record important architectural decisions and their rationale.

Do not perform a wholesale rewrite. Preserve working functionality and make changes incrementally.

---

# PART 3 — PROTOBUF CONTRACT ARCHITECTURE

## 3.1 Protobuf is the authoritative external contract

Use Google Protocol Buffers as the canonical machine-readable definition of the RESONANT automation API.

Use proto3 syntax.

Use versioned packages, initially:

`resonant.v1`

The schema must be designed as a stable public interface, independent of internal C++ object layouts.

Use generated C++ types and generated client libraries where appropriate.

Use gRPC as the initial RPC transport.

Keep the application service interfaces independent of gRPC so that local in-process callers do not need network transport.

Do not implement a generic string-based command interpreter as the primary API.

Avoid interfaces such as:

    ExecuteCommand("create_sphere", "{...}")

Use strongly typed requests, responses, services and enums.

## 3.2 Organize the schema into logical packages

Create a structure similar to:

    proto/
      resonant/
        v1/
          common/
            identifiers.proto
            resources.proto
            units.proto
            geometry.proto
            transforms.proto
            errors.proto
            pagination.proto
            events.proto
            jobs.proto
            validation.proto
            metadata.proto

          application/
            application_service.proto
            capabilities.proto
            diagnostics.proto
            settings.proto

          project/
            project_service.proto
            project.proto

          scene/
            scene_service.proto
            scene.proto
            selection.proto
            transforms.proto

          geometry/
            geometry_service.proto
            primitives.proto
            imports.proto
            exports.proto

          materials/
            material_service.proto
            material.proto

          emitters/
            emitter_service.proto
            emitter.proto

          arrays/
            array_service.proto
            array.proto

          manipulators/
            manipulator_service.proto
            manipulator.proto

          targets/
            target_service.proto
            target.proto

          acoustics/
            acoustics_service.proto
            acoustic_models.proto

          mechanics/
            mechanics_service.proto
            mechanical_models.proto

          electromagnetics/
            electromagnetics_service.proto
            electromagnetic_models.proto

          simulation/
            simulation_service.proto
            solver.proto
            simulation.proto

          results/
            results_service.proto
            fields.proto
            probes.proto
            slices.proto

          optimization/
            optimization_service.proto
            optimization.proto

          experiments/
            experiment_service.proto
            measurement.proto

          visualization/
            visualization_service.proto
            visualization.proto

          files/
            file_service.proto
            file_formats.proto

          automation/
            transaction_service.proto
            workflow_service.proto
            subscription_service.proto
            session_service.proto

          help/
            help_service.proto

The exact package boundaries may be adjusted after the audit, but maintain clear separation and avoid one enormous schema file.

Do not create empty services simply to fill out this directory. Define the complete intended API architecture, then implement the services appropriate to the existing application state.

## 3.3 Define common message conventions

Establish shared types for:

- Resource identifiers.
- Project identifiers.
- Object identifiers.
- Job identifiers.
- Transaction identifiers.
- Request identifiers.
- Session identifiers.
- Revision numbers.
- Timestamps.
- Coordinate frames.
- Transformations.
- Physical units where necessary.
- Resource metadata.
- Validation results.
- Structured errors.
- Pagination.
- Capability descriptions.

Use UUID-based stable resource identifiers.

Do not use scene indices, display names or object ordering as permanent identifiers.

Names may change. IDs must not.

Every resource should expose enough metadata to support inspection and reliable automation.

## 3.4 Naming conventions

Use consistent RPC naming:

- `Create...`
- `Get...`
- `List...`
- `Update...`
- `Delete...`
- `Validate...`
- `Execute...`
- `Cancel...`
- `Import...`
- `Export...`
- `Subscribe...`

Use explicit field names.

Examples:

- `frequency_hz`
- `phase_radians`
- `position_meters`
- `amplitude_peak_pa`
- `temperature_kelvin`
- `time_seconds`

Where a unit cannot reasonably be included in the field name, define it explicitly in the schema documentation and type.

Do not use ambiguous fields such as `value`, `frequency`, `amplitude` or `position` without sufficient context.

## 3.5 Schema evolution

Establish strict Protobuf compatibility rules:

- Never reuse a deleted field number.
- Reserve deleted field numbers and names.
- Do not silently change field semantics.
- Add optional capabilities in backward-compatible ways.
- Version breaking API changes.
- Maintain deprecation notices.
- Document migrations.
- Keep generated code reproducible.
- Run automated schema compatibility checks in CI.

Use a tool such as Buf if appropriate after checking availability, version, license and integration requirements.

Document the compatibility policy.

---

# PART 4 — APPLICATION SERVICE LAYER

This is the most important implementation component.

## 4.1 Create authoritative application services

Create clean service interfaces between the public automation contract and internal domain implementations.

Examples:

- `ProjectService`
- `SceneService`
- `GeometryService`
- `MaterialService`
- `EmitterService`
- `ArrayService`
- `ManipulatorService`
- `TargetService`
- `SimulationService`
- `ResultsService`
- `OptimizationService`
- `ExperimentService`
- `VisualizationService`
- `FileService`
- `ApplicationService`
- `TransactionService`
- `WorkflowService`

Use interfaces and dependency injection where appropriate.

Avoid giant manager classes with hundreds of unrelated responsibilities.

Avoid global mutable state.

Avoid coupling service implementations to Qt widgets.

Services must be usable by:

- Native GUI.
- In-process Python.
- gRPC.
- Future MCP and web adapters.

## 4.2 Implement complete resource lifecycle operations

For all relevant resource types, support appropriate lifecycle operations:

- Create.
- Get.
- List.
- Update.
- Delete.
- Duplicate.
- Validate.
- Inspect.
- Export or serialize where appropriate.

Not every object needs every operation, but omissions must be deliberate and documented.

## 4.3 Stable object identity

Every persistent object must have a UUID.

Support:

- Stable IDs across save/load.
- New IDs for duplicated resources.
- Explicit relationship references.
- Revision tracking.
- Clear deletion behavior.
- Resource existence queries.
- Resource type discovery.

Implement a consistent resource reference structure.

Avoid fragile references based on names or scene-tree positions.

## 4.4 Revision and concurrency model

Every mutable resource should have a revision number or equivalent change token.

Support optimistic concurrency:

A client can request:

    UpdateEmitter(
        emitter_id,
        expected_revision,
        changes
    )

If the object has changed since the client's last observation, reject the stale update with a structured conflict response.

Do not silently overwrite another client's modifications.

Support project-level revision information for compound operations.

---

# PART 5 — COMMANDS, QUERIES AND APPLICATION STATE

## 5.1 Separate commands from queries

Use clear conceptual separation:

**Commands** modify state.

Examples:

- Create geometry.
- Move emitter.
- Assign material.
- Start simulation.
- Delete object.

**Queries** inspect state.

Examples:

- Get emitter.
- List materials.
- Get scene hierarchy.
- Get project metadata.
- Get simulation configuration.
- Retrieve results metadata.

**Subscriptions** receive state changes and events.

Maintain consistent behavior across all services.

## 5.2 Application and scene snapshots

Provide operations to retrieve:

- Application status.
- Current project state.
- Scene hierarchy.
- Object metadata.
- Resource revisions.
- Available solvers.
- Active jobs.
- Current selections where appropriate.
- Current visualization state.

Provide incremental change notifications after the initial snapshot.

Do not require remote clients to repeatedly retrieve the entire project.

## 5.3 Application capability discovery

Create a formal capability registry.

A client must be able to discover:

- Available physics domains.
- Available solver backends.
- Supported solver methods.
- Available geometry types.
- Supported material models.
- Available optimization algorithms.
- Supported file formats.
- Available plugins.
- Available visualization modes.
- Optional dependencies.
- Hardware capabilities.
- Feature maturity.
- Known limitations.

Each capability should have metadata such as:

- Stable capability identifier.
- Display name.
- Description.
- Availability.
- Maturity: stable, experimental, planned or unavailable.
- Required dependencies.
- Supported physics domain.
- Supported dimensions.
- Relevant parameter constraints.
- Known limitations.

The capability registry must reflect actual application availability.

Do not advertise a solver or feature as available when it is only planned.

Use the registry as the source for GUI feature availability, API discovery and future MCP tool discovery.

---

# PART 6 — EVENT AND JOB FRAMEWORK

## 6.1 Unified event system

Create an application-wide event framework.

Events should cover:

- Project created, opened, modified, saved and closed.
- Scene objects created, modified and deleted.
- Geometry modified.
- Materials changed.
- Emitter and array configuration changed.
- Simulation queued, started, progressed, completed, failed and cancelled.
- Optimization progress and completion.
- Measurement creation.
- Experiment state changes.
- Result creation.
- Application state changes.
- Capability changes where relevant.
- Diagnostic events.

Each event should include:

- Event ID.
- Event type.
- Timestamp.
- Sequence number.
- Source client or subsystem.
- Project ID where relevant.
- Resource ID where relevant.
- Revision information where relevant.
- Structured event payload.

Use typed Protobuf event messages.

Avoid sending arbitrary unstructured strings as the only event content.

## 6.2 Event subscriptions

Provide subscription interfaces.

Support filtering by:

- Project.
- Resource.
- Event category.
- Job.
- Session.

Support:

- Initial snapshot followed by incremental events.
- Reconnection.
- Sequence tracking.
- Detection of missed events.
- Resynchronization.
- Subscription cancellation.

Use streaming RPCs where appropriate.

Do not assume network connections will remain uninterrupted.

## 6.3 Unified background job manager

Implement or integrate a common job framework.

Use it for:

- Simulations.
- Optimization.
- Geometry processing.
- Mesh generation.
- Large imports and exports.
- Result processing.
- Long-running workflows.

A job should expose:

- Job ID.
- Job type.
- State.
- Progress.
- Current phase.
- Start time.
- Elapsed time.
- Completion time.
- Error information.
- Cancellation support.
- Result references.

Use well-defined states such as:

- CREATED
- QUEUED
- RUNNING
- CANCELLING
- CANCELLED
- COMPLETED
- FAILED

A long-running RPC should normally return a job reference promptly rather than hold the client connection open until completion.

The GUI, Python and remote clients must observe the same job state.

Support job inspection, cancellation and result retrieval.

Implement progress reporting without excessive event flooding.

---

# PART 7 — TRANSACTIONS, BATCHING AND WORKFLOWS

## 7.1 Batch operations

Support efficient batch operations.

Examples:

- Create multiple geometry objects.
- Populate arrays with many emitters.
- Update multiple emitter phases.
- Assign materials to multiple objects.
- Apply transforms to groups.
- Configure a simulation in one operation.

Batch operations should avoid unnecessary scene rebuilding and expensive recalculation after every individual modification.

Use appropriate internal batching and deferred recomputation.

## 7.2 Transactions

Implement transactions where practical.

Support:

- Begin transaction.
- Stage operations.
- Validate transaction.
- Commit transaction.
- Abort transaction.
- Retrieve transaction status.

A transaction should provide atomic behavior when the operation set can genuinely be committed atomically.

For operations with irreversible external side effects, such as hardware measurements or file exports, explicitly define partial completion and failure behavior.

Do not falsely advertise universal rollback.

Support transaction grouping in the undo/redo history.

## 7.3 Dry-run and planning

Provide a validation and planning mode.

An automation client should be able to submit a proposed sequence of changes and request:

- Validation.
- Expected modifications.
- Dependency checks.
- Resource requirements.
- Potential conflicts.
- Warnings.
- Estimated memory and computational requirements where possible.
- Expected side effects.

A dry-run must not mutate project state.

Provide a clear distinction between validation and actual execution.

## 7.4 Workflow execution engine

Establish the architecture for executing compound workflows.

Support a sequence or dependency graph of operations.

A workflow should eventually support:

- Ordered steps.
- Dependency relationships.
- Conditional execution.
- Checkpoints.
- Progress.
- Cancellation.
- Safe retries.
- Structured outputs.
- Error recovery.
- Workflow history.

Build a practical initial implementation if the existing architecture permits it.

Do not create an unnecessarily complicated general-purpose workflow platform before the basic application services are stable.

---

# PART 8 — IDEMPOTENCY, RETRIES AND MULTI-CLIENT COORDINATION

## 8.1 Idempotency

Support request IDs and idempotency keys for operations where duplicate execution could be harmful.

Example:

A client creates an emitter, loses its connection before receiving the response and retries.

The system must avoid creating duplicate resources when the same idempotency key is used.

Define retention and scope rules for idempotency records.

## 8.2 Retry semantics

Document:

- Which operations are safe to retry.
- Which operations require idempotency keys.
- Which operations must not be automatically retried.
- How clients discover whether an operation completed after a connection failure.

## 8.3 Multi-client coordination

Allow simultaneous clients:

- Desktop GUI.
- Python scripts.
- Remote automation clients.
- MCP agents.
- Web dashboards.

Define:

- Session identity.
- Client identity.
- Resource ownership where appropriate.
- Locking where necessary.
- Conflict detection.
- Revision checking.
- Event visibility.
- Client disconnection behavior.

Do not assume a single-client application.

Avoid locking entire projects for routine edits when resource-level conflict detection is sufficient.

---

# PART 9 — SCIENTIFIC DATA AND UNIT SAFETY

## 9.1 Units and coordinate conventions

Establish authoritative conventions:

- SI internally.
- Explicit coordinate systems.
- Right-handed coordinate convention.
- World and local coordinate frames.
- Explicit transform semantics.
- Explicit phase conventions.
- Radians internally unless an interface field explicitly defines otherwise.
- Explicit amplitude definitions.
- Explicit RMS versus peak conventions.
- Explicit decibel reference quantities.
- Explicit time and frequency units.

Document the global world coordinate convention:

- X: horizontal.
- Y: depth.
- Z: vertical.

Allow object-local coordinate frames.

Never assume a position is expressed in world coordinates when it may be local.

## 9.2 Scientific result provenance

Every result must retain relevant provenance:

- Physics domain.
- Solver identity and version.
- Solver configuration.
- Material data and provenance.
- Geometry revision.
- Source configuration.
- Boundary conditions.
- Mesh configuration.
- Time-step or frequency settings.
- Numerical assumptions.
- Calibration information where relevant.
- Units.
- Approximation level.
- Relevant uncertainty or limitations.

A remote client must be able to determine how a result was produced.

Do not present results from approximate models as equivalent to full-wave numerical solutions.

## 9.3 Large-data transfer

Do not use Protobuf messages as an unrestricted container for massive numerical arrays.

Separate the control plane from the bulk data plane.

**Control plane: Protobuf/gRPC**

Use for:

- Commands.
- Queries.
- Metadata.
- Job status.
- Event notifications.
- Result references.
- Small data responses.

**Data plane: appropriate binary transfer**

Use for:

- Large 2D and 3D fields.
- Complex-valued simulation volumes.
- Meshes.
- Measurement datasets.
- Large optimization results.
- Waveforms.

Evaluate:

- Chunked binary streaming.
- HDF5-backed access.
- Local memory-mapped files.
- Remote streaming.
- Compression.
- Downsampling.
- Region-of-interest extraction.

Provide efficient result access operations such as:

- Retrieve a selected 2D slice.
- Retrieve a bounded 3D region.
- Retrieve selected sample points.
- Retrieve a probe history.
- Retrieve result metadata.
- Retrieve a downsampled field.

Do not require downloading an entire volume to inspect one plane.

Result references must include sufficient metadata to identify the dataset, dimensions, data type, units and coordinate frame.

Do not expose arbitrary filesystem paths to untrusted remote clients.

---

# PART 10 — COMPLETE SERVICE COVERAGE

Define and implement service interfaces for the following areas.

## 10.1 Application and project services

- Application status.
- Version information.
- Capabilities.
- Diagnostics.
- Project creation.
- Project opening.
- Project saving.
- Project closing.
- Project cloning.
- Project metadata.
- Project revisions.
- Project recovery.
- Undo and redo.
- Project validation.

## 10.2 Scene and geometry services

- Create primitives.
- Create parametric geometry.
- Import supported geometry formats.
- Export supported geometry formats.
- Retrieve geometry metadata.
- Update dimensions and parameters.
- Position, rotate and scale.
- Duplicate and delete.
- Group and ungroup.
- Hide and show.
- Lock and unlock.
- Inspect geometry.
- Generate or modify simulation meshes.
- Validate geometry.
- Manage coordinate frames.

Include operations for existing geometry types and future custom geometry.

## 10.3 Materials services

- Create material.
- Get material.
- List materials.
- Update properties.
- Delete material.
- Assign material.
- Remove material assignment.
- Import material data.
- Retrieve provenance.
- Validate material compatibility.

Preserve domain-specific material models.

## 10.4 Emitters and arrays

Support:

- Create emitter.
- Delete emitter.
- Inspect emitter.
- Update individual properties.
- Move and rotate emitter.
- Set frequency.
- Set amplitude.
- Set phase.
- Set waveform.
- Set directivity.
- Set orientation.
- Set source model.
- Create array.
- Populate array.
- Configure array geometry.
- Configure element spacing.
- Configure array coordinate frame.
- Configure phase and delay distributions.
- Inspect individual array elements.
- Batch update array elements.
- Create custom arrays.

Support all relevant emitter types without forcing acoustic, mechanical and electromagnetic sources into an inappropriate shared physical model.

## 10.5 Wave manipulators

Support operations for:

- Parabolic reflectors.
- Elliptical reflectors.
- Spherical reflectors.
- Acoustic lenses.
- Waveguides.
- Resonant cavities.
- Diffraction structures.
- Layered structures.
- Custom geometry.
- Other supported domain-specific manipulators.

Provide geometry, material, placement and simulation configuration operations.

## 10.6 Physics and simulation services

Support domain-specific configuration and execution.

Include:

- Solver discovery.
- Solver configuration.
- Solver validation.
- Simulation region configuration.
- Boundary conditions.
- Mesh settings.
- Frequency-domain settings.
- Time-domain settings.
- Resource estimates.
- Simulation start.
- Simulation cancellation.
- Simulation status.
- Simulation results.
- Solver diagnostics.

Maintain domain-specific service contracts.

## 10.7 Results and measurements

Support:

- Result discovery.
- Result metadata.
- Field sampling.
- Cross-sections.
- Probes.
- Time histories.
- Frequency sweeps.
- Result comparisons.
- Export.
- Measurement ingestion.
- Measurement metadata.
- Calibration references.

## 10.8 Optimization services

Support:

- Create optimization problem.
- Configure variables.
- Configure objectives.
- Configure constraints.
- Configure algorithms.
- Validate problem.
- Estimate requirements.
- Execute.
- Monitor.
- Cancel.
- Inspect candidates.
- Compare candidates.
- Export selected candidate.
- Retrieve optimization history.

## 10.9 Visualization services

Expose meaningful visualization state and operations:

- Camera position.
- Camera orientation.
- View presets.
- Object visibility.
- Display modes.
- Field quantity.
- Color scale.
- Clipping planes.
- Slice creation and modification.
- Isosurfaces.
- Volume rendering settings.
- Animation controls.
- Screenshot capture.
- Visualization export.

Keep rendering state separate from physical simulation state.

Visualization changes must not unnecessarily trigger solver recomputation.

## 10.10 File services

Support:

- Supported format discovery.
- File inspection.
- Import.
- Export.
- Progress.
- Cancellation.
- Structured errors.
- Validation.
- File metadata.

Respect the application's security model and file access boundaries.

---

# PART 11 — ERROR HANDLING AND VALIDATION

Create a consistent structured error system.

Support clear error categories:

- INVALID_ARGUMENT.
- NOT_FOUND.
- ALREADY_EXISTS.
- FAILED_PRECONDITION.
- RESOURCE_EXHAUSTED.
- PERMISSION_DENIED.
- UNAUTHENTICATED.
- ABORTED.
- CONFLICT.
- UNAVAILABLE.
- DEADLINE_EXCEEDED.
- CANCELLED.
- INTERNAL.

Map these appropriately to gRPC status codes and structured application error details.

Errors should identify:

- Failed operation.
- Relevant resource.
- Invalid parameter where applicable.
- Explanation.
- Suggested correction where possible.
- Whether the operation changed any state.
- Whether retrying is safe.

Provide detailed validation messages.

Examples:

- Unsupported solver for selected physics domain.
- Invalid material properties.
- Geometry is non-manifold.
- Insufficient mesh resolution.
- Unstable time step.
- Missing boundary conditions.
- Unsupported emitter configuration.
- Resource estimate exceeds configured limits.

Avoid generic "operation failed" responses.

Validation must be available before committing important changes.

---

# PART 12 — SECURITY AND TRUST BOUNDARIES

Design the framework for both local and future remote use.

## 12.1 Local access

Support a secure local transport strategy.

Evaluate:

- Localhost gRPC.
- Unix domain sockets.
- Other appropriate local IPC.

Prefer local IPC for native local automation where practical.

Do not expose an unrestricted network listener by default.

## 12.2 Remote access preparation

Establish architecture for:

- Authentication.
- Authorization.
- TLS.
- Client identity.
- Session management.
- Permission scopes.
- Request auditing.
- Rate limiting.
- Resource limits.
- Connection management.

## 12.3 Permissions

Define permission categories such as:

- Application inspection.
- Project read.
- Project modification.
- Geometry modification.
- Simulation execution.
- Optimization execution.
- File import/export.
- Python execution.
- Hardware control.
- Administrative configuration.

Enforce permissions at the service layer, not only in the GUI or MCP adapter.

## 12.4 Dangerous operations

Python execution, plugin installation, external file access and future hardware control require explicit permission boundaries.

Do not assume that an authenticated client is automatically trusted with unrestricted access.

Do not add telemetry or external data transmission without explicit user consent.

Create:

`docs/security/AUTOMATION_SECURITY.md`

---

# PART 13 — PYTHON INTEGRATION

Audit the existing Python integration planned for RESONANT.

Refactor or extend it to use the same application services.

The Python API should provide intuitive access to:

- Projects.
- Scene objects.
- Geometry.
- Materials.
- Emitters.
- Arrays.
- Wave manipulators.
- Acoustic simulation.
- Mechanical simulation.
- Electromagnetic simulation.
- Results.
- Optimization.
- Experiments.
- Visualization.
- File operations.
- Automation workflows.

Use pybind11 for appropriate in-process C++ bindings.

Do not create a second implementation of domain logic in Python.

Support both:

1. Efficient in-process Python access.
2. Future remote Python access through the Protobuf/gRPC interface.

Keep long-running scientific operations asynchronous where appropriate.

Document object lifetime and thread-safety requirements.

---

# PART 14 — MCP ARCHITECTURE AND FUTURE WEB ACCESS

The immediate priority is the Protobuf API and application service layer.

However, explicitly design the interfaces to support future MCP and web clients.

## 14.1 MCP adapter

Create:

`docs/architecture/MCP_INTEGRATION.md`

Define how Protobuf services map to MCP tools and resources.

The adapter should eventually support:

- Tool discovery.
- Typed input schemas.
- Structured output.
- Resource references.
- Long-running jobs.
- Progress.
- Cancellation.
- Capability discovery.
- Permission checks.
- Error translation.

Do not duplicate business logic in MCP tools.

Do not overwhelm AI agents with hundreds of simultaneously exposed tools.

Support categorized discovery and high-level compound operations alongside fine-grained operations.

If implementation is practical without compromising the core framework, implement a small working MCP adapter demonstrating:

- Project inspection.
- Scene inspection.
- Geometry creation.
- Emitter creation or modification.
- Simulation submission.
- Job status retrieval.

Otherwise, provide a complete adapter design and a documented integration boundary.

## 14.2 Future web interface

Prepare the API for a web dashboard and remote control interface.

The web interface should eventually be able to:

- Inspect project state.
- Inspect scene objects.
- Modify supported properties.
- Submit simulations.
- Monitor progress.
- Retrieve results.
- View selected visualizations.
- Manage jobs.

Do not build an entire web application in this milestone.

The objective is to ensure no architectural rework will be required to support one later.

---

# PART 15 — GUI INTEGRATION

The existing Qt GUI must be refactored to use the application service layer.

Audit every significant GUI action.

Replace direct business-logic implementations with service calls.

Examples:

- Create Sphere button → `GeometryService.CreatePrimitive`.
- Move Emitter action → `EmitterService.UpdateEmitter`.
- Assign Material action → `MaterialService.AssignMaterial`.
- Start Simulation action → `SimulationService.StartSimulation`.
- Save Project action → `ProjectService.SaveProject`.

The GUI may still contain presentation-specific behavior, but it must not be the exclusive implementation of meaningful application operations.

Preserve existing user workflows.

Maintain responsive GUI behavior.

Use asynchronous service calls for expensive operations.

Ensure service calls marshal state updates safely onto the appropriate Qt thread.

Do not allow remote clients to manipulate Qt widgets directly.

The GUI should receive state changes through the shared event/state architecture.

---

# PART 16 — AUTOMATION DIAGNOSTICS AND AUDIT TRAIL

Create a unified audit and diagnostic system.

For every meaningful mutation, record:

- Timestamp.
- Request ID.
- Client identity.
- Session identity.
- Source interface.
- Operation.
- Affected resource IDs.
- Before/after revision where applicable.
- Result.
- Error details if failed.

Support querying the audit history.

Do not log passwords, tokens, private credentials or sensitive file contents.

Provide diagnostic APIs for:

- Application health.
- Service availability.
- Active sessions.
- Active jobs.
- Resource usage.
- Recent errors.
- Solver availability.
- Dependency status.

Make logs useful for debugging without overwhelming the system.

Audit history should be persisted or exportable where appropriate and associated with reproducible engineering workflows.

---

# PART 17 — BUILD SYSTEM AND DEPENDENCIES

Audit the existing CMake configuration.

Evaluate and integrate appropriate dependencies for:

- Protocol Buffers.
- gRPC.
- Protobuf code generation.
- Schema compatibility checking.
- UUID handling if needed.
- Existing JSON and serialization infrastructure.
- Testing.

Check dependency versions, availability, licenses and redistribution requirements.

Do not blindly add dependencies.

Prefer CMake-integrated and reproducible dependency management.

Ensure generated code is built consistently.

Decide whether generated Protobuf code should be committed or generated during the build, and document the rationale.

Support clean builds from a fresh checkout.

Avoid coupling the entire application to gRPC libraries if only the transport adapter requires them.

---

# PART 18 — TESTING AND QUALITY ASSURANCE

This framework must be testable independently of the GUI.

## 18.1 Unit tests

Test:

- Service behavior.
- Resource lifecycle.
- UUID stability.
- Revision management.
- Validation.
- Transaction behavior.
- Idempotency.
- Conflict detection.
- Event sequencing.
- Job state transitions.
- Error mapping.

## 18.2 Protobuf contract tests

Test:

- Request serialization.
- Response serialization.
- Schema compatibility.
- Enum behavior.
- Default values.
- Field presence semantics.
- Error structures.
- Version compatibility.

## 18.3 gRPC integration tests

Test:

- Server startup.
- Client connection.
- Every implemented service.
- Authentication boundaries.
- Request validation.
- Concurrent clients.
- Disconnection and reconnection.
- Streaming events.
- Cancellation.
- Timeouts.
- Large-data access.

## 18.4 GUI/service conformance tests

Verify that performing an operation through the GUI-facing service path and through the gRPC API produces equivalent application state.

Test representative workflows:

1. Create project.
2. Create geometry.
3. Position an emitter.
4. Configure emitter properties.
5. Assign material.
6. Create a target.
7. Configure a simulation.
8. Submit a job.
9. Monitor progress.
10. Retrieve results.
11. Save and reload the project.

## 18.5 Python tests

Verify that Python operations invoke the same underlying application behavior.

Test representative workflows from a clean Python process where appropriate.

## 18.6 Performance tests

Benchmark:

- Individual RPC operations.
- Batch operations.
- Large scene inspection.
- Event throughput.
- Concurrent clients.
- Large geometry transfers.
- Result chunking.
- Job submission.
- Memory consumption.

Avoid introducing unnecessary serialization overhead into local in-process GUI operations.

## 18.7 Regression tests

Run the existing application test suite.

Preserve existing project compatibility.

Do not sacrifice existing physics functionality to implement automation.

---

# PART 19 — DOCUMENTATION

Create comprehensive documentation.

Required documents:

- `docs/reference/AUTOMATION_AUDIT.md`
- `docs/architecture/AUTOMATION_ARCHITECTURE.md`
- `docs/architecture/AUTOMATION_DECISIONS.md`
- `docs/security/AUTOMATION_SECURITY.md`
- `docs/architecture/MCP_INTEGRATION.md`
- `docs/reference/PROTOBUF_API.md`
- `docs/reference/GRPC_CLIENT_GUIDE.md`
- `docs/reference/PYTHON_AUTOMATION.md`
- `docs/reference/EVENTS_AND_JOBS.md`
- `docs/reference/TRANSACTIONS_AND_WORKFLOWS.md`
- `docs/development/ADDING_AUTOMATION_OPERATIONS.md`

The developer guide must explain exactly how to add a new application capability:

1. Implement or extend the application service.
2. Define the Protobuf messages and RPC.
3. Implement the adapter.
4. Register the capability.
5. Connect the GUI.
6. Add Python exposure where appropriate.
7. Add tests.
8. Update documentation.

The guide must make it difficult for future developers to accidentally create GUI-only functionality.

Generate reference documentation from the Protobuf schemas where practical.

---

# PART 20 — IMPLEMENTATION PHASES

Perform the implementation in controlled phases.

Do not attempt to rewrite the entire application in one pass.

## Phase A — Audit and architecture

- Inspect existing code.
- Produce audit.
- Define service boundaries.
- Define Protobuf conventions.
- Define threading and state ownership.
- Define event/job architecture.
- Define migration strategy.
- Document dependencies and risks.

## Phase B — Core automation foundation

Implement:

- Protobuf build integration.
- Common message types.
- Application service interfaces.
- Resource IDs.
- Revision model.
- Structured errors.
- Capability registry.
- Basic event architecture.

## Phase C — Core service implementations

Implement representative services:

- Application.
- Project.
- Scene.
- Geometry.
- Materials.
- Emitters.
- Arrays.

Use actual existing functionality.

Do not fabricate unsupported operations.

## Phase D — Jobs, events and transactions

Implement:

- Unified job manager.
- Event publishing.
- Subscriptions.
- Job inspection.
- Cancellation.
- Batch operations.
- Transactions where feasible.
- Dry-run validation.

## Phase E — gRPC transport

Implement:

- Local server.
- Generated C++ stubs.
- Client connection.
- Service adapters.
- Structured error translation.
- Streaming events.
- Integration tests.

The GUI must continue to function without requiring the gRPC server.

## Phase F — GUI integration

Refactor representative GUI workflows to invoke the shared application services.

Expand this to all significant existing operations as the architecture permits.

## Phase G — Python integration

Connect the Python API to the same service layer.

Preserve efficient in-process access.

## Phase H — MCP readiness

Document MCP mapping and implement a small working adapter if practical.

## Phase I — Validation and documentation

Run full regression tests.

Test the end-to-end automation workflows.

Complete documentation and update the roadmap.

At the end of each phase, provide a clear implementation report with:

- Files changed.
- Architecture changes.
- Functionality implemented.
- Tests run.
- Test results.
- Known limitations.
- Remaining work.

---

# PART 21 — STRICT ENGINEERING RULES

1. Audit before implementation.
2. Preserve working functionality.
3. Do not perform a wholesale rewrite.
4. Do not create parallel business-logic implementations.
5. Do not allow the GUI to bypass application services for meaningful operations.
6. Do not couple domain models to Protobuf-generated classes.
7. Do not couple the application service layer to gRPC.
8. Do not require gRPC for native GUI operation.
9. Do not use string-based generic commands as the main API.
10. Do not expose internal C++ memory layouts.
11. Do not use unstable object indices as resource identifiers.
12. Do not silently overwrite concurrent modifications.
13. Do not claim universal transaction rollback.
14. Do not send enormous scientific datasets as single Protobuf messages.
15. Do not invent scientific units or ambiguous field semantics.
16. Do not advertise unimplemented capabilities.
17. Do not introduce unnecessary dependencies.
18. Do not weaken security to simplify implementation.
19. Do not implement placeholder services that return fabricated success.
20. Do not sacrifice physics correctness for API convenience.
21. Do not allow remote clients to access Qt widgets directly.
22. Do not expose unrestricted Python execution or hardware control by default.
23. Keep all new code documented and tested.
24. Maintain C++20 and the existing Qt6/CMake architecture.
25. Follow the project's existing coding standards and `AGENTS.md` instructions.

Where the existing application architecture conflicts with these requirements, document the conflict and implement the smallest safe migration that establishes the intended boundary.

---

# PART 22 — ACCEPTANCE CRITERIA

This milestone is successful when all applicable criteria below are met.

### Architecture

- [ ] A documented application service layer exists.
- [ ] The GUI uses application services for representative operations.
- [ ] Protobuf schemas define the public automation contract.
- [ ] Application services do not depend on gRPC.
- [ ] Domain implementations do not depend on generated Protobuf classes.
- [ ] Stable resource identifiers exist.
- [ ] Revision and concurrency behavior is documented and tested.

### API

- [ ] Protobuf and gRPC build successfully.
- [ ] Common message conventions are established.
- [ ] Project, scene, geometry, material and emitter services have working implementations.
- [ ] Supported operations have typed request/response messages.
- [ ] Capability discovery reports actual application functionality.
- [ ] Structured errors are returned.
- [ ] API versioning policy is documented.

### Events and jobs

- [ ] Long-running work uses a unified job model.
- [ ] Clients can retrieve job status.
- [ ] Clients can cancel eligible jobs.
- [ ] Event subscriptions work.
- [ ] Event sequence and reconnection behavior are documented.
- [ ] GUI and remote clients observe consistent state.

### Transactions and safety

- [ ] Batch operations are supported where appropriate.
- [ ] Validation can occur before committing changes.
- [ ] Transaction semantics are documented.
- [ ] Idempotency is implemented for appropriate operations.
- [ ] Revision conflicts are detected.
- [ ] Permission boundaries are established.

### Data

- [ ] Scientific units and coordinate conventions are documented.
- [ ] Result provenance is preserved.
- [ ] Large-data transfer is separated from normal control messages.
- [ ] Result metadata and bounded data retrieval are supported where implemented.

### Integration

- [ ] Native GUI continues to operate independently of the gRPC server.
- [ ] Representative GUI actions invoke shared application services.
- [ ] Python integration uses the same underlying services where implemented.
- [ ] A remote gRPC client can perform representative engineering operations.
- [ ] MCP integration has a documented and tested boundary.

### Quality

- [ ] Unit tests pass.
- [ ] Contract tests pass.
- [ ] Integration tests pass.
- [ ] GUI regression tests pass.
- [ ] Build works from a clean checkout.
- [ ] Documentation is complete and accurate.
- [ ] No fabricated functionality or results have been introduced.

---

# FINAL DELIVERABLE

At completion, provide a comprehensive implementation report.

Include:

1. Original architecture findings.
2. Major architectural changes.
3. Protobuf schema inventory.
4. Implemented services and operations.
5. GUI integration status.
6. gRPC server and client status.
7. Event and job implementation status.
8. Transaction and batching status.
9. Python integration status.
10. MCP readiness.
11. Security model.
12. Test coverage and actual test results.
13. Build and dependency changes.
14. Performance measurements where available.
15. Known limitations.
16. Outstanding work.
17. Recommended next development milestone.

Be precise about what is implemented, what is experimental and what remains planned.

The final outcome should be a genuine architectural foundation for an automation-native engineering application.

**The defining principle of RESONANT is now: Every meaningful application capability is available through a stable, discoverable, versioned automation contract, regardless of whether the caller is a human using the GUI, a Python script, an external program, a web interface or an AI agent.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence medium)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Framework
  - evidence: 'Framework' [head]; 'design and implement' [body]; 'framework' [body]; 'milestone' [body]; 'feature' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this as a ticket and process it in order with the rest' (queued after tickets 008-010)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this as a ticket and process it in order with the rest' (queued after tickets 008-010)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000014
- Evidence: pending
