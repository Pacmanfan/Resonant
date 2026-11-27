---
id: py.automation-server
title: Automation Server
category: Python and Automation
order: 70
status: available
summary: Let scripts, other programs and AI assistants drive the open project through the automation API, with the same undoable, validated operations as the window.
keywords: [automation, api, grpc, protobuf, mcp, server, remote, token, session, socket, client]
related: [py.api, py.workflows, py.plugins]
---

The automation server lets other programs work on the project that is open
in the window: a Python script, a program in another language over gRPC, or
an AI assistant through MCP. They use the **same operations as the window**:
every change is validated, can be undone with **Edit > Undo**, and appears in
the scene and the panels at once.

Open it with **Tools > Automation Server**. The server is **off** until you
start it.

## Starting the server

1. Choose what connected programs may do:
   - **Change the project**: create, edit and delete objects, materials, emitters and arrays. Each change can be undone.
   - **Start and cancel jobs**: field simulations, optimizations and workflows.
   - **Open and save projects in** a directory you choose. Programs can use only names inside that directory.
   - **Administer**: see every session and every session's records.
2. Click **Start Server**. The dialog shows the socket and the token file.
3. Give the program the token: it reads the token file, or you click **Copy Token**.

Programs can always read the project, its results and its events. **No
program can drive hardware outputs**: outputs always need your authorization
in the Experiments panel ([Safety](../experiments/safety.md)).

> **Warning** The server is not a sandbox. A program that has the token can
> change the open project within the permissions you chose. Only your own
> user account can connect, and only with the token.

## gRPC

Programs in other languages connect over gRPC. With the server running, set
a port and click **Start Bridge**. The bridge listens on 127.0.0.1 only. The
gRPC client guide in the developer documentation describes the calls.

## Sessions

The table lists the connected programs: their name, how they connect (gRPC,
MCP, Python), their permissions and when they last called. Their changes
appear in the undo history under their own names.

## Stopping

Click **Stop Server**. Open connections end and the token is no longer
valid. Closing Resonant stops the server too.

## From Python

```python
from resonant_api import connect
with connect(client_name="my script") as api:
    box = api.GeometryService.CreatePrimitive(kind_id="box", parameters={"width": 0.1})
    print(box.object.name, box.header.project_revision)
```

See [the Python API](api.md) for the in-process `resonant` package.
