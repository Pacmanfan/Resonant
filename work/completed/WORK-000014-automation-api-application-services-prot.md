# WORK-000014: Automation API: application services, protobuf contract, local endpoint and gRPC bridge, events, jobs, transactions, GUI integration, Python client, MCP adapter

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-15-resonant-automation--req-chat-20261126-012.md
**Requirements:** RES-API-DOC-001, RES-API-EVT-001, RES-API-EVT-002, RES-API-EVT-003, RES-API-EVT-004, RES-API-GUI-001, RES-API-GUI-002, RES-API-GUI-003, RES-API-MCP-001, RES-API-PB-001, RES-API-PB-002, RES-API-PB-003, RES-API-PB-004, RES-API-PB-005, RES-API-PERF-001, RES-API-PY-001, RES-API-PY-002, RES-API-SEC-001, RES-API-SEC-002, RES-API-SEC-003, RES-API-SEC-004, RES-API-SVC-001, RES-API-SVC-002, RES-API-SVC-003, RES-API-SVC-004, RES-API-SVC-005, RES-API-SVC-006, RES-API-SVC-007, RES-API-SVC-008, RES-API-SVC-009, RES-API-SVC-010, RES-API-TX-001, RES-API-TX-002, RES-API-TX-003
**Decision:** ADR-000216
**Evidence:** VER-000014
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261127T171415-3885079

## Scope

Prompt 15, "Resonant Automation Framework": one application service layer
for every interface, a proto3 contract (`resonant.v1`), its adapter, a local
endpoint in the GUI and a headless host, gRPC (through a grpcio bridge, since
C++ gRPC is not installed), events, jobs, results in bounded pieces, batches
and transactions, the GUI's operations through the services and an
Automation Server dialog, a Python client and in-process dispatch, an MCP
adapter, schema compatibility, generated reference and the documents of
Part 19. Audit: `docs/reference/AUTOMATION_AUDIT.md`.

## Implementation

- `src/core/services/` (new): `ServiceTypes.h` (errors, outcomes, permissions, call context, change summaries), `EventBus`, `Session` (project host interface, `OwnedProjectHost`, revisions from scene and history listeners, material change detection, sessions, authorization, the mutation funnel with dry runs, idempotency and audit, jobs and results), `Services.h` and `ProjectServices.cpp`, `SceneServices.cpp`, `JobServices.cpp`, `TransactionService.cpp`, `ApplicationServices.cpp`.
- `src/core/manipulators/AcousticFieldRun.{h,cpp}` (new): the field run preparation shared by the GUI and the simulation service; `src/core/coupling/WorkflowActions.{h,cpp}`: `makeSceneObject`, `changedObjectState` reused by the services.
- `proto/resonant/v1/**` (17 service and message files, 68 commented methods), `proto/resonant/transport/v1/local.proto`, `proto/baseline/resonant_v1.json`.
- `src/automation_api/` (new library): `Conversions`, `Dispatcher` (+ `HandlerSupport.h`), `Handlers.cpp`, `Streams.cpp`, `LocalEndpoint` (Unix socket, same user, token, stop hook); build: `protobuf.pri`, `automation_api.pro`, `CMakeLists.txt`.
- `src/automation_host/` (new): `resonant-automation`.
- `src/app/`: `DocumentController` hosts the session and routes its operations through the services; `AcousticController::runField` through `SimulationService`; `AutomationServer` (calls on the GUI thread, bridge process) and `AutomationServerDialog` (+ `forms/AutomationServerDialog.ui`); Tools > Automation Server; `ResourcePaths` finds the descriptor set; `HelpTopics`.
- `src/python/ResonantModule.cpp`: `AutomationSession` (in-process dispatch).
- `python/resonant_api/` (new): `contract`, `transports`, `client`, `bridge`, `mcp`.
- `tools/automation/check_schema_compatibility.py`, `tools/automation/generate_protobuf_docs.py` (new); `tools/check_layering.py`: the automation rules.
- Build: `project.pro`, `CMakeLists.txt`, `src/app/app.pro`, `src/main/main.pro`, `src/python/python.pro`, `tests/core/core_tests.pro`, `tests/app/app_tests.pro` and their CMake files; `help/help.pro`, `help/CMakeLists.txt` install `resonant_api`.
- Security fix found on the way: a remote caller naming the application's session is refused.

## Tests

- `tests/core/test_services.cpp` (10): revisions, undo, events, outside changes, event paging and gaps, dry runs, idempotency, sessions and permissions, batches and transactions, material events on undo / redo, materials, emitters, arrays, field jobs and slices, workflows, capabilities, diagnostics.
- `tests/core/test_automation_api.cpp` (7): the dispatcher implements exactly the descriptor set's methods, round trips and structured errors, session policy, atomic batches through the contract, chunked slices, the endpoint's token and permissions, concurrent clients without lost updates.
- `python/tests/test_automation_api.py` (14, run by `PythonApi.ThePackageTestsPassAgainstThisBuildsModule` against this tree's headless host): both transports, dry runs, retries, permissions and files, capabilities, conflicts between clients, 4 concurrent clients with an ordered stream, resumption, transactions, a field job to a NumPy slice, workflows and cancel, MCP levels and audit, in process, latency.
- `python/tests/test_automation_schema.py` (4): compatibility with the baseline, detection of each breaking change, the generated reference is current, every method documented.
- `tests/app/test_automation_server.cpp` (3): a remote change is a GUI change, a window change reaches clients as a GUI event and audit record, the dialog lists sessions and stops with a stream open.
- Measured: GetProject median 0.12 ms over the local socket, 0.45 ms over gRPC.

## Regression impact and documentation

- The GUI's edits now go through the services: the whole app suite (110) and core suite (390 with the Python suite) pass; undo texts are kept.
- The automation API is optional in both builds; without protoc the application builds as before.
- Documents: `docs/requirements/automation_api.md`, ADR-000216, `docs/reference/AUTOMATION_AUDIT.md`, `docs/architecture/AUTOMATION_ARCHITECTURE.md`, `AUTOMATION_DECISIONS.md`, `MCP_INTEGRATION.md`, `docs/security/AUTOMATION_SECURITY.md`, `docs/reference/PROTOBUF_API.md` (generated), `GRPC_CLIENT_GUIDE.md`, `PYTHON_AUTOMATION.md`, `EVENTS_AND_JOBS.md`, `TRANSACTIONS_AND_WORKFLOWS.md`, `docs/development/ADDING_AUTOMATION_OPERATIONS.md`, help page `py.automation-server`, `docs/README.md`, `docs/architecture/{README,overview,roadmap}.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `BUILDING.md`, `README.md`, `docs/help/DOCUMENTATION_COVERAGE.md`.
- Stated in `IMPLEMENTATION_STATUS.md` and ADR-000216: gRPC is served by a bridge process; mechanical, electromagnetic and FDTD runs are listed as not yet startable through the API; no TLS.
- ADR-000217 was allocated by mistake and is unused (ids are never reused).

## Traceability

REQ-CHAT-20261126-012 -> RES-API-DOC-001, RES-API-EVT-001, RES-API-EVT-002, RES-API-EVT-003, RES-API-EVT-004, RES-API-GUI-001, RES-API-GUI-002, RES-API-GUI-003, RES-API-MCP-001, RES-API-PB-001, RES-API-PB-002, RES-API-PB-003, RES-API-PB-004, RES-API-PB-005, RES-API-PERF-001, RES-API-PY-001, RES-API-PY-002, RES-API-SEC-001, RES-API-SEC-002, RES-API-SEC-003, RES-API-SEC-004, RES-API-SVC-001, RES-API-SVC-002, RES-API-SVC-003, RES-API-SVC-004, RES-API-SVC-005, RES-API-SVC-006, RES-API-SVC-007, RES-API-SVC-008, RES-API-SVC-009, RES-API-SVC-010, RES-API-TX-001, RES-API-TX-002, RES-API-TX-003 -> WORK-000014 -> evidence (pending)

## Transitions

- 2026-11-27T21:02:13.302631+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T22:18:43.389670+00:00 active -> completed by agent-dev-20261126151713-76f632
