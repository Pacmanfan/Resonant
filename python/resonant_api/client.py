"""A client of the Resonant automation API (REQ-CHAT-20261126-012).

    from resonant_api import connect

    with connect(client_name="sweep script") as api:
        box = api.GeometryService.CreatePrimitive(kind_id="box", parameters={"width": 0.1})
        api.SceneService.UpdateObject(object_id=box.object.object_id, name="Housing",
                                      expected_revision=box.object.revision)
        for event in api.subscribe(categories=["EVENT_CATEGORY_SCENE"]):
            print(event.type)

Requests are built from keyword arguments (field names or JSON names; nested
messages as dicts; enums by name) or passed as a message. The client opens a
session, fills every request header (session, request id) and raises
ApiError with the structured error when a call is refused.
"""

from __future__ import annotations

import uuid
from typing import Any, Dict, Iterator, Optional

from google.protobuf import json_format

from .contract import Contract, Method, load_contract
from .transports import ApiError, GrpcTransport, InProcessTransport, LocalTransport, default_socket_path, read_token

PERMISSIONS = ("PERMISSION_READ", "PERMISSION_MODIFY", "PERMISSION_EXECUTE", "PERMISSION_FILES", "PERMISSION_ADMIN")


def to_dict(message) -> Dict[str, Any]:
    """A message as a plain dict (proto field names, defaults included)."""
    return json_format.MessageToDict(message, preserving_proto_field_name=True, always_print_fields_with_no_presence=True)


class _Service:
    def __init__(self, client: "Client", name: str):
        self._client = client
        self._name = name

    def __getattr__(self, method: str):
        full = f"/resonant.v1.{self._name}/{method}"
        if full not in self._client.contract.methods:
            raise AttributeError(f"resonant.v1.{self._name} has no method {method}")

        def call(request=None, *, header: Optional[dict] = None, **fields):
            return self._client.call(full, request, header=header, **fields)

        call.__name__ = method
        call.__doc__ = self._client.contract.methods[full].comment
        return call

    def __dir__(self):
        prefix = f"/resonant.v1.{self._name}/"
        return [m[len(prefix):] for m in self._client.contract.methods if m.startswith(prefix)]


class Client:
    """One session with the application over any transport."""

    def __init__(
        self,
        transport,
        contract: Optional[Contract] = None,
        client_name: str = "resonant_api client",
        permissions=None,
        source_interface: str = "SOURCE_INTERFACE_GRPC",
        open_session: bool = True,
    ):
        self.contract = contract or transport.contract
        self.transport = transport
        self.session_id = ""
        self.session = None
        if open_session:
            request: Dict[str, Any] = {"client_name": client_name, "source_interface": source_interface}
            if permissions:
                request["requested_permissions"] = [p if p.startswith("PERMISSION_") else "PERMISSION_" + p.upper() for p in permissions]
            self.session = self.call("SessionService.OpenSession", **request).session
            self.session_id = self.session.session_id

    # --- calls -----------------------------------------------------------------------------

    def request(self, method, request=None, header: Optional[dict] = None, **fields):
        """The request message of `method`, with its header filled in."""
        m: Method = method if isinstance(method, Method) else self.contract.method(method)
        cls = self.contract.message_class(m.input_type)
        if request is None:
            request = cls()
            if fields:
                json_format.ParseDict(fields, request)
        elif fields:
            raise TypeError("give a request message or keyword fields, not both")
        if "header" in cls.DESCRIPTOR.fields_by_name:
            h = request.header
            if header:
                json_format.ParseDict(header, h)
            if not h.session_id:
                h.session_id = self.session_id
            if not h.request_id:
                h.request_id = "py-" + uuid.uuid4().hex
        return request

    def call(self, method, request=None, header: Optional[dict] = None, **fields):
        """A unary call: `method` as "SceneService.GetObject" or "/resonant.v1.SceneService/GetObject"."""
        m = self.contract.method(method)
        if m.server_streaming:
            raise TypeError(f"{m.name} streams: use Client.stream")
        req = self.request(m, request, header, **fields)
        data = self.transport.unary(m.name, req.SerializeToString())
        out = self.contract.message_class(m.output_type)()
        out.ParseFromString(data)
        return out

    def stream(self, method, request=None, header: Optional[dict] = None, timeout: Optional[float] = None, **fields) -> Iterator:
        m = self.contract.method(method)
        if not m.server_streaming:
            raise TypeError(f"{m.name} is unary: use Client.call")
        req = self.request(m, request, header, **fields)
        cls = self.contract.message_class(m.output_type)
        for data in self.transport.stream(m.name, req.SerializeToString(), timeout):
            item = cls()
            item.ParseFromString(data)
            yield item

    def __getattr__(self, name: str):
        if name.endswith("Service") and f"resonant.v1.{name}" in self.contract.services:
            return _Service(self, name)
        raise AttributeError(name)

    # --- conveniences ----------------------------------------------------------------------------

    def subscribe(self, categories=(), resource_ids=(), after_sequence: int = 0, include_snapshot: bool = False,
                  job_id: str = "", timeout: Optional[float] = None) -> Iterator:
        """Events (and a leading snapshot / gap notices) as SubscribeEventsResponse items."""
        flt = {"categories": list(categories), "resource_ids": list(resource_ids), "job_id": job_id}
        return self.stream("EventService.SubscribeEvents", filter=flt, after_sequence=after_sequence,
                           include_snapshot=include_snapshot, timeout=timeout)

    def wait_for_job(self, job_id: str, timeout_seconds: float = 120.0):
        """The job once it ended (raises TimeoutError otherwise)."""
        import time

        deadline = time.monotonic() + timeout_seconds
        while True:
            remaining = deadline - time.monotonic()
            r = self.call("JobService.WaitForJob", job_id=job_id, timeout_seconds=max(0.05, min(remaining, 30.0)))
            if r.finished:
                return r.job
            if remaining <= 0:
                raise TimeoutError(f"the job {job_id} did not finish in {timeout_seconds} s")

    def read_field_slice(self, result_id: str, fixed_axis: str = "SLICE_AXIS_W", fixed_index: int = 0, stride: int = 1):
        """A slice of a sampled field as a complex64 NumPy array (rows, columns), in Pa."""
        import numpy as np

        chunks, rows, columns = [], 0, 0
        for chunk in self.stream("ResultsService.ReadFieldSlice", result_id=result_id, fixed_axis=fixed_axis,
                                 fixed_index=fixed_index, stride=stride):
            rows, columns = chunk.rows, chunk.columns
            chunks.append(np.frombuffer(chunk.samples, dtype="<c8"))
        data = np.concatenate(chunks) if chunks else np.zeros(0, dtype=np.complex64)
        return data.reshape(rows, columns)

    def close(self):
        if self.session_id:
            try:
                self.call("SessionService.CloseSession")
            except ApiError:
                pass
            self.session_id = ""
        self.transport.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def connect(
    socket_path: Optional[str] = None,
    token: Optional[str] = None,
    token_file: Optional[str] = None,
    grpc_target: Optional[str] = None,
    project=None,
    client_name: str = "resonant_api client",
    permissions=None,
    source_interface: str = "SOURCE_INTERFACE_GRPC",
    contract: Optional[Contract] = None,
) -> Client:
    """A client: in-process around `project`, over gRPC to `grpc_target`, or over the local socket (default)."""
    contract = contract or load_contract()
    if project is not None:
        transport = InProcessTransport(contract, project)
    elif grpc_target:
        if token is None:
            token = read_token(socket_path or default_socket_path(), token_file)
        transport = GrpcTransport(contract, grpc_target, token)
    else:
        path = socket_path or default_socket_path()
        transport = LocalTransport(contract, path, token if token is not None else read_token(path, token_file))
    return Client(transport, contract, client_name=client_name, permissions=permissions, source_interface=source_interface)
