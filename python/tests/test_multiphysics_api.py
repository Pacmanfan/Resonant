"""The Coupled Energy-Matter Engine through the automation API (REQ-CHAT-20261126-016, RES-CEME-015).

Against a real headless host: a material with electromagnetic, thermal and
mechanical properties, a physical system stored in the project (undoable),
validation that refuses a planned coupling, a coupled simulation as a job,
and its result (status, iterations, the conservation audit and transfers);
the MCP adapter offers the new tools at their permission levels. Skipped
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


def _system(material_id, scheme="iterative", couplings=None):
    return {
        "format": "resonant.physical-system", "version": 1, "id": "sys-heated-slab", "name": "Heated slab",
        "geometry": {"kind": "layered-slab", "lateralArea": 0.01},
        "environment": {"ambientTemperature": 293.15},
        "regions": [{"id": "slab", "name": "Slab", "materialId": material_id, "thickness": 0.01, "cells": 40}],
        "domains": [
            {"id": "em", "kind": "electromagnetic", "solver": "em.layered.transfer-matrix", "settings": {"frequencyHz": 2.45e9, "incidentIntensity": 2e4}},
            {"id": "heat", "kind": "thermal", "solver": "thermal.conduction.fv",
             "settings": {"faces": {"-z": {"kind": "convective", "h": 20}, "+z": {"kind": "convective", "h": 20}}}},
        ],
        "couplings": couplings if couplings is not None else [
            {"id": "absorb", "model": "em.absorption-to-heat", "from": "em", "to": "heat"},
            {"id": "loss", "model": "thermal.temperature-to-em-properties", "from": "heat", "to": "em"},
        ],
        "materialLaws": [{"region": "slab", "property": "electromagnetic.lossTangent", "law": "linear", "value": 0.05, "reference": 293.15,
                          "slope": 2e-4, "validity": [250, 700], "provenance": "illustrative", "source": "test"}],
        "scheme": {"kind": scheme, "maxIterations": 50, "temperatureTolerance": 1e-5},
    }


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class MultiphysicsApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-mp-"))
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="multiphysics-test", contract=cls.contract)
        props = []
        for domain, values in {"electromagnetic": {"relativePermittivity": 4.0, "lossTangent": 0.05, "relativePermeability": 1.0},
                               "thermal": {"density": 1500.0, "thermalConductivity": 0.5, "specificHeat": 1000.0, "thermalExpansion": 5e-5},
                               "mechanical": {"density": 1500.0, "youngsModulus": 3e9, "poissonsRatio": 0.35}}.items():
            for key, value in values.items():
                props.append({"domain": domain, "key": key, "value_si": value, "provenance": "MATERIAL_PROVENANCE_MEASURED", "source": "test"})
        made = cls.api.MaterialService.CreateMaterial(name="Lossy dielectric", properties=props)
        cls.material = made.material.material_id

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_coupling_models_state_their_status(self):
        models = {m.model_id: m for m in self.api.MultiphysicsService.ListCouplingModels().models}
        self.assertEqual(models["em.absorption-to-heat"].status, "validated")
        self.assertEqual(models["em.absorption-to-heat"].source_quantity, "absorbed_power_density")
        self.assertEqual(models["em.lorentz-force"].status, "planned")
        self.assertEqual(models["mechanics.structure-to-acoustic.rayleigh"].status, "external")
        self.assertTrue(all(m.equations for m in models.values()))

    def test_store_validate_run_and_read_a_coupled_simulation(self):
        from google.protobuf import json_format

        system = _system(self.material)
        stored = self.api.MultiphysicsService.SetPhysicalSystem(definition=system)
        self.assertEqual(stored.system.system_id, "sys-heated-slab")
        listed = self.api.MultiphysicsService.ListPhysicalSystems().systems
        self.assertIn("sys-heated-slab", [s.system_id for s in listed])
        valid = self.api.MultiphysicsService.ValidatePhysicalSystem(system_id="sys-heated-slab").validation
        self.assertTrue(valid.valid, [i.message for i in valid.issues])

        planned = _system(self.material, couplings=[{"id": "f", "model": "em.lorentz-force", "from": "em", "to": "heat"}])
        refused = self.api.MultiphysicsService.ValidatePhysicalSystem(definition=planned).validation
        self.assertFalse(refused.valid)
        self.assertTrue(any("planned" in i.message for i in refused.issues))

        job = self.api.MultiphysicsService.StartCoupledSimulation(system_id="sys-heated-slab").job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=60)
        state = finished.DESCRIPTOR.fields_by_name["state"].enum_type.values_by_number[finished.state].name
        self.assertEqual(state, "JOB_STATE_COMPLETED", finished.error.message if finished.HasField("error") else "")
        self.assertTrue(finished.results, "the finished job names its result")
        result = self.api.MultiphysicsService.GetCoupledResult(result_id=finished.results[0].id, include_document=True).result
        self.assertEqual(result.status, "converged", result.message)
        self.assertGreater(result.passes, 2)
        self.assertTrue(result.audit.balanced)
        self.assertLess(abs(result.audit.relative_residual), 1e-9)
        self.assertEqual(len(result.transfers), 1)
        self.assertEqual(result.transfers[0].method, "conservative")
        self.assertLess(result.transfers[0].relative_difference, 1e-12)
        document = json_format.MessageToDict(result.document)
        temperature = document["fields"]["heat.temperature"]
        self.assertGreater(temperature["max"], 293.15 + 5.0)
        self.assertEqual(document["provenance"]["system"]["id"], "sys-heated-slab")

    def test_setting_a_system_is_undoable(self):
        system = _system(self.material, scheme="one-way", couplings=[{"id": "absorb", "model": "em.absorption-to-heat", "from": "em", "to": "heat"}])
        system["id"] = "sys-undo"
        self.api.MultiphysicsService.SetPhysicalSystem(definition=system)
        self.assertIn("sys-undo", [s.system_id for s in self.api.MultiphysicsService.ListPhysicalSystems().systems])
        self.api.ProjectService.Undo()
        self.assertNotIn("sys-undo", [s.system_id for s in self.api.MultiphysicsService.ListPhysicalSystems().systems])

    def test_mcp_offers_the_engine_by_level(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["start_coupled_simulation"].level, "execute")
        self.assertEqual(TOOLS["set_physical_system"].level, "modify")
        self.assertEqual(TOOLS["get_coupled_result"].level, "read")
        for spec in TOOLS.values():
            self.contract.method(spec.method)  # every tool names a method of the contract


if __name__ == "__main__":
    unittest.main()
