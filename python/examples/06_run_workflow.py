"""Run a built-in workflow template and read its outputs.

Workflows are the same as in Tools > Workflows: named steps that build,
analyse and report. This runs the electromagnetic "array steering" template
on a new project and prints what each step produced.
"""
import os
import tempfile

import resonant

OUT = os.environ.get("RESONANT_EXAMPLE_OUTPUT", os.path.join(tempfile.gettempdir(), "resonant-examples"))
print("templates:", ", ".join(w["id"] for w in resonant.automation.templates()))
p = resonant.Project("Workflow")
run = resonant.automation.run(p, "em.array-steering", {"steerPhi": 60}, output_dir=os.path.join(OUT, "workflow"))
print("status:", run["status"], run["message"])
for step in run["steps"]:
    print(f"  {step['step']:<12} {step['status']:<8} {step['seconds']:.3f} s")
