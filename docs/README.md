# Resonant Documentation — Canonical Entry Point

This is the canonical documentation root. Read `AGENTS.md` first for the
operating constitution, then use this map:

```text
requests/                       human request intake
    |
docs/requirements/              normative behavior
docs/decisions/                 active architecture decisions
    |
work/                           active implementation records
    |
tests + evidence/               executable proof and verification records
    |
docs/architecture/              current system explanation
```

## Entry points

* `README.md` (repository root) — a short orientation for a first visit: what the system is, getting started, the panels. It points here; this file stays the canonical entry point (REQ-DEVOPS-011).
* `docs/requirements/README.md` and `docs/requirements/requirements.txt` —
  normative requirement owners and IDs. See `docs/requirements/development_operations.md` for lifecycle rules (REQ-DEVOPS-001..050).
* `docs/requirements/product.md` — the product vision and product requirements of Resonant (RES-PRODUCT-001..016).
* `docs/requirements/application_foundation.md` — the V0.1 application foundation (RES-APP-001..020).
* `docs/requirements/acoustics.md` — the acoustic physics engine (RES-ACOUSTIC-001..020); `docs/acoustics/ACOUSTIC_ENGINE.md`, `docs/acoustics/ACOUSTIC_MODELS.md` (equations, conventions, units), `docs/acoustics/ACOUSTIC_ARRAYS.md`, `docs/acoustics/ACOUSTIC_FOCUSING.md`, `docs/acoustics/ACOUSTIC_INVERSE_DESIGN.md`, `docs/acoustics/ACOUSTIC_VALIDATION.md` (numerical evidence).
* `docs/requirements/wave_manipulators.md` — passive wave manipulation and hybrid focusing (RES-MANIP-001..016); `docs/manipulators/WAVE_MANIPULATORS.md`, `docs/manipulators/REFLECTOR_DESIGN.md`, `docs/manipulators/ACOUSTIC_LENSES.md`, `docs/manipulators/WAVEGUIDES.md`, `docs/manipulators/RESONANT_CAVITIES.md`, `docs/manipulators/PASSIVE_FOCUSING.md` (the reflector field model and focus metrics), `docs/manipulators/HYBRID_FOCUSING.md`, `docs/manipulators/GEOMETRY_OPTIMIZATION.md`, `docs/manipulators/PASSIVE_WAVE_VALIDATION.md`.
* `docs/requirements/full_wave_acoustics.md` — full-wave numerical acoustics (RES-FULLWAVE-001..018); `docs/advanced_acoustics/ADVANCED_ACOUSTICS.md` (index), `docs/advanced_acoustics/SOLVER_SELECTION.md`, `docs/advanced_acoustics/FDTD_SOLVER.md`, `docs/advanced_acoustics/NUMERICAL_ACCURACY.md`, `docs/advanced_acoustics/BOUNDARY_CONDITIONS.md`, `docs/advanced_acoustics/MATERIAL_INTERACTIONS.md`, `docs/advanced_acoustics/BROADBAND_SIMULATION.md`, `docs/advanced_acoustics/FREQUENCY_ANALYSIS.md`, `docs/advanced_acoustics/MODAL_ANALYSIS.md`, `docs/advanced_acoustics/PERFORMANCE_AND_GPU.md`, `docs/advanced_acoustics/ACOUSTIC_VALIDATION.md`.
* `docs/requirements/mechanics.md` — mechanical waves and structural vibration (RES-MECH-001..018); `docs/mechanics/MECHANICAL_PHYSICS.md` (index), `docs/mechanics/STRUCTURAL_MECHANICS.md`, `docs/mechanics/MECHANICAL_MATERIALS.md`, `docs/mechanics/MODAL_ANALYSIS.md`, `docs/mechanics/FORCED_RESPONSE.md`, `docs/mechanics/TRANSIENT_VIBRATION.md`, `docs/mechanics/MECHANICAL_WAVE_PROPAGATION.md`, `docs/mechanics/MECHANICAL_FOCUSING.md`, `docs/mechanics/PHONONIC_STRUCTURES.md`, `docs/mechanics/FLUID_STRUCTURE_COUPLING.md`, `docs/mechanics/MECHANICAL_VALIDATION.md`.
* `docs/requirements/electromagnetics.md` — electromagnetic waves, antennas, arrays and RF (RES-EM-001..024); `docs/electromagnetics/ELECTROMAGNETIC_PHYSICS.md` (index), `docs/electromagnetics/MAXWELL_EQUATIONS.md`, `docs/electromagnetics/ELECTROMAGNETIC_MATERIALS.md`, `docs/electromagnetics/ANTENNA_ENGINEERING.md`, `docs/electromagnetics/RF_IMPEDANCE_AND_S_PARAMETERS.md`, `docs/electromagnetics/NEAR_FIELD_FAR_FIELD.md`, `docs/electromagnetics/ANTENNA_ARRAYS.md`, `docs/electromagnetics/ARRAY_FACTOR_AND_BEAMFORMING.md`, `docs/electromagnetics/TRANSMISSION_LINES.md`, `docs/electromagnetics/WAVEGUIDES_AND_CAVITIES.md`, `docs/electromagnetics/ELECTROMAGNETIC_SOLVERS.md`, `docs/electromagnetics/ELECTROMAGNETIC_VALIDATION.md`, `docs/electromagnetics/PERFORMANCE_AND_GPU.md`.
* `docs/requirements/compute.md` — compute backends: deterministic threads, the optional CUDA backend and VOLK kernels (RES-COMPUTE-001..011); `docs/compute/ACCELERATION_AUDIT.md` (every compute path, measurements), `docs/compute/COMPUTE_BACKENDS.md` (choosing, building, measuring).
* `docs/requirements/optimization.md` — optimization, inverse design, geometry and topology optimization (RES-OPT-001..025); `docs/optimization/README.md` (index), `docs/optimization/OPTIMIZATION_ARCHITECTURE.md`, `docs/optimization/DESIGN_VARIABLES.md`, `docs/optimization/OBJECTIVE_FUNCTIONS.md`, `docs/optimization/CONSTRAINT_SYSTEM.md`, `docs/optimization/OPTIMIZATION_ALGORITHMS.md`, `docs/optimization/INVERSE_DESIGN.md`, `docs/optimization/GEOMETRY_OPTIMIZATION.md`, `docs/optimization/TOPOLOGY_OPTIMIZATION.md`, `docs/optimization/MULTI_FIDELITY_OPTIMIZATION.md`, `docs/optimization/SURROGATE_MODELS.md`, `docs/optimization/ROBUST_OPTIMIZATION.md`, `docs/optimization/DOMAIN_OPTIMIZATION_ADAPTERS.md`, `docs/optimization/OPTIMIZATION_VALIDATION.md`.
* `docs/requirements/experiments.md` — experimental hardware, acquisition, calibration and simulation correlation (RES-EXP-001..027); `docs/experiments/README.md` (index), `docs/experiments/EXPERIMENT_ARCHITECTURE.md`, `docs/experiments/HARDWARE_ABSTRACTION.md`, `docs/experiments/DEVICE_DRIVER_FRAMEWORK.md`, `docs/experiments/SENSOR_AND_ACTUATOR_MODELS.md`, `docs/experiments/MEASUREMENT_DATA_MODEL.md`, `docs/experiments/ACQUISITION_AND_SYNCHRONIZATION.md`, `docs/experiments/CALIBRATION_FRAMEWORK.md`, `docs/experiments/SIGNAL_PROCESSING_PIPELINE.md`, `docs/experiments/SPATIAL_MEASUREMENT.md`, `docs/experiments/SIMULATION_MEASUREMENT_CORRELATION.md`, `docs/experiments/EXPERIMENT_SAFETY.md`, `docs/experiments/HARDWARE_INTEGRATIONS.md`, `docs/experiments/EXPERIMENT_VALIDATION.md`.
* `docs/requirements/automation.md` — Python API, scripting, automation, plugins, in-app help and demonstrations (RES-PY-001..011, RES-AUTO-001..008, RES-PLUG-001..008, RES-HELP-001..012, RES-DEMO-001..010); `docs/python/PYTHON_API.md`, `docs/python/PYTHON_SCRIPTING.md`, `docs/python/PYTHON_EXECUTION_ARCHITECTURE.md`, `docs/automation/AUTOMATION_ARCHITECTURE.md`, `docs/automation/WORKFLOW_SYSTEM.md`, `docs/plugins/PLUGIN_ARCHITECTURE.md`, `docs/plugins/PLUGIN_DEVELOPMENT.md`, `docs/plugins/PLUGIN_API_REFERENCE.md`, `docs/help/HELP_SYSTEM_ARCHITECTURE.md`, `docs/help/HELP_CONTENT_STRUCTURE.md`, `docs/help/SCREENSHOT_CAPTURE_SYSTEM.md`, `docs/help/DOCUMENTATION_BUILD.md`, `docs/help/DOCUMENTATION_COVERAGE.md` (generated), `docs/demos/DEMO_SYSTEM_ARCHITECTURE.md`, `docs/demos/DEMO_PROFILE_FORMAT.md`, `docs/demos/DEMO_SEQUENCE_FORMAT.md`, `docs/demos/PRESENTER_MODE.md`, `docs/demos/VIDEO_PRODUCTION_GUIDE.md`, `docs/demos/DEMO_CATALOG.md`, `docs/reference/ANALYST_REFERENCE_AUDIT.md` (the RF_IQ_Analyst reference audit).
* `docs/requirements/automation_api.md` — the automation API: application services, the protobuf contract, the local endpoint and gRPC bridge, events, jobs, transactions, Python and MCP (RES-API-SVC-001..010, RES-API-EVT-001..004, RES-API-TX-001..003, RES-API-PB-001..005, RES-API-SEC-001..004, RES-API-GUI-001..003, RES-API-PY-001..002, RES-API-MCP-001, RES-API-PERF-001, RES-API-DOC-001); `docs/reference/AUTOMATION_AUDIT.md` (the audit before the change), `docs/architecture/AUTOMATION_ARCHITECTURE.md`, `docs/architecture/AUTOMATION_DECISIONS.md`, `docs/security/AUTOMATION_SECURITY.md`, `docs/architecture/MCP_INTEGRATION.md`, `docs/reference/PROTOBUF_API.md` (generated from `proto/`), `docs/reference/GRPC_CLIENT_GUIDE.md`, `docs/reference/PYTHON_AUTOMATION.md`, `docs/reference/EVENTS_AND_JOBS.md`, `docs/reference/TRANSACTIONS_AND_WORKFLOWS.md`, `docs/development/ADDING_AUTOMATION_OPERATIONS.md`.
* `docs/requirements/distribution.md` — the self-contained Linux ZIP (RES-DIST-001..015); `docs/release/LINUX_ZIP_DISTRIBUTION.md` (for users), `docs/release/PACKAGING_AUDIT.md`, `docs/release/PACKAGING_DECISIONS.md`, `docs/release/LINUX_COMPATIBILITY.md`, `docs/release/LICENSE_AUDIT.md`, `docs/release/RELEASE_REPORT.md`, `docs/release/CHANGELOG.md`; the tools are in `tools/release/`.
* User documentation: the in-app help's single source is `help/src/` (Markdown, built into `<build>/help` by `tools/help/build_help.py`; never edit the generated HTML); its screenshots are captured by `help/screenshots/manifest.json`. `python/README.md` (the Python package), `plugins/README.md` (plugins), `demos/README.md` (demonstrations).
* `BUILDING.md` (repository root) — build and test procedure for CMake and qmake; `IMPLEMENTATION_STATUS.md` — what is implemented, tested, partial, missing or deferred; `DECISIONS.md` — reading guide to the application's decision records; `examples/README.md` — example projects.
* `docs/architecture/README.md` — current architecture and subsystem map; `docs/architecture/overview.md` (layers, modules, conventions), `docs/architecture/roadmap.md` (milestones).
* `docs/decisions/README.md` — active ADRs; historical records are under
  `docs/history/decisions/`.
* `CI_CD.md` — technical map of the development-control system as implemented (components, lifecycle, state, verification).
* `docs/process/configuration.md` — `devsys.json`: every project setting, bootstrapping a project (`scripts/devsys.py install / init / doctor`), architecture rules, record formats (ADR-000199).
* `docs/process/portability.md` — supported environment and the platform primitives (`scripts/devsys_platform.py`).
* `docs/process/engineering-rules.md` — practical workflow summary.
* `docs/process/state-model.md` — valid states/transitions (authoritative).
* `docs/process/autonomous-operating-model.md` — operating manual for the autonomous system.
* `docs/process/chat-intake.md` — chat → canonical request convergence.
* `docs/process/crash-investigation.md` — crash investigation workflow.
* `docs/process/build-contract.md` — canonical BUILD→TEST→VERIFICATION contract.
* `docs/process/recovery.md` — failure recovery; `docs/process/escalation.md` — human escalation; `docs/process/process-protection.md` — process protection.
* `docs/process/historical-id-collisions.md` — how an accepted historical id collision is declared and how `drift_detect` classifies a duplicate (ADR-000149).
* `docs/process/worker-orchestrator.md` — the **Worker Orchestrator**: workers that execute this system's requests unattended (one active request per worker, claims through the framework, gated completion, verified recovery), `scripts/devsys.py workers | queue | priority`, the Qt control panel `tools/worker-panel` (WORKER-001..013, `docs/requirements/worker_orchestration.md`, ADR-000200).
* `docs/process/agent-profiles.md` — the agent protocol workers speak, agent profiles per worker slot (`workers.profiles` / `workers.assign`), and the documented vendor-specific profiles (configuration only; adapters in `scripts/workers/profiles/`).
* `docs/process/agent-coordination.md` — **agent coordination**: the roster of agents with friendly names, the messages agents and the operator send each other (`scripts/msg.py`), the web control panel (`tools/panel/web_panel.py`) and the terminal dashboard (`tools/panel/dash.py`), shared across git worktrees; the station (`tools/panel/station.py`) that the first agent starts: push delivery through a local hub, panels that end with the last agent (COORD-001..012, `docs/requirements/agent_coordination.md`, ADR-000201, ADR-000202).
* `docs/history/worker-orchestrator-merge.md` — historical: how the former external orchestrator was imported (phase 1) and merged (phase 2).
* `docs/verification/tests.md` — canonical verification commands.
* `scripts/test_manifest.json` + `scripts/run_tests.py` — the registered test suites and the tiered, affected-test orchestrator (REQ-DEVOPS-046, ADR-000186).
* `docs/audits/current-state.md` — current risks, known gaps and architecture invariants.
* `docs/verification/architecture-audit.md` — the **architecture / conformance audit**: `./tools/audit [quick|standard|baseline]` measures the implementation against the canonical documents (reusing the existing check scripts and the test orchestrator), records PASS / DRIFT / UNKNOWN per documented rule with evidence under `.audit/runs/AUDIT-*/`, compares with the previous audit and writes a compact AI audit packet; read-only and advisory (ARCH-AUDIT-*, `docs/requirements/architecture_audit.md`, ADR-000195).
* `work/README.md` and `work/work.txt` — active work state.
* `evidence/README.md` and `evidence/evidence.txt` — durable verification proof.

PROJECT-SPECIFIC: add one entry per subsystem architecture document,
interface / file-format specification, user manual and developer guide.

## Current architecture location

Current architecture documents are under `docs/architecture/`. The repository
does not use parallel `desired/` and `actual/` trees. Historical migration
documents may mention those former paths; they are not current authority.

## Build/test

Build through the configured adapter (`python3 scripts/devsys.py build build`;
tree `paths.build_dir`, default `build/debug/`). Run the commands in
`docs/verification/tests.md` and record actual results in `evidence/`
(`scripts/evidence.py`). `python3 scripts/devsys.py status` shows what is
happening in the repository.
