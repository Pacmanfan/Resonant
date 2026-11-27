# MCP Integration

How an AI assistant uses Resonant through the Model Context Protocol
(RES-API-MCP-001). The adapter is `python/resonant_api/mcp.py`.

## Running it

```bash
# The application's automation server (GUI: Tools > Automation Server) or the headless host:
resonant-automation --socket "$XDG_RUNTIME_DIR/resonant/automation.sock" &
# An MCP server on stdin/stdout for the assistant (reading tools only):
PYTHONPATH=<share>/resonant/python python3 -m resonant_api.mcp
# Allow changes and jobs as well:
python3 -m resonant_api.mcp --allow modify,execute
# See what would be offered:
python3 -m resonant_api.mcp --allow modify --list-tools
```

An MCP client configuration names the command above; the adapter reads the
token file beside the socket.

## Mapping

| MCP | Resonant |
|---|---|
| server | one adapter = one session opened as `SOURCE_INTERFACE_MCP` with exactly the allowed permissions |
| tool | one contract method (e.g. `create_primitive` = `GeometryService.CreatePrimitive`) |
| tool description | the method's comment in `proto/` plus its permission level |
| input schema | JSON Schema generated from the request message (header removed; enums as names; maps and repeated fields) |
| call result | the response as JSON (proto field names) |
| tool error | the structured error: code, message, field violations, current revision, request id |

The tools: reading (application, capabilities, diagnostics, project, scene,
objects, geometry kinds, materials, emitters, arrays, solvers, field
estimates, results, samples, jobs, events, audit, workflows, optimization
problems and runs), modify (create, update, delete, duplicate, group,
materials, emitters, arrays, field regions, batches, undo, redo), execute
(field simulations, cancel, workflows, optimizations) and files (open,
save). Sessions, transactions and streams stay with programmatic clients.

## Safety for assistants

- Reading tools only unless `--allow` says otherwise.
- Every change tool takes `dry_run` (report without applying) and `idempotency_key` (a retried call applies once).
- Changes are undoable in the application and recorded in the audit trail as the MCP session's.
- There is no tool for hardware outputs, code execution or arbitrary files.

## Testing

`McpAdapter` needs no MCP SDK: `AutomationApiTests.test_mcp_tools_follow_the_permission_levels`
checks the offered tools, their schemas, refusals by level, dry runs, invalid
arguments and the audit records. `serve_stdio` uses the `mcp` package
(1.25 here) when present.
