# Automation API: Application Services, Protocol Buffers, gRPC, Events, Transactions and MCP

**Owner of:** `RES-API-SVC-001..010`, `RES-API-EVT-001..004`, `RES-API-TX-001..003`, `RES-API-PB-001..005`, `RES-API-SEC-001..004`, `RES-API-GUI-001..003`, `RES-API-PY-001..002`, `RES-API-MCP-001`, `RES-API-PERF-001`, `RES-API-DOC-001`
**Source:** REQ-CHAT-20261126-012 (Prompt 15, "Resonant Automation Framework")
**Refines:** `automation.md` (Python API, workflows: kept, now also reachable through the API), `application_foundation.md` (document, undo history, jobs)
**Decisions:** ADR-000216
**Documents:** `docs/architecture/AUTOMATION_ARCHITECTURE.md`, `docs/architecture/AUTOMATION_DECISIONS.md`, `docs/security/AUTOMATION_SECURITY.md`, `docs/architecture/MCP_INTEGRATION.md`, `docs/reference/AUTOMATION_AUDIT.md`, `docs/reference/PROTOBUF_API.md` (generated), `docs/reference/GRPC_CLIENT_GUIDE.md`, `docs/reference/PYTHON_AUTOMATION.md`, `docs/reference/EVENTS_AND_JOBS.md`, `docs/reference/TRANSACTIONS_AND_WORKFLOWS.md`, `docs/development/ADDING_AUTOMATION_OPERATIONS.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

## Application services

### RES-API-SVC-001 — One service layer for every interface

The meaningful operations on a project are implemented once, in
`src/core/services` (project, scene, geometry, materials, emitters, arrays,
simulation, jobs, results, optimization, workflows, transactions,
application). The GUI, in-process Python, the protobuf adapter (local socket
and gRPC) and the MCP adapter call this implementation; none has its own
copy. The layer includes no Qt, protobuf or gRPC header, and nothing else in
the core includes it.
*Verification:* `Services.*`; `AutomationApi.TheDispatcherImplementsExactlyTheContractsMethods`; `tools/check_layering.py` (layering suite).

### RES-API-SVC-002 — Stable resource identifiers

Objects, materials, jobs, results, sessions and transactions are named by
stable ids (UUIDs for objects and materials, `S-`, `R-`, `T-` prefixed ids
for sessions, results and transactions), never by an index or a position.
*Verification:* `Services.CreateUpdateDeleteWithRevisionsUndoAndEvents`.

### RES-API-SVC-003 — Revisions and optimistic concurrency

The project revision advances by exactly one per committed change, also for
the GUI's own commands, undo, redo and direct (non-command) edits made by
Python; each object records the revision at which it last changed. A
mutation given an expected project or object revision that no longer holds
is refused with `CONFLICT` and the current revision; nothing is overwritten
silently.
*Verification:* `Services.CreateUpdateDeleteWithRevisionsUndoAndEvents`, `Services.ChangesMadeOutsideTheServicesAdvanceRevisionsAndProduceEvents`; `AutomationApiTests.test_conflicting_edits_from_two_clients`.

### RES-API-SVC-004 — Structured errors

A refused call reports a code (invalid argument, not found, already exists,
conflict, failed precondition, permission denied, resource exhausted,
unimplemented, unavailable, cancelled, internal), a message, the violated
fields, the resource, the current revision for a conflict, whether a retry
can succeed, and the request id.
*Verification:* `AutomationApi.UnaryCallsRoundTripAndErrorsAreStructured`; `AutomationApiTests.test_crud_over_both_transports_with_structured_errors`.

### RES-API-SVC-005 — Undoable mutations, no empty steps, dry runs

Every project change made through a service is one command on the
application's undo history (whoever made it, the GUI can undo it); a call
that changes nothing adds no step and no revision; a dry run applies the
change to a copy and reports it, changing nothing.
*Verification:* `Services.CreateUpdateDeleteWithRevisionsUndoAndEvents`, `Services.DryRunsChangeNothingAndIdempotencyKeysReplay`; `AutomationServerTest.ARemoteChangeIsAGuiChange`.

### RES-API-SVC-006 — Idempotent retries

A mutation repeated with the same idempotency key (per session) returns the
first outcome, marked as replayed, without applying it again; the same key
for another operation is refused. The last 1000 keys are kept.
*Verification:* `Services.DryRunsChangeNothingAndIdempotencyKeysReplay`; `AutomationApiTests.test_retries_with_an_idempotency_key_apply_once`.

### RES-API-SVC-007 — Sessions and permissions

Remote callers open a session (client name, interface) and hold the
permissions they request within the host's policy: read, modify, execute
(jobs), files (open / save within one project directory) and admin (all
sessions, all audit records). The default remote policy is read, modify and
execute. In-process callers (the GUI, Python in the process) act as the
application's session. A remote caller cannot use the application's session
or claim to be the GUI or Python.
*Verification:* `Services.SessionsAndPermissionsAreEnforced`; `AutomationApi.RemoteSessionsGetNoMoreThanThePolicy`; `AutomationApiTests.test_sessions_hold_only_what_they_asked_for_within_the_policy`.

### RES-API-SVC-008 — Audit trail

Every mutation and every refused mutation is recorded (time, request,
session, client, interface, operation, resources, revisions before and
after, outcome, duration) in a bounded trail; a session reads its own
records, an admin session all of them.
*Verification:* `Services.SessionsAndPermissionsAreEnforced`; `AutomationServerTest.AWindowChangeReachesClientsAsAnEventFromTheGui`; `AutomationApiTests.test_mcp_tools_follow_the_permission_levels`.

### RES-API-SVC-009 — Capabilities as they are

`GetCapabilities` lists the domains, solvers, geometry kinds, material
models, optimization algorithms, file formats, optional dependencies and
features of this build, with availability and limitations: what exists in
the application but is not yet reachable through the API is listed as such,
what is missing here (CUDA, HDF5, VOLK) as unavailable, and hardware outputs
as never available. It lists every operation with its permission, whether it
mutates and whether it is safe to retry.
*Verification:* `Services.WorkflowsCapabilitiesAndDiagnostics`; `AutomationApiTests.test_capabilities_list_every_operation_and_what_is_not_available`.

### RES-API-SVC-010 — Diagnostics

`GetDiagnostics` reports health, the services, open sessions, active jobs,
the event and audit counters, recent errors, resident memory and uptime.
*Verification:* `Services.WorkflowsCapabilitiesAndDiagnostics`.

## Events and jobs

### RES-API-EVT-001 — One ordered event stream for every change

Project, scene, material, job, result and session changes are events with a
global sequence number, time, the causing session, client and interface,
the resources, the project revision and a typed payload, whatever made the
change (a client, the GUI, undo, a Python edit). Job progress is throttled
(5 % or 250 ms).
*Verification:* `Services.ChangesMadeOutsideTheServicesAdvanceRevisionsAndProduceEvents`, `Services.MaterialChangesProduceEventsAlsoOnUndoAndRedo`; `AutomationApi.ConcurrentClientsAreSerializedWithoutLostUpdates`; `AutomationServerTest.AWindowChangeReachesClientsAsAnEventFromTheGui`.

### RES-API-EVT-002 — Subscriptions that resume and say what they missed

A subscription may start with a scene snapshot (and then carries the events
after the snapshot's sequence), or resume after any sequence; events no
longer retained (10 000 are kept) are reported as a gap; filters by
category, resource, job and session apply; a page of events never skips any.
A subscription ends when its client goes away, its session closes or the
server stops.
*Verification:* `Services.EventPagesResumeWithoutLosingEvents`; `AutomationApiTests.test_many_clients_at_once_lose_nothing_and_see_one_order`, `test_a_subscription_resumes_after_a_sequence`; `AutomationServerTest.TheDialogListsSessionsAndStopsWithAStreamOpen`.

### RES-API-EVT-003 — Jobs

Field simulations, workflows and optimizations started through the API run
as jobs of the application's job manager with states (queued, running,
cancelling, cancelled, completed, failed), progress, phase, elapsed time,
error and results; they can be listed, waited for (at most 600 s per call)
and cancelled; what a finished job produces is handled on the owner thread.
*Verification:* `Services.AFieldJobProducesAResultReadInBoundedSlices`, `Services.WorkflowsCapabilitiesAndDiagnostics`; `AutomationApiTests.test_a_job_can_be_waited_for_and_cancelled_jobs_say_so`.

### RES-API-EVT-004 — Results in bounded pieces

A result's summary states its quantity, unit (Pa, peak amplitude), value
type, grid (origin, unit axes, steps in meters, counts), frequency, maximum
and provenance (solver, version, approximation, assumptions, project
revision, job). Field slices are streamed in chunks of complex float32
pairs, reduced by a stride, and refused with `RESOURCE_EXHAUSTED` above
4 194 304 samples; point samples give the nearest sample and mark points
outside the grid instead of extrapolating.
*Verification:* `Services.AFieldJobProducesAResultReadInBoundedSlices`; `AutomationApi.FieldSlicesStreamInChunks`; `AutomationApiTests.test_a_field_job_streams_its_result`.

## Transactions and workflows

### RES-API-TX-001 — Atomic batches

A batch of project changes (create, update, delete, assign, emitters,
arrays, focusing, field regions) is applied all together as one undo step,
or not at all when one fails; each operation reports its outcome, the later
ones "not run". A dry run validates and reports the plan.
*Verification:* `Services.BatchesAreAtomicAndTransactionsDetectConflicts`; `AutomationApi.BatchesThroughTheContractAreAtomic`.

### RES-API-TX-002 — Transactions, honestly scoped

A transaction is begun on a revision, staged, validated on a copy and
committed as one undo step; a commit after the project moved on is refused
with `CONFLICT`. Transactions cover project changes only: files, jobs and
hardware are not rolled back, and no universal rollback is claimed.
*Verification:* `Services.BatchesAreAtomicAndTransactionsDetectConflicts`; `AutomationApiTests.test_transactions_commit_once_or_detect_the_conflict`.

### RES-API-TX-003 — Workflows through the API

Workflow templates (built in and from plugins) are listed with their
definitions and run as jobs on a copy of the project; with "apply changes"
(modify permission) a completed run's changes become one undo step recorded
in the project's automation history. Files a workflow writes stay in a run
folder on the application's side.
*Verification:* `Services.WorkflowsCapabilitiesAndDiagnostics`; `AutomationApiTests.test_a_job_can_be_waited_for_and_cancelled_jobs_say_so`.

## Protocol Buffers contract

### RES-API-PB-001 — proto3 schemas are the public contract

`proto/resonant/v1` (package `resonant.v1`) defines every message and RPC:
common types (identifiers, headers, errors, spatial types in SI units with
the unit in the field name, pagination, validation, jobs, events) and the
services. Every request starts with a header (session, request id,
idempotency key, expected project revision, dry run) and every response with
one (request id, project revision, warnings, dry run, replayed).
*Verification:* `AutomationApi.TheDispatcherImplementsExactlyTheContractsMethods`; `SchemaGuardTests.test_every_method_and_service_is_documented`.

### RES-API-PB-002 — Generated at build time, optional

The C++ code and a descriptor set (with comments) are generated from
`proto/` by protoc in both builds (qmake `protobuf.pri`, CMake
`RESONANT_WITH_AUTOMATION_API`); nothing generated is committed. Without
protoc and libprotobuf the application builds and runs without the API.
*Verification:* the build (qmake and CMake suites).

### RES-API-PB-003 — One adapter, exactly the contract

`src/automation_api` is the only code that uses generated types: a
dispatcher that decodes, calls the services and encodes, for exactly the
methods of the contract, and the local endpoint. Domain types never see
protobuf.
*Verification:* `AutomationApi.TheDispatcherImplementsExactlyTheContractsMethods`; `tools/check_layering.py`.

### RES-API-PB-004 — Compatibility within v1

Within `resonant.v1` the contract only grows: removing or renumbering
anything, changing a field's type, cardinality or name, reusing a reserved
number or changing a method's types or streaming is refused by
`tools/automation/check_schema_compatibility.py` against the released
baseline (`proto/baseline/resonant_v1.json`). A breaking change needs
`resonant.v2` beside v1.
*Verification:* `SchemaGuardTests.test_the_contract_is_compatible_with_its_baseline`, `test_breaking_changes_are_found`.

### RES-API-PB-005 — Reference generated from the schemas

`docs/reference/PROTOBUF_API.md` is generated from the schemas and their
comments; every method and service is documented; the checked-in reference
must match the contract.
*Verification:* `SchemaGuardTests.test_the_protobuf_reference_is_generated_from_the_current_contract`, `test_every_method_and_service_is_documented`.

## Transport and security

### RES-API-SEC-001 — The local endpoint

The application (GUI or headless host) listens on a Unix-domain socket in a
directory private to the user (0700, socket 0600), accepts only peers of the
same user (SO_PEERCRED), requires the endpoint's 256-bit token on every call
(compared in constant time), bounds frames (64 MiB) and connections (64),
and reaps finished connection threads.
*Verification:* `AutomationApi.TheLocalEndpointAdmitsOnlyTheTokenHolder`.

### RES-API-SEC-002 — The gRPC transport

The gRPC bridge (`python3 -m resonant_api.bridge`, grpcio) relays every
method, unary and server streaming, to the local endpoint as bytes; it binds
127.0.0.1 unless a non-loopback address is explicitly allowed (the channel
is not encrypted), requires the token in the `x-resonant-token` metadata,
and ends refused calls with the error's gRPC status and the serialized error
in the `resonant-error-bin` trailing metadata.
*Verification:* `AutomationApiTests.*` (over gRPC), `test_crud_over_both_transports_with_structured_errors`.

### RES-API-SEC-003 — The headless host

`resonant-automation` hosts one project, the services and the local endpoint
without a window; it writes its token to a 0600 file (never prints it),
grants files only with a project directory and admin only when told, and
cancels its jobs when it stops.
*Verification:* `AutomationApiTests` (every test runs against it).

### RES-API-SEC-004 — No sandbox claimed, no outputs, bounded files

A client holding the token acts on the project with its session's
permissions; this is documented as not a sandbox. No method drives hardware
outputs. Remote file access is limited to names inside the host's project
directory (no absolute paths, no `..`) and needs the files permission; files
written for clients stay on the application's side.
*Verification:* `Services.SessionsAndPermissionsAreEnforced`; `AutomationApiTests.test_sessions_hold_only_what_they_asked_for_within_the_policy`.

## GUI

### RES-API-GUI-001 — The GUI uses the services

The document controller hosts the session; creating primitives, renaming,
moving, showing and hiding, reparenting, geometry parameters, material
assignment, the material library, deleting, duplicating, undo, redo, saving
and acoustic field runs go through the services as the application's
session (interface GUI), with the GUI's own undo texts.
*Verification:* `AutomationServerTest.AWindowChangeReachesClientsAsAnEventFromTheGui`; the app suite.

### RES-API-GUI-002 — The automation server in the GUI

Tools > Automation Server starts and stops the local endpoint on the open
project (off until started), sets what remote sessions may do, starts the
gRPC bridge, shows the socket and token file (home as `~`), copies the
token and lists the sessions. Remote calls run on the user-interface thread,
so a remote change is a GUI change; stopping with calls or streams open
completes without hanging.
*Verification:* `AutomationServerTest.*`.

### RES-API-GUI-003 — No dependency of the GUI on the API

The application works without the automation server running and without
the automation API built.
*Verification:* the app suite (the server is not started there except by `AutomationServerTest`).

## Python and MCP

### RES-API-PY-001 — A Python client of the contract

`resonant_api` builds its messages from the descriptor set, opens a
session, fills headers, raises `ApiError` with the structured error, and
offers `subscribe`, `wait_for_job` and `read_field_slice` (NumPy) over the
local socket, gRPC or in-process; the `resonant` package (API 1.0) is kept.
*Verification:* `AutomationApiTests.*`.

### RES-API-PY-002 — The same contract in process

The native module's `AutomationSession` dispatches the contract on a
`resonant.Project` in the Python process (no socket), with the project's
lock held and the GIL released; direct edits of the project still produce
revisions and events.
*Verification:* `AutomationApiTests.test_the_same_contract_in_process`.

### RES-API-MCP-001 — An MCP adapter on the same API

`resonant_api.mcp` offers chosen methods as MCP tools with descriptions and
JSON schemas from the contract, by permission level (read by default;
modify, execute and files only when allowed), with dry runs and idempotency
keys for changes; its session is opened as an MCP client with exactly those
permissions, so validation and audit apply; it runs over stdio with the
`mcp` package and is tested without one.
*Verification:* `AutomationApiTests.test_mcp_tools_follow_the_permission_levels`.

## Performance and documentation

### RES-API-PERF-001 — Measured, and no serialization in the GUI

Call latency over the local socket and over gRPC is measured and recorded
in the evidence; the GUI calls the services directly (no serialization).
*Verification:* `AutomationApiTests.test_call_latency`.

### RES-API-DOC-001 — Documentation

The documents listed above exist and are current; the developer guide
explains how to add an operation without creating a GUI-only function.
*Verification:* `scripts/doc_integrity.py`; review.
