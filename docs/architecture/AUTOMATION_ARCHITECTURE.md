# Automation Architecture

How programs, scripts and AI assistants drive Resonant (REQ-CHAT-20261126-012,
ADR-000216, requirements `RES-API-*` in
[automation_api.md](../requirements/automation_api.md)). For the workflow
engine itself see [the workflow architecture](../automation/AUTOMATION_ARCHITECTURE.md).

## Layers

```text
  GUI (DocumentController, panels)      Python in process      gRPC clients        MCP assistant
        |  direct calls                    | AutomationSession     |  gRPC (bytes)      | tools
        |                                  |  (native module)      v                    v
        |                                  |                  resonant_api.bridge   resonant_api.mcp
        |                                  |                       | Unix socket        | (a gRPC or socket client)
        |                                  v                       v                    |
        |                        src/automation_api: Dispatcher <- LocalEndpoint <------+
        |                          (decode, call, encode: the only user of generated protobuf types)
        v                                  v
  src/core/services: ProjectService, SceneService, GeometryService, MaterialService, EmitterService,
                     ArrayService, SimulationService, JobService, ResultService, OptimizationService,
                     WorkflowService, TransactionService, ApplicationService
                     Session: revisions, events (EventBus), sessions, audit, idempotency, jobs, results
        v
  domains and infrastructure: scene, commands, materials, acoustics, optimization, automation, ...
```

Rules (checked by `tools/check_layering.py`): the services include no Qt,
protobuf or gRPC header; nothing outside `src/core/services` includes them;
`src/automation_api` and `src/automation_host` include no Qt.

## The session

`services::Session` belongs to a **project host** (`ProjectHost`: project,
undo history, jobs, file path, replacing the project). Hosts: the GUI's
`DocumentController` (through `DocumentHost`), `OwnedProjectHost` (the
headless host `resonant-automation`, tests), and the native module's
`AutomationSession` (a Python project).

- **Mutations** go through `Session::mutate`: authorize, replay an idempotency key, check expected revisions, build the command (on a copy for a dry run), push it, record the audit. A call that changes nothing adds no undo step.
- **Revisions** are counted by listeners on the scene and the history: every committed change advances the project revision by one, including the GUI's own commands, undo, redo and direct Python edits; objects record their last revision.
- **Events** are published to the `EventBus` (thread-safe, 10 000 retained, global sequence) by the same listeners, attributed to the call being executed.
- **Jobs** started by services are tracked; when they end, their results are registered on the owner's thread (`collectFinishedJobs`).

## Threads

Calls run on the session's owner thread. The GUI's `AutomationServer`
marshals each call to the user-interface thread (`BlockingQueuedConnection`);
the headless host serializes calls with a lock and collects finished jobs
every 50 ms. Streams (`SubscribeEvents`, `ReadFieldSlice`) wait on the
connection's thread and visit the owner only to read. Stopping the endpoint
keeps the owner's events flowing until every connection ended.

## The contract and its build

`proto/resonant/v1` (17 files of services and their messages, plus common
types) is compiled by protoc at build time: C++ messages for the adapter and
a descriptor set with comments for Python, documentation and compatibility.
qmake: `protobuf.pri` (`RESONANT_AUTOMATION_API`), CMake:
`RESONANT_WITH_AUTOMATION_API`. Without protoc and libprotobuf the
application builds without the API. `proto/resonant/transport/v1/local.proto`
is the local endpoint's framing, not part of the public contract.

## Transports

- **Local endpoint** (`LocalEndpoint`): a Unix-domain socket, one call per connection, 4-byte little-endian length frames of `LocalRequest` / `LocalResponse`. See [the security document](../security/AUTOMATION_SECURITY.md).
- **gRPC** (`python3 -m resonant_api.bridge`): grpcio generic handlers relay each method's bytes to the socket; server streaming for events and slices; errors as gRPC status plus `resonant-error-bin`.
- **In process**: `resonant_api.connect(project=...)` uses the native module's dispatcher.

## Where the GUI uses the services

`DocumentController` creates primitives, renames, moves, shows and hides,
reparents, edits geometry parameters, assigns materials, replaces the
material library, deletes, duplicates, undoes, redoes and saves through the
services; `AcousticController` starts field runs through `SimulationService`.
Panels that set a whole component state push one command; the session counts
it like any other change.
