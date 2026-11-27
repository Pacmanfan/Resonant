"""The resonant.v1 contract's guards (REQ-CHAT-20261126-012).

The contract stays compatible with its released baseline; the checker finds
each kind of breaking change; the generated protobuf reference is current.
Uses the build's descriptor set (RESONANT_API_DESCRIPTORS) or protoc.
"""

import copy
import importlib.util
import json
import os
import pathlib
import shutil
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
DESCRIPTORS = os.environ.get("RESONANT_API_DESCRIPTORS", "")
HAVE_CONTRACT = bool(DESCRIPTORS and os.path.isfile(DESCRIPTORS)) or bool(shutil.which("protoc"))


def _tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / "automation" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


@unittest.skipUnless(HAVE_CONTRACT, "neither the build's descriptor set nor protoc is available")
class SchemaGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compat = _tool("check_schema_compatibility")
        cls.docs = _tool("generate_protobuf_docs")
        cls.current = cls.compat.current_summary(DESCRIPTORS if DESCRIPTORS and os.path.isfile(DESCRIPTORS) else None)
        cls.baseline = json.loads((ROOT / "proto" / "baseline" / "resonant_v1.json").read_text())

    def test_the_contract_is_compatible_with_its_baseline(self):
        self.assertEqual(self.compat.compare(self.baseline, self.current), [])

    def test_breaking_changes_are_found(self):
        base = self.baseline
        message = "resonant.v1.CreatePrimitiveRequest"
        number = next(n for n, f in base["messages"][message]["fields"].items() if f["name"] == "kind_id")

        removed = copy.deepcopy(base)
        del removed["messages"][message]["fields"][number]
        self.assertTrue(any("removed without reserving" in b for b in self.compat.compare(base, removed)))

        reserved = copy.deepcopy(removed)
        reserved["messages"][message]["reserved_numbers"].append(int(number))
        self.assertEqual(self.compat.compare(base, reserved), [])  # removing and reserving is allowed

        retyped = copy.deepcopy(base)
        retyped["messages"][message]["fields"][number]["type"] = "TYPE_INT32"
        self.assertTrue(any("changed its type" in b for b in self.compat.compare(base, retyped)))

        renamed = copy.deepcopy(base)
        renamed["messages"][message]["fields"][number]["name"] = "kind"
        self.assertTrue(any("changed its name" in b for b in self.compat.compare(base, renamed)))

        repeated = copy.deepcopy(base)
        repeated["messages"][message]["fields"][number]["cardinality"] = "repeated"
        self.assertTrue(any("cardinality" in b for b in self.compat.compare(base, repeated)))

        no_method = copy.deepcopy(base)
        del no_method["services"]["resonant.v1.SceneService"]["GetObject"]
        self.assertTrue(any("GetObject was removed" in b for b in self.compat.compare(base, no_method)))

        streaming = copy.deepcopy(base)
        streaming["services"]["resonant.v1.SceneService"]["GetObject"]["server_streaming"] = True
        self.assertTrue(any("server_streaming" in b for b in self.compat.compare(base, streaming)))

        enum = copy.deepcopy(base)
        enum["enums"]["resonant.v1.JobState"].pop(next(iter(enum["enums"]["resonant.v1.JobState"])))
        self.assertTrue(any("JobState" in b for b in self.compat.compare(base, enum)))

        added = copy.deepcopy(base)
        added["messages"][message]["fields"]["99"] = {"name": "new_field", "type": "TYPE_STRING", "cardinality": "singular",
                                                       "optional": False, "oneof": ""}
        added["services"]["resonant.v1.SceneService"]["NewMethod"] = {"input": "x", "output": "y", "client_streaming": False,
                                                                     "server_streaming": False}
        self.assertEqual(self.compat.compare(base, added), [])  # additions are compatible

    def test_the_protobuf_reference_is_generated_from_the_current_contract(self):
        from resonant_api.contract import Contract, compile_descriptor_set

        data = pathlib.Path(DESCRIPTORS).read_bytes() if DESCRIPTORS and os.path.isfile(DESCRIPTORS) else compile_descriptor_set(ROOT / "proto")
        text = self.docs.render(Contract(data))
        self.assertEqual((ROOT / "docs" / "reference" / "PROTOBUF_API.md").read_text(), text,
                         "docs/reference/PROTOBUF_API.md is out of date: run tools/automation/generate_protobuf_docs.py")

    def test_every_method_and_service_is_documented(self):
        from resonant_api.contract import Contract, compile_descriptor_set

        data = pathlib.Path(DESCRIPTORS).read_bytes() if DESCRIPTORS and os.path.isfile(DESCRIPTORS) else compile_descriptor_set(ROOT / "proto")
        contract = Contract(data)
        undocumented = [m.name for m in contract.methods.values() if m.service.startswith("resonant.v1.") and not m.comment]
        self.assertEqual(undocumented, [])
        self.assertEqual([s for s, c in contract.services.items() if s.startswith("resonant.v1.") and not c], [])


if __name__ == "__main__":
    unittest.main()
