"""The resonant.v1 contract for Python (REQ-CHAT-20261126-012, ADR-000216).

Message classes are built from the descriptor set that the build produces
from proto/ (src/automation_api/resonant_api.desc), so Python and C++ always
share one schema. Nothing generated is committed. The set is found through
RESONANT_API_DESCRIPTORS, the installed copy
(share/resonant/automation/resonant_api.desc), or, in a source checkout with
protoc available, compiled from proto/ on first use.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from google.protobuf import descriptor_pb2, descriptor_pool, message_factory

_PACKAGE_DIR = pathlib.Path(__file__).resolve().parent

API_PACKAGE = "resonant.v1"
TRANSPORT_PACKAGE = "resonant.transport.v1"


@dataclass
class Method:
    """One RPC of the contract."""

    name: str  # "/resonant.v1.SceneService/GetObject"
    service: str  # "resonant.v1.SceneService"
    short_name: str  # "GetObject"
    input_type: str
    output_type: str
    server_streaming: bool
    comment: str = ""


@dataclass
class FieldDoc:
    name: str
    type_name: str
    label: str
    comment: str = ""


@dataclass
class MessageDoc:
    full_name: str
    comment: str = ""
    fields: List[FieldDoc] = field(default_factory=list)


def _candidate_files():
    env = os.environ.get("RESONANT_API_DESCRIPTORS")
    if env:
        yield pathlib.Path(env)
    # Installed: <prefix>/share/resonant/python/resonant_api -> <prefix>/share/resonant/automation.
    yield _PACKAGE_DIR.parent.parent / "automation" / "resonant_api.desc"


def proto_source_dir() -> Optional[pathlib.Path]:
    """proto/ of a source checkout (python/resonant_api -> proto), if there is one."""
    root = _PACKAGE_DIR.parent.parent / "proto"
    return root if (root / "resonant" / "v1").is_dir() else None


def compile_descriptor_set(proto_root: pathlib.Path) -> bytes:
    """The descriptor set of every .proto under `proto_root`, compiled with protoc."""
    protoc = shutil.which("protoc")
    if not protoc:
        raise FileNotFoundError("protoc is not installed")
    files = sorted(str(p.relative_to(proto_root)) for p in proto_root.rglob("*.proto") if "baseline" not in p.parts)
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "resonant_api.desc")
        subprocess.run(
            [protoc, f"-I{proto_root}", f"--descriptor_set_out={out}", "--include_imports", "--include_source_info", *files],
            check=True,
            capture_output=True,
        )
        with open(out, "rb") as f:
            return f.read()


def find_descriptor_set() -> bytes:
    for path in _candidate_files():
        if path.is_file():
            return path.read_bytes()
    proto = proto_source_dir()
    if proto is not None and shutil.which("protoc"):
        return compile_descriptor_set(proto)
    raise FileNotFoundError(
        "the resonant.v1 descriptor set was not found: set RESONANT_API_DESCRIPTORS to the build's "
        "src/automation_api/resonant_api.desc, or install Resonant"
    )


def _comments(file_proto) -> Dict[tuple, str]:
    out = {}
    for loc in file_proto.source_code_info.location:
        text = (loc.leading_comments or loc.trailing_comments or "").strip()
        if text:
            out[tuple(loc.path)] = " ".join(line.strip() for line in text.splitlines())
    return out


class Contract:
    """The messages and methods of resonant.v1 (and the local transport's frames)."""

    def __init__(self, data: bytes):
        self.file_set = descriptor_pb2.FileDescriptorSet.FromString(data)
        self.pool = descriptor_pool.DescriptorPool()
        for f in self.file_set.file:  # protoc lists dependencies before their users
            self.pool.AddSerializedFile(f.SerializeToString())
        self.methods: Dict[str, Method] = {}
        self.messages: Dict[str, MessageDoc] = {}
        self.services: Dict[str, str] = {}  # full name -> comment
        self._classes: Dict[str, type] = {}
        for f in self.file_set.file:
            if not f.package.startswith("resonant."):
                continue
            comments = _comments(f)
            for si, s in enumerate(f.service):
                full = f"{f.package}.{s.name}"
                self.services[full] = comments.get((6, si), "")
                for mi, m in enumerate(s.method):
                    self.methods[f"/{full}/{m.name}"] = Method(
                        name=f"/{full}/{m.name}",
                        service=full,
                        short_name=m.name,
                        input_type=m.input_type.lstrip("."),
                        output_type=m.output_type.lstrip("."),
                        server_streaming=m.server_streaming,
                        comment=comments.get((6, si, 2, mi), ""),
                    )
            for mi, msg in enumerate(f.message_type):
                self._index_message(f.package, msg, (4, mi), comments)

    def _index_message(self, prefix, msg, path, comments):
        full = f"{prefix}.{msg.name}"
        doc = MessageDoc(full, comments.get(path, ""))
        labels = {1: "optional", 2: "required", 3: "repeated"}
        for fi, fd in enumerate(msg.field):
            type_name = fd.type_name.lstrip(".") if fd.type_name else descriptor_pb2.FieldDescriptorProto.Type.Name(fd.type)[5:].lower()
            label = labels.get(fd.label, "")
            if fd.proto3_optional:
                label = "optional"
            elif label == "optional":
                label = ""
            doc.fields.append(FieldDoc(fd.name, type_name, label, comments.get(path + (2, fi), "")))
        self.messages[full] = doc
        for ni, nested in enumerate(msg.nested_type):
            if not nested.options.map_entry:
                self._index_message(full, nested, path + (3, ni), comments)

    def message_class(self, full_name: str) -> type:
        cls = self._classes.get(full_name)
        if cls is None:
            cls = message_factory.GetMessageClass(self.pool.FindMessageTypeByName(full_name))
            self._classes[full_name] = cls
        return cls

    def new(self, full_name: str, **fields):
        return self.message_class(full_name)(**fields)

    def method(self, name: str) -> Method:
        """A method by "/resonant.v1.Service/Method", "Service.Method" or "Service/Method"."""
        if name in self.methods:
            return self.methods[name]
        short = name.replace("/", ".").strip(".")
        if not short.startswith(API_PACKAGE + "."):
            short = f"{API_PACKAGE}.{short}"
        service, _, method = short.rpartition(".")
        key = f"/{service}/{method}"
        if key not in self.methods:
            raise KeyError(f"the contract has no method {name!r}")
        return self.methods[key]


_DEFAULT: Optional[Contract] = None


def load_contract(data: Optional[bytes] = None) -> Contract:
    """The contract (cached when read from the default locations)."""
    global _DEFAULT
    if data is not None:
        return Contract(data)
    if _DEFAULT is None:
        _DEFAULT = Contract(find_descriptor_set())
    return _DEFAULT
