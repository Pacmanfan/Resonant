"""Functional structures through the automation API (REQ-CHAT-20261126-017, RES-FUNC-014).

Against a real headless host with the microwave-cavity example open: the
analysis catalogue, the stored structure and its parameters, an undoable
parameter change with optimistic concurrency, an analysis as a job whose
result names its model and becomes stale when the geometry changes, a sweep,
and the MCP adapter's tools at their permission levels. Skipped when this
build has no automation API.
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
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "functional" / "a-microwave-cavity.resonant"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class FunctionalApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-fs-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "cavity.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="functional-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="cavity.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def _result(self, job):
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        self.assertTrue(finished.results, "the finished job names its result")
        return self.api.FunctionalStructureService.GetFunctionalResult(result_id=finished.results[0].id, include_document=True).result

    def test_the_catalogue_states_models_and_metrics(self):
        kinds = {k.kind_id: k for k in self.api.FunctionalStructureService.ListAnalysisKinds().kinds}
        cavity = kinds["em.cavity.rectangular"]
        self.assertEqual(cavity.category, "confinement")
        self.assertIn("full-wave", [m.fidelity for m in cavity.models])
        self.assertIn("Hz", [m.unit for m in cavity.metrics if m.key == "cavity.f1"])
        self.assertIn("optics.resonator.two-mirror", kinds)
        self.assertIn("acoustic.horn", kinds)

    def test_parameters_analysis_staleness_and_sweep(self):
        from google.protobuf import json_format

        service = self.api.FunctionalStructureService
        structures = service.ListFunctionalStructures().structures
        self.assertEqual([s.structure_id for s in structures], ["fs-microwave-cavity"])
        parameters = {p.parameter_id: p for p in service.GetStructureParameters(structure_id="fs-microwave-cavity").parameters}
        self.assertAlmostEqual(parameters["a"].value, 0.02286)
        self.assertEqual(parameters["a"].unit, "m")
        self.assertFalse(parameters["aspect"].editable)

        result = self._result(service.AnalyzeStructure(structure_id="fs-microwave-cavity").job)
        metrics = {m.key: m for m in result.metrics}
        self.assertGreater(metrics["cavity.f1"].value, 5e9)
        self.assertEqual(metrics["cavity.f1"].unit, "Hz")
        self.assertEqual(result.model_id, "em.cavity.rectangular.analytical")
        self.assertFalse(result.stale)
        self.assertEqual(result.behaviour, "resonant")
        document = json_format.MessageToDict(result.document)
        self.assertEqual(document["modes"][0]["label"], "TE101")

        changed = service.SetStructureParameters(structure_id="fs-microwave-cavity", values={"d": 0.034})
        self.assertAlmostEqual({p.parameter_id: p.value for p in changed.parameters}["d"], 0.034)
        later = service.GetFunctionalResult(result_id=result.result_id).result
        self.assertTrue(later.stale)
        self.assertEqual(later.staleness[0].what, "geometry")
        self.api.ProjectService.Undo()
        self.assertFalse(service.GetFunctionalResult(result_id=result.result_id).result.stale)

        sweep = self._result(service.SweepStructure(structure_id="fs-microwave-cavity",
                                                    axes=[{"parameter_id": "a", "first": 0.02, "last": 0.024, "points": 3}]).job)
        self.assertEqual(sweep.kind, "sweep")
        points = json_format.MessageToDict(sweep.document)["points"]
        self.assertEqual(len(points), 3)
        self.assertGreater(points[0]["metrics"]["cavity.f1"], points[2]["metrics"]["cavity.f1"])

    def test_a_stale_revision_is_a_conflict(self):
        from resonant_api import ApiError

        with self.assertRaises(ApiError):
            self.api.FunctionalStructureService.SetStructureParameters(structure_id="fs-microwave-cavity", values={"a": 0.023},
                                                                       expected_revision=999999)

    def test_mcp_offers_the_structures_by_level(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["analyze_structure"].level, "execute")
        self.assertEqual(TOOLS["optimize_structure"].level, "execute")
        self.assertEqual(TOOLS["set_structure_parameters"].level, "modify")
        self.assertEqual(TOOLS["get_functional_result"].level, "read")
        for spec in TOOLS.values():
            self.contract.method(spec.method)


if __name__ == "__main__":
    unittest.main()
