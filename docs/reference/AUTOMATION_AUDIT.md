# Automation Audit (before the automation API)

The state of the code before REQ-CHAT-20261126-012, what it meant for an
automation API, and the migration chosen (ADR-000216). This audit describes
the code as it was found; the architecture that replaced the gaps is in
[the automation architecture](../architecture/AUTOMATION_ARCHITECTURE.md).

## How changes were made

| Path | Before | Finding |
|---|---|---|
| The window | `DocumentController` built `Command` objects itself (create, rename, transform, visibility, parameters, material, delete, duplicate, move, material library) and pushed them on its `CommandStack` | the only implementation of each operation lived in a Qt class: no other interface could reuse it |
| Domain panels | pushed `SetObjectStateCommand` with whole component states through `DocumentController::setObjectState` | kept: a whole state is one validated command; the session sees it like any change |
| Python in the process | `resonant.Project` (pybind11) edited its own project directly (`ObjectRef.change`, workflow actions) | no undo history, no revisions, no events |
| The console / script editor | a worker process changed a JSON copy; the result was applied as one composite (`projectChangeCommand`) | kept; also used by transactions and workflows (`Session::applyCopy`) |
| Workflows | `WorkflowJob` on a snapshot, the resulting project applied as one undo step | reusable as a job behind the API |
| Field runs | `AcousticController` built the field job (model, evaluator, estimate) inline | moved to `manipulators/AcousticFieldRun` so the GUI and the simulation service use one function |

## Identity, state and threads

- Objects and materials already had UUID ids (`Id`), never indices: kept as the API's resource ids.
- The undo history (`CommandStack`) had a listener and a clean mark; there was no revision counter. Revisions had to be counted from the history and the scene listener, so GUI changes and undo count as well.
- `JobManager` ran jobs on worker threads with listeners on those threads; the GUI forwarded them with queued connections. Results lived only in the panel controllers.
- The project and its history were owned by the GUI thread; nothing was thread-safe beyond the job manager.

## Dependencies found

- `protoc` 3.21 and libprotobuf 3.21: present. C++ gRPC (`libgrpc++`, `grpc_cpp_plugin`): **absent**, and installing system packages needs administrator rights.
- Python: `grpcio` 1.78, `protobuf` 6.33 and the `mcp` SDK 1.25 present. Buf: absent.

## Consequences for the design

1. Move each meaningful operation into a Qt-free service in the core and make the GUI call it (the GUI keeps its undo texts).
2. Count revisions and publish events from the scene and history listeners, not from the services alone, so changes made outside the services still count.
3. Host the API in the application on a local socket with calls run on the owner's thread, and serve gRPC from a separate bridge process (grpcio), because the C++ gRPC library is not available.
4. Check schema compatibility with a project tool against a recorded baseline (no Buf).
5. Keep the existing Python package; add a client of the contract and in-process dispatch.

## Conflicts recorded

- The request asks for "generated C++ stubs" of gRPC services. With no `grpc_cpp_plugin` the C++ side generates messages only; the transport is the bridge. Recorded in ADR-000216 (decision 4) and [the automation decisions](../architecture/AUTOMATION_DECISIONS.md); a C++ gRPC server can replace the bridge later without changing the contract.
- Mechanical, electromagnetic and full-wave runs stay in their panels for now: the API lists them as available in the application and not yet startable through the API, rather than offering a placeholder.
