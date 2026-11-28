"""Metamaterials through the automation API (REQ-CHAT-20261126-019, RES-META-011, RES-META-012).

Against a real headless host with the layered thermal example open: the
effective-medium analyses in the catalogue, an analysis whose result carries
the effective material, saving it into the library with provenance
"homogenized" (and undoing that), refusal once the result is stale, and the
MCP tool at its permission level. Skipped when this build has no automation API.
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
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "metamaterials" / "e-layered-thermal.resonant"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class MetamaterialApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-meta-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "laminate.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="metamaterial-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="laminate.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def _analysis(self):
        job = self.api.FunctionalStructureService.AnalyzeStructure(structure_id="fs-layered-thermal").job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        return finished.results[0].id

    def test_the_catalogue_lists_the_effective_medium_analyses(self):
        kinds = {k.kind_id: k for k in self.api.FunctionalStructureService.ListAnalysisKinds().kinds}
        for kind in ("mechanics.mass-in-mass-lattice", "acoustic.resonator-duct", "em.layered-effective-medium", "em.split-ring-array",
                     "thermal.layered-conductivity"):
            self.assertIn(kind, kinds)
            self.assertEqual(kinds[kind].category, "effective-medium")

    def test_an_effective_material_is_saved_with_its_derivation_and_undone(self):
        from google.protobuf import json_format

        service = self.api.FunctionalStructureService
        result_id = self._analysis()
        result = service.GetFunctionalResult(result_id=result_id, include_document=True).result
        metrics = {m.key: m.value for m in result.metrics}
        effective = json_format.MessageToDict(result.document)["details"]["effectiveMaterial"]
        self.assertEqual(effective["kind"], "homogenized")
        saved = service.CreateEffectiveMaterial(result_id=result_id, material_id="mat-laminate-eff", name="Laminate (effective)")
        self.assertEqual(saved.material.material_id, "mat-laminate-eff")
        conductivity = [p for p in saved.material.properties if p.domain == "thermal" and p.key == "thermalConductivity"]
        self.assertEqual(len(conductivity), 1)
        self.assertAlmostEqual(conductivity[0].value_si, metrics["thermal.perpendicular"], places=9)
        provenance = conductivity[0].DESCRIPTOR.fields_by_name["provenance"].enum_type.values_by_number[conductivity[0].provenance].name
        self.assertEqual(provenance, "MATERIAL_PROVENANCE_HOMOGENIZED")
        self.assertIn("fs-layered-thermal", conductivity[0].source)
        along = service.CreateEffectiveMaterial(result_id=result_id, material_id="mat-laminate-along",
                                                components={"thermal.thermalConductivity": "parallel"})
        value = [p.value_si for p in along.material.properties if p.key == "thermalConductivity"][0]
        self.assertAlmostEqual(value, metrics["thermal.parallel"], places=6)
        ids = [m.material_id for m in self.api.MaterialService.ListMaterials().materials]
        self.assertIn("mat-laminate-eff", ids)
        self.api.ProjectService.Undo()
        self.api.ProjectService.Undo()
        ids = [m.material_id for m in self.api.MaterialService.ListMaterials().materials]
        self.assertNotIn("mat-laminate-eff", ids)

    def test_a_stale_result_is_refused(self):
        from resonant_api import ApiError

        service = self.api.FunctionalStructureService
        result_id = self._analysis()
        service.SetStructureParameters(structure_id="fs-layered-thermal", values={"te": 0.0002})
        try:
            with self.assertRaises(ApiError) as caught:
                service.CreateEffectiveMaterial(result_id=result_id)
            self.assertIn("stale", str(caught.exception))
        finally:
            self.api.ProjectService.Undo()

    def test_the_mcp_tool_needs_modify(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["create_effective_material"].level, "modify")
        self.contract.method(TOOLS["create_effective_material"].method)
