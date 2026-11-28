"""Multiscale material modeling through the automation API (REQ-CHAT-20261126-020, RES-MSC-014).

Against a real headless host with the porous-ceramic example open: the representations, a
derivation as a job whose result names its model card and convergence, assignment to a
library material with provenance and lineage, staleness after the RVE changes, the
external engines listed as not integrated, and the MCP tools at their permission levels.
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
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "multiscale" / "c-porous-composite.resonant"


def _state(job):
    return job.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[job.state].name


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class MultiscaleApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-ms-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "porous.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="multiscale-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="porous.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def _derive(self):
        service = self.api.MultiscaleService
        job = service.DeriveMaterialProperties(representation_id="rve-porous-alumina", method_id="rve.thermal-conductivity").job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertEqual(_state(finished), "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        return finished.results[0].id

    def test_representations_and_catalogues(self):
        service = self.api.MultiscaleService
        reps = {r.representation_id: r for r in service.ListRepresentations().representations}
        self.assertEqual(reps["rve-porous-alumina"].scale, "microstructure")
        self.assertEqual(reps["rve-porous-alumina"].provenance, "synthetic")
        self.assertIn("mat-ex-pore-air", list(reps["rve-porous-alumina"].constituents))
        solvers = {s.solver_id: s for s in service.ListExternalSolvers().solvers}
        for sid in ("lammps", "openmm", "quantum-espresso", "cp2k", "gpaw"):
            self.assertIn("not integrated", solvers[sid].status)
        cards = [c["id"] for c in (dict(x) for x in service.ListModelCards().cards)]
        self.assertIn("rve.potential-fe", cards)

    def test_derive_assign_lineage_and_staleness(self):
        from google.protobuf import json_format

        service = self.api.MultiscaleService
        result_id = self._derive()
        result = service.GetMultiscaleResult(result_id=result_id)
        doc = json_format.MessageToDict(result.document)
        self.assertEqual(doc["modelCard"], "rve.potential-fe")
        self.assertFalse(result.stale)
        mean = [p for p in doc["properties"] if p["key"] == "thermalConductivity" and p["component"] == "mean"][0]["value"]
        assigned = service.AssignDerivedProperties(result_id=result_id, material_id="mat-ex-porous-alumina",
                                                   properties=["thermal.thermalConductivity"])
        k = [p for p in assigned.material.properties if p.key == "thermalConductivity"][0]
        self.assertAlmostEqual(k.value_si, mean, places=9)
        provenance = k.DESCRIPTOR.fields_by_name["provenance"].enum_type.values_by_number[k.provenance].name
        self.assertEqual(provenance, "MATERIAL_PROVENANCE_HOMOGENIZED")
        self.assertTrue(k.derivation_id.startswith("drv-"))
        lineage = json_format.MessageToDict(service.GetMaterialLineage(material_id="mat-ex-porous-alumina").lineage)
        self.assertEqual(lineage["properties"][0]["representation"]["id"], "rve-porous-alumina")
        # Change the RVE: the property and the result become stale.
        rep = json_format.MessageToDict(service.GetRepresentation(representation_id="rve-porous-alumina").document)
        rep["data"]["rve"]["seed"] = 5
        service.SetRepresentation(definition=rep)
        self.assertTrue(service.GetMultiscaleResult(result_id=result_id).stale)
        lineage = json_format.MessageToDict(service.GetMaterialLineage(material_id="mat-ex-porous-alumina").lineage)
        self.assertTrue(lineage["properties"][0]["stale"])
        self.api.ProjectService.Undo()
        self.api.ProjectService.Undo()

    def test_unsupported_transitions_are_refused(self):
        from resonant_api import ApiError

        with self.assertRaises(ApiError):
            self.api.MultiscaleService.DeriveMaterialProperties(representation_id="rve-porous-alumina", method_id="atomistic.elastic-constants")

    def test_mcp_tools_have_their_levels(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["derive_material_properties"].level, "execute")
        self.assertEqual(TOOLS["assign_derived_properties"].level, "modify")
        self.assertEqual(TOOLS["get_material_lineage"].level, "read")
        for name in ("derive_material_properties", "assign_derived_properties", "get_material_lineage", "iterate_region_coupling"):
            self.contract.method(TOOLS[name].method)
