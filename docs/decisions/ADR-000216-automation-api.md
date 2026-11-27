# ADR-000216: Automation API — a Qt-free application service layer, a proto3 contract with one adapter, a local endpoint in the application, gRPC through a separate bridge, one event stream, honest transactions and an MCP adapter on the same API

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/automation_api.md` RES-API-SVC-001..010, RES-API-EVT-001..004, RES-API-TX-001..003, RES-API-PB-001..005, RES-API-SEC-001..004, RES-API-GUI-001..003, RES-API-PY-001..002, RES-API-MCP-001, RES-API-PERF-001, RES-API-DOC-001
**Request:** REQ-CHAT-20261126-012
**Builds on:** ADR-000205 (layering), ADR-000215 (Python API, workflows, plugins)

## Context

Resonant had to be drivable by other programs, scripts and AI assistants
with the same operations, validation, undo and notifications as the window,
from several clients at once, without the GUI depending on any of it. Before
this change the GUI's `DocumentController` turned user actions into commands
itself, Python edited a project directly or applied a changed copy, and no
external interface existed. The audit is `docs/reference/AUTOMATION_AUDIT.md`.

## Decisions

1. **Application services in the core** (`src/core/services`), Qt-free and
   transport-free: one implementation per operation taking a `CallContext`
   (session, permissions, interface, request id, idempotency key, expected
   revision, dry run) and returning `Outcome<T>` with a structured error.
   `Session::mutate` is the single funnel for changes: authorize, replay an
   idempotency key, check expected revisions, dry-run on a copy, push one
   command, record the audit. The layer is a level above the domains: the
   domains never include it (layering rule).
2. **Revisions are counted, not inferred.** The session listens to the scene
   and the undo history: every committed change (including the GUI's own
   commands, undo, redo and direct Python edits) advances the project
   revision by one, objects record their last revision, and the same
   listeners publish the events. Material changes are detected from the
   library after each command, so undo and redo report them too.
3. **proto3 schemas are the contract; one adapter uses them.** Generated C++
   and a descriptor set come from `proto/` at build time (qmake and CMake;
   optional). `src/automation_api` (dispatcher, conversions, local endpoint)
   is the only user of generated types. Compatibility within v1 is checked
   against a recorded baseline by a project tool (Buf is not installed and
   was not added); the reference documentation is generated from the
   schemas' comments.
4. **The application hosts a local endpoint; gRPC is a separate bridge.**
   C++ gRPC (libgrpc++) is not installed here and adding it to the
   application would tie every build to it. The application (GUI or the
   headless host `resonant-automation`) listens on a Unix-domain socket
   (private directory, same user, a token); `python3 -m resonant_api.bridge`
   (grpcio) serves gRPC on 127.0.0.1 and relays every method as bytes,
   including server streams. Alternatives: libgrpc++ in the application
   (unavailable here, heavy dependency), HTTP/JSON (a second contract). A
   C++ gRPC server can later replace the bridge without changing the
   contract or the services.
5. **Calls run on the owner's thread.** The GUI marshals each call to the
   user-interface thread (`BlockingQueuedConnection`); the headless host
   serializes calls with a lock. A remote change is therefore exactly a GUI
   change. Streams wait on their own thread and visit the owner only to read;
   stopping the endpoint keeps the owner's events flowing until every
   connection ended (no deadlock).
6. **Sessions, permissions, policy.** Remote callers must open a session and
   receive at most the host's policy (default read, modify, execute; files
   only with a project directory; admin only when allowed). In-process
   callers act as the application's session. Not a sandbox, and documented
   as such. No method drives hardware outputs.
7. **Transactions only where they are true.** Batches and staged
   transactions are applied to a copy with the live revisions and committed
   as one undo step (`applyCopy`), refused on a moved base revision. They
   cover project changes only; files, jobs and hardware are out of scope and
   no universal rollback is claimed.
8. **Bounded data.** Field results are read as summaries, chunked slices
   with a stride (refused above a limit) or point samples; nothing sends a
   whole volume in one message.
9. **Python and MCP on the same contract.** `resonant_api` builds messages
   from the descriptor set (nothing generated is committed) and talks to the
   socket, gRPC or the in-process dispatcher in the native module. The MCP
   adapter derives tools and schemas from the contract, offers them by
   permission level and opens its own MCP session, so validation and audit
   are the same as for any client.
10. **The GUI goes through the services.** `DocumentController` hosts the
    session and routes its meaningful operations through it (keeping its
    undo texts); domain panels that set a whole object state still push a
    command directly, and their changes are seen by the session like any
    other.

## Consequences

- One behaviour for every interface; adding an operation means a service, a
  message, a handler and tests (`docs/development/ADDING_AUTOMATION_OPERATIONS.md`).
- gRPC clients need the bridge process (started from the GUI's dialog or by
  hand); the GUI never needs it.
- Mechanical, electromagnetic and full-wave runs are available in the
  application but not yet startable through the API; capabilities say so.

## Superseded or extended

Extends ADR-000215: Python keeps its `resonant` package and gains
`resonant_api`; workflows are also reachable through the API.
