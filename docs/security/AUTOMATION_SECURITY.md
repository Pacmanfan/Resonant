# Automation Security

What protects a Resonant project when the automation API is on
(RES-API-SEC-001..004, RES-API-SVC-007..008), and what does not.

**This is not a sandbox.** A client holding the token acts on the open
project with the permissions of the session it opened. Scripts and plugins
remain executable code with the user's rights (see the plugin documentation).

## Who can connect

- **Off by default.** The GUI's server runs only after Tools > Automation Server > Start Server; the headless host only when started.
- **Local socket.** `$XDG_RUNTIME_DIR/resonant/automation.sock` by default (else a per-user folder in the temporary directory). The directory is created 0700 and must belong to the user; the socket is 0600; a stale socket is replaced, any other file at the path is left alone and refused.
- **Same user only.** Each connection's peer credentials (SO_PEERCRED) must match the application's user.
- **A token.** 256 random bits from the system's random source, written to `automation.token` beside the socket with mode 0600, never printed or logged. Every request carries it; it is compared in constant time. `RESONANT_AUTOMATION_TOKEN` sets it explicitly (tests, the bridge started by the GUI).
- **gRPC bridge.** Binds 127.0.0.1 by default and requires the token in the `x-resonant-token` metadata. The channel is not encrypted: another address needs `--allow-non-loopback` and belongs on a network you trust or behind an SSH tunnel.
- **Limits.** Request frames up to 64 MiB, at most 64 connections, field slices up to 4 194 304 samples, 100 000 points per sample request, batches of 10 000 operations, waits of 600 s.

## What a session may do

| Permission | Allows | Default for remote sessions |
|---|---|---|
| read | queries, events, results | yes |
| modify | project changes (each one undoable) | yes (the GUI's dialog can turn it off) |
| execute | starting and cancelling jobs | yes (can be turned off) |
| files | opening and saving projects **inside the host's project directory** (no absolute paths, no `..`) | only when the host sets a project directory and allows it |
| admin | listing every session, reading every session's audit records | only when allowed |

A session asks for permissions and gets at most the policy. A remote client
cannot use the application's own session and cannot claim to be the GUI or
Python (it may declare itself an MCP adapter, which only labels its records).

## What no session can do

- Drive **hardware outputs**: the API has no such method; outputs need the operator's authorization in the Experiments panel (ADR-000213).
- Run arbitrary Python or native code: the API has no method for it; plugins load only from the user's explicit configuration.
- Write files outside the project directory: workflow output stays in a run folder on the application's side and is not returned as paths.

## Accountability

Every mutation and refused mutation is in the audit trail with the session,
client, interface, operation, resources, revisions and outcome; events name
their source session. The GUI's dialog lists the sessions with their
permissions and last call.

## Paths shown

The dialog shows the socket and token file with the home directory as `~`;
documentation and screenshots use no machine paths.
