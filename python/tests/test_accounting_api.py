"""Unified accounting through the automation API (REQ-CHAT-20261126-022, RES-ACC-016).

Against a real headless host with the coupled-resonators example open: the quantity
families and conversions, a stored study run as a job, its report for a subsystem without
running again, a facet query with full context, an export, a record kept in the project,
and the MCP tools at their levels. Skipped when this build has no automation API.
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
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "accounting" / "d-coupled-resonators.resonant"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class AccountingApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-acc-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "coupled.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="accounting-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="coupled.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def _run(self, study_id):
        job = self.api.AccountingService.RunAccounting(study_id=study_id).job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        return finished.results[0].id

    def test_families_and_conversions(self):
        from google.protobuf import json_format
        from resonant_api import ApiError

        service = self.api.AccountingService
        families = {f["id"]: f for f in (json_format.MessageToDict(x) for x in service.ListQuantityFamilies().families)}
        self.assertEqual(families["torque"]["dimension"], families["energy"]["dimension"])
        self.assertFalse(families["torque"]["conserved"])
        self.assertTrue(families["energy"]["conserved"])
        converted = service.ConvertQuantity(value=1.0, from_unit="kWh", to_unit="J")
        self.assertAlmostEqual(converted.value, 3.6e6)
        with self.assertRaises(ApiError):
            service.ConvertQuantity(value=1.0, from_unit="J", to_unit="W")

    def test_run_report_query_export_and_keep(self):
        from google.protobuf import json_format

        service = self.api.AccountingService
        result_id = self._run("st-beats")
        result = service.GetAccountingResult(result_id=result_id)
        self.assertIn(result.status, ("verified", "warning", "not applicable"))
        report = service.GetAccountingReport(result_id=result_id,
                                             system={"id": "r1", "kind": "subsystem", "members": ["body:r1", "element:k1"]})
        checks = {c["model"]: c for c in json_format.MessageToDict(report.report)["checks"]}
        self.assertEqual(checks["energy.total"]["status"], "verified")
        query = json_format.MessageToDict(service.QueryAccounting(result_id=result_id, facet="energy",
                                                                  system={"id": "r1", "members": ["body:r1", "element:k1"]}).result)
        coupling = [q for q in query["quantities"] if q["id"] == "work:r1->coupling"][0]
        self.assertEqual(coupling["role"], "output")
        self.assertEqual(coupling["unit"], "J")
        self.assertIn("provenance", coupling)
        text = service.ExportAccounting(result_id=result_id, format="markdown").text
        self.assertIn("| Check | Status |", text)
        saved = service.SaveAccountingRecord(result_id=result_id, record_id="acc-api", name="Beats")
        self.assertEqual(saved.record_id, "acc-api")
        self.assertIn("acc-api", [r.record_id for r in service.ListAccountingRecords().records])
        self.api.ProjectService.Undo()

    def test_mcp_tools_have_their_levels(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["run_accounting"].level, "execute")
        self.assertEqual(TOOLS["set_accounting_study"].level, "modify")
        self.assertEqual(TOOLS["query_accounting"].level, "read")
        for name in ("run_accounting", "query_accounting", "compare_accounting_systems", "export_accounting", "convert_quantity"):
            self.contract.method(TOOLS[name].method)
