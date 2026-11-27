"""How a Client reaches the application (REQ-CHAT-20261126-012).

Every transport moves the same serialized resonant.v1 messages:

* LocalTransport: the application's Unix-domain socket (the GUI's automation
  server or the headless host resonant-automation), one call per connection.
* GrpcTransport: a gRPC channel to the bridge (python3 -m resonant_api.bridge).
* InProcessTransport: the dispatcher inside this Python process, around a
  resonant.Project (the native module built with the automation API).

A failed call raises ApiError with the structured resonant.v1.Error.
"""

from __future__ import annotations

import os
import pathlib
import socket
import struct
from typing import Iterator, Optional, Tuple

from .contract import Contract

# gRPC status codes by number (the transports' common language).
STATUS_NAMES = {
    0: "OK", 1: "CANCELLED", 2: "UNKNOWN", 3: "INVALID_ARGUMENT", 4: "DEADLINE_EXCEEDED", 5: "NOT_FOUND",
    6: "ALREADY_EXISTS", 7: "PERMISSION_DENIED", 8: "RESOURCE_EXHAUSTED", 9: "FAILED_PRECONDITION",
    10: "ABORTED", 11: "OUT_OF_RANGE", 12: "UNIMPLEMENTED", 13: "INTERNAL", 14: "UNAVAILABLE",
    15: "DATA_LOSS", 16: "UNAUTHENTICATED",
}

ERROR_METADATA_KEY = "resonant-error-bin"
TOKEN_METADATA_KEY = "x-resonant-token"


class ApiError(Exception):
    """A refused or failed call, with the application's structured error."""

    def __init__(self, status: int, error=None, message: str = ""):
        self.status = status
        self.status_name = STATUS_NAMES.get(status, str(status))
        self.error = error
        if error is not None:
            from google.protobuf import descriptor  # noqa: F401  (the enum descriptor lives on the message)

            self.code = error.DESCRIPTOR.fields_by_name["code"].enum_type.values_by_number[error.code].name.replace("ERROR_CODE_", "")
            self.message = error.message
            self.violations = [(v.field, v.description) for v in error.violations]
            self.current_revision = error.current_revision
            self.retryable = error.retryable
            self.request_id = error.request_id
            self.resource_id = error.resource.id
        else:
            self.code = self.status_name
            self.message = message
            self.violations = []
            self.current_revision = 0
            self.retryable = status == 14
            self.request_id = ""
            self.resource_id = ""
        text = f"{self.code}: {self.message}"
        if self.violations:
            text += " (" + "; ".join(f"{f}: {d}" if f else d for f, d in self.violations) + ")"
        super().__init__(text)


def _error(contract: Contract, status: int, data: bytes, fallback: str = "") -> ApiError:
    if data:
        err = contract.new("resonant.v1.Error")
        err.ParseFromString(data)
        return ApiError(status, err)
    return ApiError(status, None, fallback or STATUS_NAMES.get(status, "error"))


def default_socket_path() -> str:
    """The socket the application listens on by default (as LocalEndpoint::defaultSocketPath)."""
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if runtime:
        return str(pathlib.Path(runtime) / "resonant" / "automation.sock")
    import tempfile

    return str(pathlib.Path(tempfile.gettempdir()) / f"resonant-{os.getuid()}" / "automation.sock")


def read_token(socket_path: str, token_file: Optional[str] = None) -> str:
    """The access token: RESONANT_AUTOMATION_TOKEN, else the token file (beside the socket by default)."""
    env = os.environ.get("RESONANT_AUTOMATION_TOKEN")
    if env:
        return env.strip()
    path = pathlib.Path(token_file) if token_file else pathlib.Path(socket_path).parent / "automation.token"
    return path.read_text().strip()


class LocalTransport:
    """The application's Unix-domain socket."""

    MAX_FRAME = 256 << 20

    def __init__(self, contract: Contract, socket_path: Optional[str] = None, token: Optional[str] = None, timeout: Optional[float] = 120.0):
        self.contract = contract
        self.socket_path = socket_path or default_socket_path()
        self.token = token if token is not None else read_token(self.socket_path)
        self.timeout = timeout

    def _send(self, method: str, payload: bytes) -> socket.socket:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        try:
            s.connect(self.socket_path)
        except OSError as e:
            s.close()
            raise ApiError(14, None, f"the application is not reachable at its socket ({e.strerror})") from None
        request = self.contract.new("resonant.transport.v1.LocalRequest", method=method, payload=payload)
        request.metadata["token"] = self.token
        data = request.SerializeToString()
        s.sendall(struct.pack("<I", len(data)) + data)
        return s

    @staticmethod
    def _read_exact(s: socket.socket, n: int) -> bytes:
        chunks, got = [], 0
        while got < n:
            chunk = s.recv(min(n - got, 1 << 20))
            if not chunk:
                raise ApiError(14, None, "the application closed the connection")
            chunks.append(chunk)
            got += len(chunk)
        return b"".join(chunks)

    def _frame(self, s: socket.socket):
        (size,) = struct.unpack("<I", self._read_exact(s, 4))
        if size > self.MAX_FRAME:
            raise ApiError(8, None, "a response frame is too large")
        response = self.contract.new("resonant.transport.v1.LocalResponse")
        response.ParseFromString(self._read_exact(s, size))
        return response

    def raw_unary(self, method: str, payload: bytes) -> Tuple[int, bytes, bytes]:
        s = self._send(method, payload)
        try:
            r = self._frame(s)
            return r.status, r.payload, r.error
        finally:
            s.close()

    def unary(self, method: str, payload: bytes) -> bytes:
        status, data, error = self.raw_unary(method, payload)
        if status != 0:
            raise _error(self.contract, status, error)
        return data

    def raw_stream(self, method: str, payload: bytes, timeout: Optional[float] = None):
        """Yields (True, item) per item, then (False, (status, error))."""
        s = self._send(method, payload)
        s.settimeout(timeout)
        try:
            while True:
                r = self._frame(s)
                if r.stream_item:
                    yield True, r.payload
                    continue
                yield False, (r.status, r.error)
                return
        finally:
            s.close()

    def stream(self, method: str, payload: bytes, timeout: Optional[float] = None) -> Iterator[bytes]:
        for is_item, value in self.raw_stream(method, payload, timeout):
            if is_item:
                yield value
            else:
                status, error = value
                if status != 0:
                    raise _error(self.contract, status, error)

    def close(self):
        pass


class GrpcTransport:
    """A gRPC channel to the bridge. Messages travel as bytes (the bridge relays them)."""

    def __init__(self, contract: Contract, target: str, token: str, timeout: Optional[float] = 120.0, channel=None):
        import grpc

        self._grpc = grpc
        self.contract = contract
        self.target = target
        self.token = token
        self.timeout = timeout
        self.channel = channel or grpc.insecure_channel(target)
        self._metadata = ((TOKEN_METADATA_KEY, token),)

    def _raise(self, e) -> ApiError:
        data = b""
        try:
            for key, value in e.trailing_metadata() or ():
                if key == ERROR_METADATA_KEY:
                    data = value
        except Exception:
            pass
        code = e.code().value[0] if e.code() is not None else 2
        return _error(self.contract, code, data, e.details() or "")

    def unary(self, method: str, payload: bytes) -> bytes:
        call = self.channel.unary_unary(method)
        try:
            return call(payload, metadata=self._metadata, timeout=self.timeout)
        except self._grpc.RpcError as e:
            raise self._raise(e) from None

    def stream(self, method: str, payload: bytes, timeout: Optional[float] = None) -> Iterator[bytes]:
        call = self.channel.unary_stream(method)
        responses = call(payload, metadata=self._metadata, timeout=timeout)
        try:
            for item in responses:
                yield item
        except self._grpc.RpcError as e:
            raise self._raise(e) from None
        finally:
            responses.cancel()

    def close(self):
        self.channel.close()


class InProcessTransport:
    """The dispatcher in this process, around a resonant.Project (same contract, no socket)."""

    def __init__(self, contract: Contract, project):
        from resonant._native import native  # the native module of the resonant package

        if not hasattr(native, "AutomationSession"):
            raise RuntimeError("this build of the Python module has no automation API (built without protobuf)")
        self.contract = contract
        self.project = project
        self.native = native.AutomationSession(project)

    def unary(self, method: str, payload: bytes) -> bytes:
        status, data, error = self.native.call(method, payload)
        if status != 0:
            raise _error(self.contract, status, error)
        return data

    def stream(self, method: str, payload: bytes, timeout: Optional[float] = None) -> Iterator[bytes]:
        status, items, error = self.native.stream(method, payload)
        yield from items
        if status != 0:
            raise _error(self.contract, status, error)

    def close(self):
        self.native = None
