"""Dynamic material state through the automation API (REQ-CHAT-20261126-021, RES-DMS-014).

Against a real headless host with the damage example open: the model catalogue with the
models that are not implemented, a stored study run as a job, its record saved and
committed to a material with provenance computed, and the MCP tools at their levels.
Skipped when this build has no automation API.
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
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "materialstate" / "d-damage-resonance.resonant"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class MaterialStateApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-mst-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "damage.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="material-state-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="damage.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_the_catalogue_names_what_is_not_implemented(self):
        from google.protobuf import json_format

        models = [json_format.MessageToDict(m) for m in self.api.MaterialStateService.ListMaterialStateModels().models]
        status = {m["id"]: m["status"] for m in models}
        self.assertEqual(status["mechanics.j2.point"], "implemented")
        self.assertEqual(status["em.preisach"], "implemented")
        self.assertEqual(status["mechanics.fatigue"], "not implemented")
        self.assertEqual(status["optics.nonlinear"], "not implemented")

    def test_run_save_and_commit_a_damaged_state(self):
        from google.protobuf import json_format

        service = self.api.MaterialStateService
        job = service.RunMaterialStateStudy(study_id="st-damage-point").job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        result = service.GetMaterialStateResult(result_id=finished.results[0].id)
        self.assertTrue(result.reliable)
        doc = json_format.MessageToDict(result.document)
        secant = doc["final"]["secantModulus"]
        self.assertLess(secant, 25e9)
        saved = service.SaveMaterialStateRecord(result_id=finished.results[0].id, record_id="rec-api", name="Damaged laminate")
        self.assertEqual(saved.record_id, "rec-api")
        records = [r.record_id for r in service.ListMaterialStateRecords().records]
        self.assertIn("rec-api", records)
        service.CommitMaterialState(record_id="rec-api", material_id="mat-ex-laminate")
        material = self.api.MaterialService.GetMaterial(material_id="mat-ex-laminate").material
        e = [p for p in material.properties if p.domain == "mechanical" and p.key == "youngsModulus"][0]
        self.assertAlmostEqual(e.value_si, secant, delta=1e-3)
        provenance = e.DESCRIPTOR.fields_by_name["provenance"].enum_type.values_by_number[e.provenance].name
        self.assertEqual(provenance, "MATERIAL_PROVENANCE_COMPUTED")
        self.assertTrue(e.derivation_id.startswith("drv-"))
        self.api.ProjectService.Undo()
        self.api.ProjectService.Undo()

    def test_not_implemented_models_are_refused(self):
        from resonant_api import ApiError

        with self.assertRaises(ApiError):
            self.api.MaterialStateService.RunMaterialStateStudy(study={"model": "mechanics.fatigue"})

    def test_mcp_tools_have_their_levels(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["run_material_state_study"].level, "execute")
        self.assertEqual(TOOLS["commit_material_state"].level, "modify")
        self.assertEqual(TOOLS["list_material_state_models"].level, "read")
        for name in ("run_material_state_study", "evolve_physical_system", "commit_material_state", "get_material_state_record"):
            self.contract.method(TOOLS[name].method)
