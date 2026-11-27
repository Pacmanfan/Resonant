"""The help's API reference (help/src/reference/api.md) matches the package (RES-HELP-011)."""
import os
import subprocess
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class ApiReference(unittest.TestCase):
    def test_the_generated_reference_is_current(self):
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "help", "generate_api_reference.py"), "--check"],
                           capture_output=True, text=True, env=dict(os.environ))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


    def test_the_documentation_coverage_report_is_current(self):
        help_dir = os.environ.get("RESONANT_HELP_DIR", os.path.join(ROOT, "build", "debug", "help"))
        if not os.path.exists(os.path.join(help_dir, "index.json")):
            self.skipTest(f"the help is not built in {help_dir}")
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "help", "coverage.py"), "--check", "--help-dir", help_dir],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
