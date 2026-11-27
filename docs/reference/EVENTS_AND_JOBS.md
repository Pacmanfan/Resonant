# Events and Jobs

(RES-API-EVT-001..004.) Methods and messages: [the protobuf reference](PROTOBUF_API.md).

## Events

One stream per application, ordered by a global `sequence` (1, 2, 3, ...).
Each event has an id (`EV-<sequence>`), time, `category`, `type`, the source
(session, client name, interface: GUI, Python, gRPC, MCP), the project, the
resources, the project revision and a typed payload.

| Category | Types | Payload |
|---|---|---|
| project | `project.changed` (every committed change, undo, redo), `project.created`, `project.opened`, `project.saved` | project id and name, the change's description |
| scene | `scene.object.created`, `.updated`, `.deleted`, `.moved`; `scene.reset` (the whole scene was replaced) | object id, type, parent, object revision |
| material | `material.changed` (the library changed, also by undo and redo) | the change's description |
| job | `job.queued`, `job.running`, `job.progress`, `job.completed`, `job.failed`, `job.cancelled` | the job's state, progress, phase, elapsed time |
| result | `result.created` | result id, kind, job |
| session | `session.opened`, `session.closed` | session id, client name |

The source of a change does not matter: a GUI command, an undo, a Python
edit and a client's call produce the same events. Job progress is published
at most every 5 % or 250 ms.

### Subscribing

`SubscribeEvents` (server streaming) with:

- `include_snapshot`: first a `SceneSnapshot` (every object, the project revision, the event sequence it describes), then every event after that sequence: no change can fall between the two.
- `after_sequence`: resume after the last sequence you saw (0 without a snapshot: from now).
- `filter`: categories, resource ids, a job, a session.

The application keeps the last 10 000 events. When some you asked for are
gone, the stream sends a `GapNotice` (first and last missed, oldest
retained): take a new snapshot. `ListEvents` returns the same events in
pages of at most 1000 for polling clients; `last_sequence` is where to resume
(a page never skips events, also when the filter matches few of them).

A subscription ends when the client goes away, its session closes or the
server stops.

## Jobs

Field simulations (`StartAcousticFieldSimulation`), workflows
(`ExecuteWorkflow`) and optimizations (`StartOptimization`) return at once
with a job; the work runs on the application's workers.

| State | Meaning |
|---|---|
| QUEUED | waiting for a worker |
| RUNNING | `progress_fraction` 0..1 and `phase` tell how far |
| CANCELLING | a cancel was asked (`CancelJob`); the job stops at its next check |
| CANCELLED, COMPLETED, FAILED | ended; `error` says why for the first and last |

`WaitForJob` blocks up to `timeout_seconds` (at most 600) and says whether
the job finished. A finished job's `results` name what it produced, once the
application has handled them (a field result is registered; a workflow's
changes are applied when asked; an optimization run is recorded in the
project). `GetWorkflowRun` gives a workflow's run record.

## Results

`GetResult` gives the quantity and unit (pressure, Pa, peak amplitude), the
grid (origin, unit axes, steps in meters, counts), the frequency, the
largest magnitude, and the provenance: the solver and its version, the
approximation (analytical free field or Kirchhoff physical optics: not
full-wave), the assumptions, the project revision the run started from and
the job. Read the data with `ReadFieldSlice` (a plane of the grid, a stride,
chunks of complex float32 pairs, refused above 4 194 304 samples) or
`ReadFieldSamples` (the nearest sample at given points; points outside the
grid are marked, not extrapolated). Results live while the application
runs; the project keeps the history of runs.
