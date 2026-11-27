# Adding an Automation Operation

How to add a capability to Resonant so that the window, Python, gRPC and MCP
all get it from one implementation (RES-API-SVC-001, ADR-000216). **Do not
add an operation to a panel or to `DocumentController` alone**: a meaningful
operation that exists only in the GUI is a defect.

## Checklist

1. **The service** (`src/core/services`). Add a method to the right service
   in `Services.h`, taking a `CallContext` and returning `Outcome<T>`. A
   change goes through `s_.mutate(context, "Service.Method", resources,
   expectedRevision, builder)`: the builder returns the command (or none when
   there is nothing to change), validates its input and reports errors with
   `invalid(message, field)`, `notFound(...)` or `ServiceError::make(code, ...)`.
   A read starts with `s_.authorize(context, Permission::Read)`. A job: submit
   to `s_.host().jobs()` and `s_.trackJob(...)` with what to do when it ends.
   No Qt, protobuf or gRPC here; no physics either (call the domain).
2. **The contract** (`proto/resonant/v1/<area>/<area>_service.proto`).
   Add `rpc Method(MethodRequest) returns (MethodResponse);` with a comment
   (the comment becomes the reference and the MCP tool description), a request
   starting with `RequestHeader header = 1;` and a response starting with
   `ResponseHeader header = 1;`. Units in field names (`_meters`, `_radians`,
   `_hz`, `_seconds`, `_pa`); `optional` for patch fields; new field numbers
   only. Run `python3 tools/automation/check_schema_compatibility.py`.
3. **The adapter** (`src/automation_api/Handlers.cpp`). Register it with
   `unary<Request, Response>("Service", "Method", permission, mutating,
   idempotent, handler)`; the handler converts, calls the service and fills
   the response (`detail::mutated(header, mutation)` for changes). Batchable
   changes also get an `apply...` function and a `BatchOperation` case.
4. **Capabilities.** The method appears in `GetCapabilities.operations`
   automatically; add a `CapabilityView` in `ApplicationServices.cpp` when it
   adds a solver, format or feature, with its real availability.
5. **The GUI.** Make the window's action call the service
   (`document.services().<service>.<method>(DocumentController::guiContext(), ...)`);
   keep only presentation (dialogs, selection) in the GUI.
6. **Python.** Nothing for `resonant_api` (it follows the contract); add a
   `resonant` package function only if the in-process API needs it. For MCP,
   add an entry to `TOOLS` in `python/resonant_api/mcp.py` with its level.
7. **Tests.** A service test in `tests/core/test_services.cpp`; the contract
   test (`AutomationApi.TheDispatcherImplementsExactlyTheContractsMethods`)
   fails until the handler exists; an end-to-end case in
   `python/tests/test_automation_api.py`; a GUI test when the window uses it.
8. **Documentation.** Regenerate the reference
   (`python3 tools/automation/generate_protobuf_docs.py`), add the requirement
   to `docs/requirements/automation_api.md`, update the guides when behaviour
   changes, and record the baseline at release
   (`check_schema_compatibility.py --update-baseline`).

## Guards that catch omissions

- The dispatcher must implement exactly the contract's methods (contract test).
- Every method and service must have a comment, and the reference must match (schema tests).
- Within v1 nothing may be removed or changed incompatibly (compatibility test).
- The services may not include transport headers, and domains may not include the services (layering).
