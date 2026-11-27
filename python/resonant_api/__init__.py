"""resonant_api: the Resonant automation API for Python (REQ-CHAT-20261126-012).

A client of the resonant.v1 protobuf contract over the application's local
socket, gRPC (the bridge) or in-process, plus the gRPC bridge itself and the
MCP adapter. It needs only the `protobuf` package (and `grpcio` for gRPC);
message classes come from the contract's descriptor set.

    from resonant_api import connect
    with connect(client_name="my script") as api:
        print(api.ProjectService.GetProject().project.name)

See docs/reference/GRPC_CLIENT_GUIDE.md and docs/reference/PYTHON_AUTOMATION.md.
"""

from .client import Client, connect, to_dict
from .contract import Contract, Method, load_contract
from .transports import ApiError, GrpcTransport, InProcessTransport, LocalTransport, default_socket_path, read_token

__all__ = [
    "ApiError", "Client", "Contract", "GrpcTransport", "InProcessTransport", "LocalTransport", "Method",
    "connect", "default_socket_path", "load_contract", "read_token", "to_dict",
]
