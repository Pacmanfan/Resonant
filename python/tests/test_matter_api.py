"""Functionally architected matter through the automation API (REQ-CHAT-20261127-001, RES-FAM-019).

Against a real headless host with a copy of the energy-converter example open: the
architectures and their validation, an analysis refused with its reason and one that runs,
exploration and manufacturability that change nothing, an alternative compared side by
side, a stack added to the coupled engine, the layer table, and the MCP tools at their
levels. Skipped when this build has no automation API.
"""

import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

HOST = os.environ.get("RESONANT_AUTOMATION_HOST", "")
DESCRIPTORS = os.environ.get("RESONANT_API_DESCRIPTORS", "")
AVAILABLE = bool(HOST and DESCRIPTORS and os.path.isfile(HOST) and os.path.isfile(DESCRIPTORS))
EXAMPLES = pathlib.Path(__file__).resolve().parents[2] / "examples" / "matter"


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class MatterApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-matter-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLES / "b-energy-converter.resonant", projects / "converter.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="matter-test", contract=cls.contract,
                          permissions=["read", "modify", "files"])
        cls.api.ProjectService.OpenProject(file_name="converter.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_architectures_analyses_and_alternatives(self):
        from google.protobuf import json_format
        from resonant_api import ApiError

        matter = self.api.MatterService
        listed = [json_format.MessageToDict(a) for a in matter.ListArchitectures().architectures]
        self.assertEqual({a["id"] for a in listed}, {"arch-bonded", "arch-film"})
        caps = json_format.MessageToDict(matter.GetMatterCapabilities().capabilities)
        self.assertIn("harvester", [a["id"] for a in caps["analyses"]])
        check = json_format.MessageToDict(matter.CheckStackCompatibility(architecture_id="arch-bonded", stack_id="st-beam", analysis="coupled-engine").result)
        self.assertFalse(check["compatible"])  # the segmented electrode is refused, with its reason
        with self.assertRaises(ApiError):
            matter.AnalyzeStack(architecture_id="arch-bonded", stack_id="st-beam", analysis="coupled-engine")
        harvested = json_format.MessageToDict(matter.AnalyzeStack(architecture_id="arch-bonded", stack_id="st-beam", analysis="harvester",
                                                                  options={"tipMass": 0.002}).result)
        self.assertGreater(harvested["metrics"]["power"], 0.0)
        self.assertEqual(harvested["result"]["balance"], "verified")
        made = json_format.MessageToDict(matter.CheckManufacturability(architecture_id="arch-film").result)
        self.assertEqual(made["status"], "infeasible")
        explored = json_format.MessageToDict(matter.ExploreArchitecture(architecture_id="arch-film").result)
        self.assertTrue(any("delamination" in f["statement"] for f in explored["findings"]))
        self.assertIn("Nothing was changed", explored["disclaimer"])
        mechanisms = json_format.MessageToDict(matter.ListMechanisms(from_domain="thermal", to_domain="electrical").result)
        self.assertFalse(any(m["implemented"] for m in mechanisms["direct"]))
        matter.DuplicateArchitecture(architecture_id="arch-bonded", new_id="arch-thick", name="Thicker substrate")
        doc = json_format.MessageToDict(matter.GetArchitecture(architecture_id="arch-thick").architecture)
        self.assertEqual(doc["parent"], "arch-bonded")
        compared = json_format.MessageToDict(matter.CompareArchitectures(spec={"architectures": ["arch-bonded", "arch-film"]}).result)
        self.assertIn("modes.f1", compared["metrics"])
        self.assertNotIn("score", compared)
        table = matter.ExportLayerTable(architecture_id="arch-bonded", stack_id="st-beam", format="csv").text
        self.assertIn("pzt.0,pzt,transduction", table)
        report = matter.GenerateArchitectureReport(architecture_id="arch-film", format="markdown").text
        self.assertIn("Manufacturing: infeasible", report)
        pathway = json_format.MessageToDict(matter.EvaluatePathway(architecture_id="arch-bonded", function_id="f-harvest").result)
        self.assertTrue(pathway["complete"])

    def test_mcp_tools_have_their_levels(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["analyze_stack"].level, "read")
        self.assertEqual(TOOLS["explore_architecture"].level, "read")
        self.assertEqual(TOOLS["set_architecture"].level, "modify")
        self.assertEqual(TOOLS["create_physical_system_from_stack"].level, "modify")
        for name in ("list_architectures", "analyze_stack", "list_mechanisms", "evaluate_pathway", "check_manufacturability"):
            self.contract.method(TOOLS[name].method)
