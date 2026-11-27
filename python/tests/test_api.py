"""Tests of the Resonant Python API (REQ-CHAT-20261126-010, RES-PY-*).

Run with the native module on the path:
    RESONANT_PYTHON_MODULE_DIR=build/debug/src/python PYTHONPATH=python python3 -m unittest discover -s python/tests
"""
import gc
import json
import os
import tempfile
import threading
import unittest

import resonant

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def acoustic_project():
    p = resonant.Project("Acoustic")
    array = resonant.scene.add_array(p, "Array", count=8, spacing=0.0043, object_id="py-array")
    target = resonant.scene.add_target(p, "Focus", position=(0.01, 0.0, 0.06), object_id="py-focus")
    region = resonant.scene.add_region(p, "Plane", size=(0.08, 0.01, 0.09), samples=(41, 33), position=(0, 0, 0.035), object_id="py-plane")
    return p, array, target, region


class ApiBasics(unittest.TestCase):
    def test_versions_and_requirements(self):
        self.assertEqual(resonant.API_VERSION, "1.0")
        self.assertTrue(resonant.CORE_VERSION)
        resonant.require(1, 0)
        with self.assertRaises(resonant.Error):
            resonant.require(2)
        with self.assertRaises(resonant.Error):
            resonant.require(1, 9)

    def test_objects_have_safe_lifetimes(self):
        p = resonant.Project("Lifetime")
        box = resonant.scene.add_geometry(p, "Box", parameters={"width": 0.2})
        del p
        gc.collect()
        self.assertTrue(box.valid)  # the reference keeps its project alive
        self.assertEqual(box.parameters["width"], 0.2)
        q = resonant.Project("Remove")
        b = resonant.scene.add_geometry(q, "B")
        q.remove(b.id)
        self.assertFalse(b.valid)
        with self.assertRaises(resonant.Error):
            _ = b.name

    def test_scene_construction_and_validation(self):
        p = resonant.Project("Scene")
        for kind in resonant.scene.types():
            obj = p.add({"type": kind, "name": f"obj {kind}"})
            self.assertTrue(obj.valid, kind)
        box = p.object("obj geometry")
        box.position = (1.0, 2.0, 3.0)
        box.set_parameters({"width": 0.5, "depth": 0.25})
        self.assertEqual(box.position, [1.0, 2.0, 3.0])
        self.assertEqual(box.parameters["depth"], 0.25)
        with self.assertRaises(resonant.Error):
            box.set_parameters({"width": -1.0})
        emitter = p.object("obj emitter")
        emitter.set_component({"amplitudePa": 3.0})
        self.assertEqual(emitter.component["amplitudePa"], 3.0)
        with self.assertRaises(resonant.Error):
            emitter.set_component({"amplitudePa": -3.0})
        with self.assertRaises(resonant.Error):
            p.object("no such object")
        with self.assertRaises(resonant.Error):
            p.add({"type": "warp-drive"})

    def test_save_load_materials_and_settings(self):
        p, *_ = acoustic_project()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "p.resonant")
            p.save(path)
            q = resonant.project.open(path)
        self.assertEqual(len(q.objects()), len(p.objects()))
        self.assertTrue(any(m["name"].lower().startswith("alumin") for m in resonant.materials.list_materials(q)))
        resonant.acoustics.set_frequency(q, 25000.0)
        self.assertEqual(q.settings("acoustics")["frequencyHz"], 25000.0)
        with self.assertRaises(resonant.Error):
            q.set_settings("acoustics", {"frequencyHz": -1.0})
        summary = resonant.project.summary(q)
        self.assertEqual(summary["objects"]["acoustic.array"], 1)
        copy = q.copy()
        copy.remove("Array")
        self.assertEqual(len(q.objects()), 3)  # the copy is independent


class Simulations(unittest.TestCase):
    def test_acoustic_field_and_focusing(self):
        p, array, target, region = acoustic_project()
        before = abs(resonant.acoustics.pressure(p, [(0.01, 0, 0.06)])[0])
        field = resonant.acoustics.field(p, region)
        self.assertEqual(field["pressure"].shape, (1, 33, 41))
        self.assertGreater(field["max_pressure"], 0.0)
        resonant.acoustics.focus(p, array, target)
        after = abs(resonant.acoustics.pressure(p, [(0.01, 0, 0.06)])[0])
        self.assertGreater(after, 2.0 * before)
        peak = resonant.analysis.peak(resonant.acoustics.field(p, region))
        self.assertGreater(peak["max_pressure"], field["max_pressure"])

    def test_mechanics_and_electromagnetics(self):
        p = resonant.Project("Mechanics")
        resonant.scene.add_structure(p, "Beam", parameters={"width": 0.5, "depth": 0.03, "height": 0.005},
                                     supports=["clamped", "free", "free", "free", "free", "free"])
        f = resonant.mechanics.modal(p, modes=3)["frequencies"]
        self.assertEqual(len(f), 3)
        self.assertAlmostEqual(f[0], 16.6, delta=1.0)  # Euler-Bernoulli: 16.5 Hz for this aluminium cantilever
        q = resonant.Project("EM")
        dipole = resonant.scene.add_antenna(q, "Dipole", parameters={"length": 0.15, "wireRadius": 0.001})
        report = resonant.electromagnetics.antenna(q, dipole)
        self.assertAlmostEqual(report["inputResistance"], 73.0, delta=8.0)
        pattern = resonant.electromagnetics.pattern(q, dipole)
        self.assertAlmostEqual(float(pattern["magnitude"].max()), 1.0, places=6)
        array = resonant.scene.add_em_array(q, "Array", count=10, spacing=0.15)
        before = resonant.electromagnetics.pattern_metrics(q, array)["sidelobeDb"]
        resonant.electromagnetics.taper(q, array, 30.0)
        after = resonant.electromagnetics.pattern_metrics(q, array)["sidelobeDb"]
        self.assertLess(after, before - 10.0)

    def test_long_running_simulations_can_be_cancelled(self):
        p, array, target, region = acoustic_project()
        calls = []

        def progress(fraction, message):
            calls.append(fraction)

        with self.assertRaises(resonant.Error) as raised:
            resonant.simulation.fdtd(p, region, progress=progress, cancel=lambda: len(calls) > 2)
        self.assertIn("cancel", str(raised.exception))
        job = resonant.simulation.Background(resonant.acoustics.field, p, region)
        self.assertEqual(job.wait(60)["pressure"].shape, (1, 33, 41))

    def test_pressure_at_many_points_follows_spherical_spreading(self):
        p = resonant.Project("Spreading")
        resonant.scene.add_emitter(p, "Source")  # 1 Pa at 1 m
        values = [abs(v) for v in resonant.acoustics.pressure(p, [(0, 0, 0.05), (0, 0, 0.1), (0, 0, 0.4), (1, 0, 0)])]
        for got, want in zip(values, [20.0, 10.0, 2.5, 1.0]):
            self.assertAlmostEqual(got, want, places=9)

    def test_errors_are_reported_not_hidden(self):
        p = resonant.Project("Errors")
        with self.assertRaises(resonant.Error):
            resonant.acoustics.field(p, "missing region")
        region = resonant.scene.add_region(p, "Plane")
        with self.assertRaises(resonant.Error) as raised:
            resonant.acoustics.field(p, region)
        self.assertIn("source", str(raised.exception))


class OptimizationAndAutomation(unittest.TestCase):
    def test_optimization_and_promotion(self):
        p, array, target, region = acoustic_project()
        problem = resonant.optimization.problem_from_template(p, "acoustic.array-phase-focus")
        problem["algorithm"]["maxEvaluations"] = 300
        design_before = p.object("Array").component
        progress = []
        run = resonant.optimization.run(p, problem, progress=lambda f, m: progress.append(f))
        self.assertTrue(progress)
        self.assertEqual(p.object("Array").component, design_before)  # a run never changes the design
        best = resonant.optimization.best(run)
        self.assertIsNotNone(best)
        self.assertGreater(best["objectiveValues"][0], run["candidates"][0]["objectiveValues"][0])
        resonant.optimization.promote(p, run, best)
        self.assertNotEqual(p.object("Array").component, design_before)
        record = json.loads(p.to_json())["extensions"]["optimization"]
        self.assertEqual(len(record["revisions"]), 1)
        self.assertEqual(len(record["runs"]), 1)

    def test_workflows_run_with_real_actions(self):
        ids = [w["id"] for w in resonant.automation.templates()]
        self.assertIn("em.dipole-pattern", ids)
        p = resonant.Project("Workflow")
        with tempfile.TemporaryDirectory() as d:
            run = resonant.automation.run(p, "em.dipole-pattern", output_dir=d)
            self.assertEqual(run["status"], "completed", run["message"])
            self.assertTrue(os.path.exists(os.path.join(d, "dipole-pattern.csv")))
        custom = resonant.automation.workflow("custom", "Custom", [
            {"id": "box", "action": "scene.add", "params": {"type": "geometry", "name": "Box"}},
            {"id": "fail", "action": "workflow.check", "params": {"value": 5, "max": 1, "message": "five is too many"}},
            {"id": "never", "action": "scene.add", "params": {"type": "geometry", "name": "Never"}}])
        run = resonant.automation.run(p, custom)
        self.assertEqual(run["status"], "failed")
        self.assertIn("five is too many", run["message"])
        self.assertEqual([s["step"] for s in run["steps"]], ["box", "fail"])
        self.assertTrue(any(a["id"] == "acoustics.field" for a in resonant.automation.actions()))

    def test_experiment_definitions(self):
        p = resonant.project.open(os.path.join(ROOT, "examples", "experiments", "mechanical_experiments.resonant"))
        experiments = resonant.experiments.list_experiments(p)
        self.assertTrue(experiments)
        report = resonant.experiments.validate(p, experiments[0]["id"])
        self.assertIn("ok", report)
        self.assertAlmostEqual(resonant.measurements.compare([1.0, 2.0], [1.0, 2.5])["maxError"], 0.5)


class Reporting(unittest.TestCase):
    def test_reports_and_exports(self):
        p, array, target, region = acoustic_project()
        field = resonant.acoustics.field(p, region)
        with tempfile.TemporaryDirectory() as d:
            resonant.io.export_field_csv(field, os.path.join(d, "field.csv"))
            rows = resonant.io.read_csv(os.path.join(d, "field.csv"))
            self.assertEqual(len(rows), 41 * 33)
            resonant.reporting.write_html(os.path.join(d, "r.html"), "Report", [("Field", {"max": field["max_pressure"]})])
            self.assertIn("Report", open(os.path.join(d, "r.html"), encoding="utf-8").read())
        self.assertAlmostEqual(resonant.analysis.width_6db([0, 0.2, 1.0, 0.2, 0], 0.01), 0.02)


class Threads(unittest.TestCase):
    def test_independent_projects_in_parallel_threads(self):
        results = {}

        def work(name):
            p, array, target, region = acoustic_project()
            results[name] = resonant.acoustics.field(p, region)["max_pressure"]

        threads = [threading.Thread(target=work, args=(i,)) for i in range(3)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(len(set(round(v, 12) for v in results.values())), 1)


if __name__ == "__main__":
    unittest.main()
