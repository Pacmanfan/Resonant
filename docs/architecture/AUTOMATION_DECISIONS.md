# Automation Decisions

A reading guide to the choices behind the automation API. The record of the
decision is [ADR-000216](../decisions/ADR-000216-automation-api.md); this
page explains each choice with its alternatives.

| Question | Chosen | Alternatives considered | Why |
|---|---|---|---|
| Where do operations live? | Qt-free services in `src/core/services` | keep them in `DocumentController`; a new library | the GUI, Python, gRPC and MCP must share one implementation; the core is already Qt-free and tested without a display |
| How are changes counted? | listeners on the scene and the undo history | count only service calls | GUI commands, undo, redo and Python edits must advance revisions and produce events too |
| What is the contract? | proto3 in `proto/resonant/v1`, generated at build time | hand-written JSON over HTTP; FlatBuffers | the request asks for Protocol Buffers; typed, versioned, streaming, generated docs |
| Who sees generated types? | only `src/automation_api` | services returning messages | domains and services stay independent of the transport (layering rule) |
| How is gRPC served? | a grpcio bridge process relaying bytes | libgrpc++ in the application | `libgrpc++` and `grpc_cpp_plugin` are not installed and need administrator rights; a C++ server can replace the bridge later with the same contract |
| How does the application listen? | a Unix-domain socket, same user, a token | TCP on loopback | file permissions and peer credentials limit access to the user; no port is opened by the application itself |
| Which thread runs a call? | the owner's (GUI thread / host lock) | a thread per call with locks in the services | the project and its history are single-threaded; a remote change is then exactly a GUI change |
| Transactions? | staged on a copy, committed as one undo step, refused on a moved base | locks; undo-based rollback of anything | honest scope: project changes only, no claim of rolling back files, jobs or hardware |
| Large results? | summaries, chunked slices with a stride and a limit, point samples | whole fields in one message | bounded memory on both sides; gRPC message limits |
| Compatibility? | a project tool against a recorded baseline | Buf | Buf is not installed; the tool checks what matters (removals, renumbering, type, cardinality, name, streaming) |
| Python? | build messages from the descriptor set | commit generated `_pb2.py` | nothing generated is committed; one schema source |
| MCP? | an adapter deriving tools from the contract, by permission level | hand-written tools | the same validation, permissions and audit as any client; tools follow the contract |
| Permissions by default? | read, modify, execute for remote sessions; files and admin only when the host allows them | everything; read only | useful by default, file access only within one directory, administration explicit |
