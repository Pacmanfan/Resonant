"""Every example script runs to the end against this build (RES-PY-011)."""
import glob
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class ExampleScripts(unittest.TestCase):
    def test_every_example_runs(self):
        scripts = sorted(glob.glob(os.path.join(ROOT, "python", "examples", "*.py")))
        self.assertGreaterEqual(len(scripts), 7)
        with tempfile.TemporaryDirectory() as out:
            env = dict(os.environ, RESONANT_EXAMPLE_OUTPUT=out, MPLBACKEND="Agg")
            for script in scripts:
                with self.subTest(script=os.path.basename(script)):
                    r = subprocess.run([sys.executable, script], env=env, capture_output=True, text=True, timeout=300)
                    self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertTrue(os.path.exists(os.path.join(out, "focal-plane.csv")))
            self.assertTrue(os.path.exists(os.path.join(out, "report.html")))

    def test_templates_compile(self):
        for script in glob.glob(os.path.join(ROOT, "python", "templates", "*.py")):
            with open(script, encoding="utf-8") as f:
                compile(f.read(), script, "exec")


if __name__ == "__main__":
    unittest.main()
