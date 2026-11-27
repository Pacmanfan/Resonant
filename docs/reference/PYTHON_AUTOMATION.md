# Python Automation

`resonant_api` is the Python client of the automation API (RES-API-PY-001..002).
It complements the `resonant` package (the in-process Python API, see
[the Python API](../python/PYTHON_API.md)): `resonant` works on a project in
your process, `resonant_api` talks the same contract as every other client,
to a running application or in process.

Needs: `protobuf` (and `grpcio` for gRPC, `numpy` for field slices). Message
classes come from the descriptor set (`RESONANT_API_DESCRIPTORS`, the
installation, or `protoc` with a source checkout); nothing generated is
committed.

## Connecting

```python
from resonant_api import connect, ApiError, to_dict

api = connect(client_name="sweep")                                   # the local socket (token from the token file)
api = connect(grpc_target="127.0.0.1:50051", client_name="sweep")    # through the gRPC bridge
api = connect(project=resonant.Project("p"), client_name="local")    # in this process (native module with the API)
api = connect(client_name="viewer", permissions=["read"])            # ask for less
```

`connect` opens a session; `api.close()` (or `with connect(...) as api:`) closes it.

## Calls

Services are attributes, methods take keyword fields (proto field names or
JSON names; nested messages as dicts; enums by name) or a message:

```python
made = api.GeometryService.CreatePrimitive(kind_id="cylinder", parameters={"radius": 0.05, "height": 0.1},
                                           local_transform={"position_meters": {"z_meters": 0.1}})
obj = made.object
api.SceneService.UpdateObject(object_id=obj.object_id, name="Housing", expected_revision=obj.revision)
plan = api.GeometryService.CreatePrimitive(kind_id="box", header={"dry_run": True})       # nothing changes
api.GeometryService.CreatePrimitive(kind_id="box", header={"idempotency_key": "box-1"})   # safe to retry
print(to_dict(api.ProjectService.GetProject().project))
```

A refused call raises `ApiError` with `code` (e.g. `"CONFLICT"`), `message`,
`violations` [(field, description)], `current_revision`, `retryable`,
`request_id`.

## Conveniences

```python
job = api.wait_for_job(job_id, timeout_seconds=120)              # WaitForJob until it ends
field = api.read_field_slice(result_id, stride=2)                # complex64 NumPy array (rows, columns), Pa
for item in api.subscribe(categories=["EVENT_CATEGORY_SCENE"], include_snapshot=True):
    if item.WhichOneof("item") == "event":
        print(item.event.sequence, item.event.type, item.event.source.client_name)
```

## The gRPC bridge and MCP from Python

```python
from resonant_api.bridge import serve
server, port = serve(socket_path, token, port=0)      # a bridge in this process (tests, tools)
from resonant_api.mcp import open_adapter
tools = open_adapter(allow=("modify",)).tools()       # what an assistant would see
```

## In process versus the `resonant` package

`connect(project=p)` runs the contract's dispatcher in the native module on
`p`: the same validation, revisions, events and audit as remote clients,
without a socket. `SubscribeEvents` needs a socket or gRPC; in process, poll
`EventService.ListEvents`. Edits made with the `resonant` package on the
same project still produce revisions and events.

Examples: `python/examples/` (the `resonant` package); the automation tests
`python/tests/test_automation_api.py` show every feature against a real host.
