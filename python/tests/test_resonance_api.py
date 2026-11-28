"""Resonance through the automation API (REQ-CHAT-20261126-018, RES-RSN-013).

Against a real headless host: resonance identification of a ring-down signal
and a refusal of a record too short; the coupled-cantilever example analyzed
as a job whose result carries its in-phase and anti-phase modes with their
loss breakdown; the MCP adapter's tool. Skipped when this build has no
automation API.
"""

import json
import math
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

HOST = os.environ.get("RESONANT_AUTOMATION_HOST", "")
DESCRIPTORS = os.environ.get("RESONANT_API_DESCRIPTORS", "")
AVAILABLE = bool(HOST and DESCRIPTORS and os.path.isfile(HOST) and os.path.isfile(DESCRIPTORS))
EXAMPLE = pathlib.Path(__file__).resolve().parents[2] / "examples" / "resonance" / "b-coupled-cantilevers.resonant"


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class ResonanceApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import connect, load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-rs-"))
        projects = cls.dir / "projects"
        projects.mkdir()
        shutil.copy(EXAMPLE, projects / "cantilevers.resonant")
        socket_path = cls.dir / "automation.sock"
        cls.host = subprocess.Popen([HOST, "--socket", str(socket_path), "--project-dir", str(projects), "--allow-files"],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        info = json.loads(cls.host.stdout.readline())
        token = pathlib.Path(info["tokenFile"]).read_text().strip()
        cls.api = connect(socket_path=info["socket"], token=token, client_name="resonance-test", contract=cls.contract,
                          permissions=["read", "modify", "execute", "files"])
        cls.api.ProjectService.OpenProject(file_name="cantilevers.resonant", discard_unsaved_changes=True)

    @classmethod
    def tearDownClass(cls):
        cls.api.close()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def test_identify_a_ring_down_and_refuse_a_short_record(self):
        from resonant_api import ApiError

        dt, f0, q = 1e-5, 1200.0, 60.0
        samples = [math.exp(-math.pi * f0 / q * i * dt) * math.cos(2 * math.pi * f0 * i * dt) for i in range(5000)]
        r = self.api.ResonanceService.IdentifyResonances(samples=samples, sample_interval=dt, min_frequency_hz=200, max_frequency_hz=5000)
        self.assertEqual(r.method, "matrix-pencil")
        self.assertEqual(len(r.resonances), 1)
        self.assertAlmostEqual(r.resonances[0].frequency_hz, f0, places=4)
        self.assertAlmostEqual(r.resonances[0].quality, q, places=4)
        with self.assertRaises(ApiError):
            self.api.ResonanceService.IdentifyResonances(samples=samples[:60], sample_interval=dt, min_frequency_hz=1000)

    def test_coupled_cantilevers_report_their_hybrid_modes(self):
        job = self.api.FunctionalStructureService.AnalyzeStructure(structure_id="fs-coupled-cantilevers").job
        finished = self.api.wait_for_job(job.job_id, timeout_seconds=120)
        self.assertTrue(finished.results, finished.error.message if finished.HasField("error") else "")
        result = self.api.FunctionalStructureService.GetFunctionalResult(result_id=finished.results[0].id).result
        labels = [m.label for m in result.modes]
        self.assertEqual(labels[:2], ["in-phase", "anti-phase"])
        self.assertEqual(result.modes[0].nature, "natural")
        self.assertTrue(any(k.startswith("structural") for k in result.modes[0].loss_quality))
        metrics = {m.key: m.value for m in result.metrics}
        self.assertGreater(metrics["coupled.splitting"], 0.0)
        self.assertGreater(metrics["coupled.transferFraction"], 0.9)

    def test_mcp_offers_identification_for_reading(self):
        from resonant_api.mcp import TOOLS

        self.assertEqual(TOOLS["identify_resonances"].level, "read")
        self.contract.method(TOOLS["identify_resonances"].method)


if __name__ == "__main__":
    unittest.main()
