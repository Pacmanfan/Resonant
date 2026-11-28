"""The experimental laboratory through the automation API (REQ-CHAT-20261126-023, RES-LAB-017).

Against a real headless host with a copy of the resonator example (and its data) open:
models and their physical parameters, an import with its report, a comparison kept as a
record, a calibration job accepted and validated on another run (validation on the same
data refused), an evidence summary, a report, and the MCP tools at their levels. Skipped
when this build has no automation API.
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
EXAMPLES = pathlib.Path(__file__).resolve().parents[2] / "examples" / "laboratory"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class LaboratoryApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-lab-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLES / "a-resonator-frf.resonant", projects / "resonator.resonant")
        shutil.copytree(EXAMPLES / "data", projects / "data")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="laboratory-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="resonator.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_import_compare_calibrate_and_validate(self):
        from google.protobuf import json_format
        from resonant_api import ApiError

        lab = self.api.LaboratoryService
        models = [json_format.MessageToDict(m)["id"] for m in lab.ListLaboratoryModels().models]
        self.assertIn("resonator.lumped", models)
        params = [json_format.MessageToDict(p)["id"] for p in lab.GetLaboratoryModelParameters(model="resonator.lumped").parameters]
        self.assertEqual(params, ["mass", "stiffness", "damping"])
        first = lab.ImportDataset(spec={"path": "data/a-frf-run1.csv", "study": "st-resonator", "role": "calibration"})
        self.assertTrue(first.accepted)
        run1 = json_format.MessageToDict(first.report)["datasets"][0]
        second = lab.ImportDataset(spec={"path": "data/a-frf-run2.csv", "study": "st-resonator", "role": "validation"})
        run2 = json_format.MessageToDict(second.report)["datasets"][0]
        nominal = {"model": "resonator.lumped", "configuration": {"mass": 0.45, "stiffness": 2.2e5, "damping": 5.0}}
        compared = lab.CompareWithPrediction(spec={"id": "cmp-api", "study": "st-resonator", "kind": "response", "measured": run1, "predicted": nominal})
        self.assertIn("inconsistent", compared.verdict)
        job = lab.StartCalibration(spec={"id": "cal-api", "study": "st-resonator", "model": "resonator.lumped",
                                         "configuration": nominal["configuration"], "datasets": [run1], "observable": "magnitude",
                                         "parameters": [{"id": "mass"}, {"id": "stiffness"}, {"id": "damping"}]}).job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        result = lab.GetCalibrationResult(result_id=finished.results[0].id)
        self.assertEqual(result.status, "calibrated")
        lab.AcceptCalibration(result_id=finished.results[0].id)
        with self.assertRaises(ApiError):
            lab.ValidateCalibration(calibration_id="cal-api", datasets=[run1])
        validated = lab.ValidateCalibration(calibration_id="cal-api", datasets=[run2], validation_id="val-api")
        self.assertEqual(validated.status, "validated within uncertainty")
        explanation = json_format.MessageToDict(lab.ExplainRecord(record_id="cmp-api").explanation)
        self.assertIn("no model or parameter was changed", explanation["disclaimer"])
        report = lab.GenerateLaboratoryReport(study_id="st-resonator", format="markdown").text
        self.assertIn("validated", report)

    def test_mcp_tools_have_their_levels(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["start_calibration"].level, "execute")
        self.assertEqual(TOOLS["import_dataset"].level, "modify")
        self.assertEqual(TOOLS["apply_calibration"].level, "modify")
        self.assertEqual(TOOLS["explain_laboratory_record"].level, "read")
        for name in ("import_dataset", "compare_with_prediction", "start_calibration", "validate_calibration", "explain_laboratory_record"):
            self.contract.method(TOOLS[name].method)
