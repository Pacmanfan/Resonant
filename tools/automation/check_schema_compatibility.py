#!/usr/bin/env python3
"""Checks the resonant.v1 contract against its released baseline (REQ-CHAT-20261126-012).

    python3 tools/automation/check_schema_compatibility.py [--descriptors FILE] [--update-baseline]

Within a major version (resonant.v1) the contract only grows. Breaking, and
refused here:

* a message, enum, service or method removed (or a package renamed);
* a field removed without reserving its number, or a reserved number reused;
* a field's number, type, cardinality (repeated / singular / map) or name changed
  (a rename keeps the wire format but breaks JSON, MCP tools and clients' code);
* an enum value removed or renumbered;
* a method's request or response type, or its streaming, changed.

Additions (messages, fields with new numbers, enum values, methods, services)
are compatible. The baseline (proto/baseline/resonant_v1.json) records the
released contract; --update-baseline rewrites it when a release adds to v1.
A breaking change needs resonant.v2 beside v1, not an edit of v1.

Exit status 0 when compatible, 1 with one line per break otherwise.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BASELINE = ROOT / "proto" / "baseline" / "resonant_v1.json"
sys.path.insert(0, str(ROOT / "python"))

_LABELS = {1: "singular", 2: "required", 3: "repeated"}


def summarize(file_set) -> dict:
    """The compatibility-relevant shape of every resonant.* file of a FileDescriptorSet."""
    from google.protobuf import descriptor_pb2

    maps = set()
    out = {"messages": {}, "enums": {}, "services": {}}

    def visit_message(prefix, msg):
        full = f"{prefix}.{msg.name}"
        if msg.options.map_entry:
            maps.add(full)
        for nested in msg.nested_type:
            visit_message(full, nested)
        for enum in msg.enum_type:
            visit_enum(full, enum)

    def visit_enum(prefix, enum):
        out["enums"][f"{prefix}.{enum.name}"] = {str(v.number): v.name for v in enum.value}

    files = [f for f in file_set.file if f.package.startswith("resonant.v")]
    for f in files:
        for msg in f.message_type:
            visit_message(f.package, msg)
        for enum in f.enum_type:
            visit_enum(f.package, enum)

    def record_message(prefix, msg):
        full = f"{prefix}.{msg.name}"
        if msg.options.map_entry:
            return
        fields = {}
        for fd in msg.field:
            type_name = fd.type_name.lstrip(".") if fd.type_name else descriptor_pb2.FieldDescriptorProto.Type.Name(fd.type)
            label = _LABELS.get(fd.label, "singular")
            if type_name in maps:
                label = "map"
            fields[str(fd.number)] = {"name": fd.name, "type": type_name, "cardinality": label,
                                      "optional": bool(fd.proto3_optional), "oneof": msg.oneof_decl[fd.oneof_index].name
                                      if fd.HasField("oneof_index") and not fd.proto3_optional else ""}
        reserved = sorted({n for r in msg.reserved_range for n in range(r.start, r.end)})
        out["messages"][full] = {"fields": fields, "reserved_numbers": reserved, "reserved_names": sorted(msg.reserved_name)}
        for nested in msg.nested_type:
            record_message(full, nested)

    for f in files:
        for msg in f.message_type:
            record_message(f.package, msg)
        for s in f.service:
            out["services"][f"{f.package}.{s.name}"] = {
                m.name: {"input": m.input_type.lstrip("."), "output": m.output_type.lstrip("."),
                         "client_streaming": m.client_streaming, "server_streaming": m.server_streaming}
                for m in s.method
            }
    return out


def compare(baseline: dict, current: dict) -> list:
    breaks = []
    for name, old in baseline["messages"].items():
        new = current["messages"].get(name)
        if new is None:
            breaks.append(f"message {name} was removed")
            continue
        for number, f in old["fields"].items():
            g = new["fields"].get(number)
            if g is None:
                if int(number) not in new["reserved_numbers"]:
                    breaks.append(f"{name}: field {f['name']} = {number} was removed without reserving its number")
                continue
            for key in ("name", "type", "cardinality"):
                if f[key] != g[key]:
                    breaks.append(f"{name}: field {number} changed its {key} ({f[key]} -> {g[key]})")
            if f.get("oneof", "") != g.get("oneof", ""):
                breaks.append(f"{name}: field {f['name']} moved into or out of a oneof")
        for number in old["reserved_numbers"]:
            if str(number) in new["fields"]:
                breaks.append(f"{name}: the reserved number {number} is used again ({new['fields'][str(number)]['name']})")
        for old_name in old["reserved_names"]:
            if any(g["name"] == old_name for g in new["fields"].values()):
                breaks.append(f"{name}: the reserved name {old_name} is used again")
    for name, values in baseline["enums"].items():
        new = current["enums"].get(name)
        if new is None:
            breaks.append(f"enum {name} was removed")
            continue
        for number, value in values.items():
            if new.get(number) != value:
                breaks.append(f"enum {name}: value {value} = {number} was removed or renumbered")
    for service, methods in baseline["services"].items():
        new = current["services"].get(service)
        if new is None:
            breaks.append(f"service {service} was removed")
            continue
        for method, old in methods.items():
            m = new.get(method)
            if m is None:
                breaks.append(f"{service}.{method} was removed")
                continue
            for key in ("input", "output", "client_streaming", "server_streaming"):
                if old[key] != m[key]:
                    breaks.append(f"{service}.{method} changed its {key} ({old[key]} -> {m[key]})")
    return breaks


def current_summary(descriptors: str | None) -> dict:
    from google.protobuf import descriptor_pb2

    from resonant_api.contract import compile_descriptor_set

    data = pathlib.Path(descriptors).read_bytes() if descriptors else compile_descriptor_set(ROOT / "proto")
    return summarize(descriptor_pb2.FileDescriptorSet.FromString(data))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="resonant.v1 compatibility check")
    parser.add_argument("--descriptors", help="a descriptor set (default: compile proto/ with protoc)")
    parser.add_argument("--baseline", default=str(BASELINE))
    parser.add_argument("--update-baseline", action="store_true", help="record the current contract as the baseline")
    args = parser.parse_args(argv)
    current = current_summary(args.descriptors)
    baseline_path = pathlib.Path(args.baseline)
    if args.update_baseline:
        if baseline_path.exists():
            breaks = compare(json.loads(baseline_path.read_text()), current)
            if breaks:
                for b in breaks:
                    print(b)
                print("schema: refusing to record a baseline that breaks the previous one")
                return 1
        baseline_path.parent.mkdir(parents=True, exist_ok=True)
        baseline_path.write_text(json.dumps(current, indent=1, sort_keys=True) + "\n")
        print(f"schema: baseline recorded ({len(current['messages'])} messages, "
              f"{sum(len(m) for m in current['services'].values())} methods)")
        return 0
    breaks = compare(json.loads(baseline_path.read_text()), current)
    for b in breaks:
        print(b)
    print(f"schema: {'PASS' if not breaks else 'FAIL'} ({len(breaks)} breaking change(s))")
    return 1 if breaks else 0


if __name__ == "__main__":
    sys.exit(main())
