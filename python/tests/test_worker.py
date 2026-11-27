"""The console worker process and its protocol (RES-PY-*)."""
import json
import os
import signal
import subprocess
import sys
import time
import unittest

import resonant


class WorkerProcess:
    def __init__(self):
        env = dict(os.environ)
        self.p = subprocess.Popen([sys.executable, "-m", "resonant.worker"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True, env=env, bufsize=1)
        self.ready = json.loads(self.p.stdout.readline())
        self.next_id = 0

    def request(self, **request):
        self.next_id += 1
        request["id"] = self.next_id
        self.p.stdin.write(json.dumps(request) + "\n")
        self.p.stdin.flush()
        return self.collect(self.next_id)

    def collect(self, request_id):
        messages = []
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise RuntimeError("the worker ended")
            m = json.loads(line)
            messages.append(m)
            if m.get("type") == "done" and m.get("id") == request_id:
                return messages

    def close(self):
        self.p.stdin.close()
        self.p.wait(10)


class WorkerProtocol(unittest.TestCase):
    def setUp(self):
        self.w = WorkerProcess()

    def tearDown(self):
        self.w.close()

    def test_ready_exec_output_and_results(self):
        self.assertEqual(self.w.ready["protocol"], 1)
        self.assertEqual(self.w.ready["api"], resonant.API_VERSION)
        m = self.w.request(op="exec", code="x = 6 * 7\nprint(x)", mode="exec")
        self.assertIn({"type": "stdout", "text": "42", "id": 1}, m)
        self.assertTrue(m[-1]["ok"])
        m = self.w.request(op="exec", code="x + 1", mode="single")
        self.assertTrue(any(x.get("type") == "stdout" and "43" in x["text"] for x in m))
        m = self.w.request(op="exec", code="for i in range(3):", mode="single")
        self.assertTrue(any(x.get("type") == "incomplete" for x in m))

    def test_errors_carry_line_numbers_and_the_worker_survives(self):
        m = self.w.request(op="exec", code="a = 1\nb = undefined_name\n", mode="exec", filename="script.py")
        error = next(x for x in m if x["type"] == "error")
        self.assertEqual(error["error"], "NameError")
        self.assertEqual(error["line"], 2)
        self.assertFalse(m[-1]["ok"])
        m = self.w.request(op="exec", code="raise SystemExit(3)", mode="exec")
        self.assertEqual(next(x for x in m if x["type"] == "error")["error"], "SystemExit")
        self.assertTrue(self.w.request(op="exec", code="print('still here')", mode="exec")[-1]["ok"])

    def test_completion_inspection_and_the_project_bridge(self):
        p = resonant.Project("Bridge")
        resonant.scene.add_geometry(p, "Box")
        self.w.request(op="set_project", project=json.loads(p.to_json()))
        names = next(x for x in self.w.request(op="complete", text="proj") if x["type"] == "completions")["items"]
        self.assertIn("project", names)
        info = next(x for x in self.w.request(op="inspect", expression="project") if x["type"] == "inspect")
        self.assertEqual(info["valueType"], "Project")
        m = self.w.request(op="exec", code="resonant.scene.add_geometry(project, 'Second')\napp.apply(project, 'add a box')", mode="exec")
        applied = next(x for x in m if x["type"] == "apply")
        self.assertEqual(applied["description"], "add a box")
        self.assertEqual(len(applied["project"]["scene"]["objects"]), 2)

    def test_a_running_script_can_be_interrupted(self):
        self.w.next_id += 1
        rid = self.w.next_id
        self.w.p.stdin.write(json.dumps({"id": rid, "op": "exec", "code": "import time\nwhile True: time.sleep(0.01)", "mode": "exec"}) + "\n")
        self.w.p.stdin.flush()
        time.sleep(0.5)
        self.w.p.send_signal(signal.SIGINT)
        m = self.w.collect(rid)
        self.assertEqual(next(x for x in m if x["type"] == "error")["error"], "KeyboardInterrupt")
        self.assertTrue(self.w.request(op="exec", code="print('after')", mode="exec")[-1]["ok"])


if __name__ == "__main__":
    unittest.main()
