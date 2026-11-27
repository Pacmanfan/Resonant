"""The gRPC transport of the automation API (REQ-CHAT-20261126-012, ADR-000216).

    python3 -m resonant_api.bridge [--socket PATH] [--token-file PATH] [--port N]

A gRPC server (grpcio) that relays every resonant.v1 method, as bytes, to the
application's local endpoint (the GUI's automation server or resonant-automation).
It decodes nothing it does not need: the application validates every request.
Server streaming carries SubscribeEvents and ReadFieldSlice. A refused call
ends with the gRPC status of the error and the serialized resonant.v1.Error in
the trailing metadata key "resonant-error-bin".

Security: it binds 127.0.0.1 unless told otherwise, and every call must carry
the application's token in the "x-resonant-token" metadata. The channel is
not encrypted: binding another address (--host) needs --allow-non-loopback and
is meant for a network you trust or an SSH tunnel. This is not a sandbox: a
client with the token acts with the permissions of the sessions it opens.
"""

from __future__ import annotations

import argparse
import hmac
import json
import signal
import sys
import threading
from concurrent import futures
from typing import Optional

import grpc

from .contract import Contract, load_contract
from .transports import ERROR_METADATA_KEY, TOKEN_METADATA_KEY, LocalTransport, default_socket_path, read_token

_STATUS = {s.value[0]: s for s in grpc.StatusCode}


def _token_of(context) -> str:
    for key, value in context.invocation_metadata():
        if key == TOKEN_METADATA_KEY:
            return value
        if key == "authorization" and value.lower().startswith("bearer "):
            return value[7:]
    return ""


class Bridge:
    """The relaying gRPC server."""

    def __init__(self, contract: Contract, local: LocalTransport, token: str):
        self.contract = contract
        self.local = local
        self.token = token
        self.calls = 0
        self._lock = threading.Lock()

    def _authorized(self, context) -> bool:
        return hmac.compare_digest(_token_of(context).encode(), self.token.encode())

    def _fail(self, context, status: int, error: bytes):
        message = ""
        if error:
            e = self.contract.new("resonant.v1.Error")
            e.ParseFromString(error)
            message = e.message
            context.set_trailing_metadata(((ERROR_METADATA_KEY, error),))
        context.abort(_STATUS.get(status, grpc.StatusCode.UNKNOWN), message or "refused")

    def _count(self):
        with self._lock:
            self.calls += 1

    def unary(self, method: str):
        def handle(request: bytes, context):
            if not self._authorized(context):
                context.abort(grpc.StatusCode.UNAUTHENTICATED, "a valid x-resonant-token is needed")
            self._count()
            try:
                status, payload, error = self.local.raw_unary(method, request)
            except Exception as e:  # the application is gone
                context.abort(grpc.StatusCode.UNAVAILABLE, f"the application is not reachable: {e}")
            if status != 0:
                self._fail(context, status, error)
            return payload

        return handle

    def streaming(self, method: str):
        def handle(request: bytes, context):
            if not self._authorized(context):
                context.abort(grpc.StatusCode.UNAUTHENTICATED, "a valid x-resonant-token is needed")
            self._count()
            frames = self.local.raw_stream(method, request, timeout=None)
            try:
                for is_item, value in frames:
                    if not context.is_active():
                        return
                    if is_item:
                        yield value
                    else:
                        status, error = value
                        if status != 0:
                            self._fail(context, status, error)
                        return
            finally:
                frames.close()  # closes the socket: the application ends the stream

        return handle

    def handlers(self):
        by_service = {}
        for method in self.contract.methods.values():
            if not method.service.startswith("resonant.v1."):
                continue
            if method.server_streaming:
                h = grpc.unary_stream_rpc_method_handler(self.streaming(method.name))
            else:
                h = grpc.unary_unary_rpc_method_handler(self.unary(method.name))
            by_service.setdefault(method.service, {})[method.short_name] = h
        return [grpc.method_handlers_generic_handler(service, methods) for service, methods in by_service.items()]


def serve(
    socket_path: Optional[str] = None,
    token: Optional[str] = None,
    token_file: Optional[str] = None,
    host: str = "127.0.0.1",
    port: int = 0,
    allow_non_loopback: bool = False,
    max_workers: int = 32,
    contract: Optional[Contract] = None,
):
    """Starts the bridge; returns (server, port). Stop it with server.stop(grace)."""
    if host not in ("127.0.0.1", "::1", "localhost") and not allow_non_loopback:
        raise ValueError("the bridge binds the loopback interface unless allow_non_loopback is set (the channel is not encrypted)")
    contract = contract or load_contract()
    path = socket_path or default_socket_path()
    token = token if token is not None else read_token(path, token_file)
    bridge = Bridge(contract, LocalTransport(contract, path, token, timeout=None), token)
    server = grpc.server(
        futures.ThreadPoolExecutor(max_workers=max_workers),
        handlers=bridge.handlers(),
        options=[("grpc.max_receive_message_length", 64 << 20), ("grpc.max_send_message_length", 64 << 20)],
    )
    address = f"[{host}]:{port}" if ":" in host else f"{host}:{port}"
    bound = server.add_insecure_port(address)
    if bound == 0:
        raise OSError(f"the bridge could not bind {address}")
    server.start()
    server.bridge = bridge
    return server, bound


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="python3 -m resonant_api.bridge", description=__doc__.split("\n\n")[0])
    parser.add_argument("--socket", help="the application's local endpoint (default: the per-user runtime directory)")
    parser.add_argument("--token-file", help="the token file (default: beside the socket); RESONANT_AUTOMATION_TOKEN overrides")
    parser.add_argument("--host", default="127.0.0.1", help="the address to bind (default 127.0.0.1)")
    parser.add_argument("--port", type=int, default=50051, help="the port (0: any free port)")
    parser.add_argument("--allow-non-loopback", action="store_true", help="allow binding a non-loopback address (unencrypted)")
    args = parser.parse_args(argv)
    try:
        server, port = serve(args.socket, None, args.token_file, args.host, args.port, args.allow_non_loopback)
    except (OSError, ValueError) as e:
        print(f"resonant_api.bridge: {e}", file=sys.stderr)
        return 1
    print(json.dumps({"grpc": f"{args.host}:{port}"}), flush=True)
    stop = threading.Event()
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    stop.wait()
    server.stop(2).wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())
