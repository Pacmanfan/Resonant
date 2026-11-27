"""The example Python plugins and the plugin API (RES-PLUG-*)."""
import json
import os
import subprocess
import sys
import unittest

import resonant
from resonant import plugins

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EXAMPLES = os.path.join(ROOT, "plugins", "examples")
BROKEN = os.path.join(ROOT, "tests", "data", "plugins", "broken-python")


class PythonPlugins(unittest.TestCase):
    def test_geometry_generator_adds_a_real_array(self):
        api = plugins.load_plugin(os.path.join(EXAMPLES, "spiral-array-generator"))
        ext = api.extensions["fermat-spiral-array"]
        self.assertEqual(ext["kind"], "geometry-generator")
        p = resonant.Project("Spiral")
        result = ext["function"](p, count=24, radius=0.02)
        array = p.object(result["id"])
        self.assertEqual(array.component["layout"], "custom")
        self.assertEqual(len(array.component["customPositions"]), 24)
        resonant.scene.add_target(p, "Focus", position=(0, 0, 0.05), object_id="f")
        resonant.acoustics.focus(p, array, "f")
        self.assertGreater(abs(resonant.acoustics.pressure(p, [(0, 0, 0.05)])[0]), 0)

    def test_analysis_extension_reports_and_changes_nothing(self):
        api = plugins.load_plugin(os.path.join(EXAMPLES, "array-report"))
        p = resonant.Project("Arrays")
        resonant.scene.add_array(p, "Acoustic", count=8, spacing=0.006)
        resonant.scene.add_em_array(p, "Radio", count=8, spacing=0.2)
        before = p.to_json()
        report = api.extensions["array-report"]["function"](p)
        self.assertEqual(p.to_json(), before)
        rows = {r["object"]: r for r in report["arrays"]}
        self.assertTrue(rows["Acoustic"]["gratingLobeRisk"])  # 6 mm > lambda / 2 at 40 kHz
        self.assertTrue(rows["Radio"]["gratingLobeRisk"])     # 0.2 m > lambda / 2 at 1 GHz
        self.assertIn("sidelobeDb", rows["Radio"])

    def test_importer_compares_compatible_quantities(self):
        api = plugins.load_plugin(os.path.join(EXAMPLES, "csv-field-importer"))
        p = resonant.Project("Import")
        resonant.scene.add_emitter(p, "Source")
        result = api.extensions["csv-field"]["function"](os.path.join(EXAMPLES, "csv-field-importer", "synthetic-field.csv"), p)
        self.assertEqual(result["label"], "imported measurement (from file)")
        self.assertEqual(result["points"], 4)
        self.assertLess(result["comparison"]["relativeRms"], 0.05)

    def test_a_failing_plugin_raises_with_its_reason(self):
        with self.assertRaises(RuntimeError) as raised:
            plugins.load_plugin(BROKEN)
        self.assertIn("fails on purpose", str(raised.exception))

    def test_the_worker_loads_plugins_and_reports_failures(self):
        w = subprocess.Popen([sys.executable, "-m", "resonant.worker"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        try:
            json.loads(w.stdout.readline())  # ready
            w.stdin.write(json.dumps({"id": 1, "op": "load_plugins", "plugins": [os.path.join(EXAMPLES, "array-report"), BROKEN]}) + "\n")
            w.stdin.flush()
            messages = []
            while not messages or messages[-1].get("type") != "done":
                messages.append(json.loads(w.stdout.readline()))
            loaded = next(m for m in messages if m["type"] == "plugins")["plugins"]
            self.assertTrue(loaded[0]["ok"])
            self.assertEqual(loaded[0]["extensions"][0]["id"], "array-report")
            self.assertFalse(loaded[1]["ok"])
            self.assertIn("fails on purpose", loaded[1]["error"])
            p = resonant.Project("W")
            resonant.scene.add_em_array(p, "Radio", count=4, spacing=0.1)
            w.stdin.write(json.dumps({"id": 2, "op": "set_project", "project": json.loads(p.to_json())}) + "\n")
            w.stdin.write(json.dumps({"id": 3, "op": "call", "plugin": "examples.array-report", "extension": "array-report"}) + "\n")
            w.stdin.flush()
            replies = []
            while len([m for m in replies if m.get("type") == "done"]) < 2:
                replies.append(json.loads(w.stdout.readline()))
            result = next(m for m in replies if m["type"] == "result")["value"]
            self.assertEqual(result["arrays"][0]["object"], "Radio")
        finally:
            w.stdin.close()
            w.wait(10)


if __name__ == "__main__":
    unittest.main()
