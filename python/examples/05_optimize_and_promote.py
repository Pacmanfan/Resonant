"""Run an optimization from a template and promote the best design.

Uses the Optimization Studio's "array phase focus" template: the element
phases of an array are the design variables and the pressure at the target
is the objective. The run is recorded in the project; promoting the best
candidate changes the design (and is recorded as a design revision).
"""
import resonant
from resonant import app

p = project if "project" in globals() else resonant.Project("Optimization")  # noqa: F821
if not any(o.component_type == "acoustic.array" for o in p.objects()):
    resonant.scene.add_array(p, "Array", count=8, spacing=0.0043)
    resonant.scene.add_target(p, "Focus", position=(0.01, 0.0, 0.06))

problem = resonant.optimization.problem_from_template(p, "acoustic.array-phase-focus")
problem["algorithm"]["maxEvaluations"] = 300
run = resonant.optimization.run(p, problem, progress=lambda f, m: None)
best = resonant.optimization.best(run)
first = run["candidates"][0]["objectiveValues"][0]
print(f"{len(run['candidates'])} candidates; objective {first:.3f} -> {best['objectiveValues'][0]:.3f} ({best['id']})")
resonant.optimization.promote(p, run, best)
print("promoted:", best["id"])
if app.running_in_console():
    app.apply(p, "Optimized array phases")
