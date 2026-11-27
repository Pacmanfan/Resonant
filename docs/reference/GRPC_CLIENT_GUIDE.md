# gRPC Client Guide

Driving Resonant from any language over gRPC (RES-API-SEC-002, RES-API-PB-*).
The messages and methods are in the [protobuf reference](PROTOBUF_API.md).

## 1. Start the application side

Either the GUI (Tools > Automation Server > Start Server, then Start Bridge)
or headless:

```bash
resonant-automation --project-dir ~/projects --allow-files &      # prints {"socket": ..., "tokenFile": ..., "pid": ...}
PYTHONPATH=<share>/resonant/python python3 -m resonant_api.bridge --port 50051   # prints {"grpc": "127.0.0.1:50051"}
```

The bridge needs `grpcio` and `protobuf` for Python and finds the descriptor
set through `RESONANT_API_DESCRIPTORS`, the installation, or `protoc` with a
source checkout.

## 2. Generate a client from `proto/`

The `.proto` files are the contract; generate stubs with your language's
gRPC plugin, for example:

```bash
protoc -I proto --cpp_out=gen --grpc_out=gen --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) $(find proto/resonant/v1 -name '*.proto')
python3 -m grpc_tools.protoc -I proto --python_out=gen --grpc_python_out=gen $(find proto/resonant/v1 -name '*.proto')
```

Python users need no generated code: `resonant_api` builds its classes from
the descriptor set ([Python automation](PYTHON_AUTOMATION.md)).

## 3. Authenticate and open a session

Every call carries the token (the content of `automation.token` beside the
socket) as metadata `x-resonant-token` (or `authorization: Bearer <token>`).
Then:

1. `SessionService.OpenSession{client_name, requested_permissions}`: keep `session.session_id`.
2. Put it in every request's `header.session_id`; set `header.request_id` to trace a call in the audit trail.
3. `SessionService.CloseSession` at the end (jobs keep running).

`ApplicationService.GetApplicationInfo` works without a session (check
`api_version` = `resonant.v1`).

## 4. Calls, revisions and errors

- Responses carry `header.project_revision`; objects carry `revision`. Give `expected_revision` (or `header.expected_project_revision`) to be refused with `CONFLICT` instead of overwriting someone else's change; the error's `current_revision` tells you what to re-read.
- `header.dry_run` reports a change without applying it; `header.idempotency_key` makes a retry safe.
- A refused call ends with a gRPC status (`INVALID_ARGUMENT`, `NOT_FOUND`, `ABORTED` for a conflict, `PERMISSION_DENIED`, `RESOURCE_EXHAUSTED`, ...) and the serialized `resonant.v1.Error` in the trailing metadata `resonant-error-bin` (field violations, resource, current revision, retryable, request id).

## 5. A complete run

```text
OpenSession            -> session
CreateArray            {count_x: 8}
CreateFieldRegion      {kind: FIELD_REGION_KIND_PLANE_XY, count_u: 41, count_v: 31, local_transform.position_meters.z_meters: 0.05}
EstimateAcousticField  {region_id}                        -> plan.allowed, sample_points, memory_bytes
StartAcousticFieldSimulation {region_id}                  -> job
WaitForJob             {job_id, timeout_seconds: 30}      -> job.results[0].id when finished
GetResult              {result_id}                        -> grid, unit, provenance
ReadFieldSlice         {result_id, fixed_axis: SLICE_AXIS_W, stride: 1}   (stream of FieldDataChunk)
SaveProject            {file_name: "run.resonant"}        (files permission)
```

`FieldDataChunk.samples` holds `count` little-endian float32 pairs (real,
imaginary) in Pa, row-major over `rows` x `columns`, starting at `offset`.

## 6. Events

`EventService.SubscribeEvents{include_snapshot: true}` streams a scene
snapshot, then every event after it. Reconnect with
`after_sequence = <last sequence seen>`; a `gap` item reports events no
longer retained (then take a new snapshot). See [events and jobs](EVENTS_AND_JOBS.md).

## grpcurl

The bridge has no reflection; give grpcurl the descriptor set:

```bash
grpcurl -plaintext -protoset <build>/src/automation_api/resonant_api.desc -H "x-resonant-token: $(cat $XDG_RUNTIME_DIR/resonant/automation.token)" \
        127.0.0.1:50051 resonant.v1.ApplicationService/GetApplicationInfo
```
