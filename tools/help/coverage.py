#!/usr/bin/env python3
"""Documentation coverage report (REQ-CHAT-20261126-010, RES-HELP-012):
for every major user-facing feature, its help topic and that topic's status,
screenshots, example projects, demonstrations and Python API, checked against
the files. Writes docs/help/DOCUMENTATION_COVERAGE.md.

    python3 tools/help/coverage.py [--help-dir build/debug/help] [--check]

--check fails when the report is out of date or a referenced topic, example,
demonstration or Python module does not exist.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "docs", "help", "DOCUMENTATION_COVERAGE.md")

# feature, help topic, example projects (relative to examples/), demonstrations, Python API
FEATURES = [
    ("Projects, scene and selection", "concepts.scene", ["foundation/primitives.resonant"], ["demo-a-overview"], "resonant.project, resonant.scene"),
    ("Geometry", "concepts.geometry", ["foundation/primitives.resonant"], [], "resonant.geometry, resonant.scene.add_geometry"),
    ("Materials", "concepts.materials", ["foundation/primitives.resonant"], ["demo-c-passive-focusing"], "resonant.materials"),
    ("Acoustic arrays", "acoustics.arrays", ["acoustics/b-linear-array.resonant", "acoustics/c-focused-planar-array.resonant"], ["demo-b-acoustic-focusing"], "resonant.scene.add_array"),
    ("Acoustic focusing", "acoustics.focusing", ["acoustics/c-focused-planar-array.resonant"], ["demo-b-acoustic-focusing", "demo-h-python-automation"], "resonant.acoustics.focus"),
    ("Acoustic field simulation and analysis", "acoustics.field-analysis", ["acoustics/a-two-source-interference.resonant"], ["demo-a-overview", "demo-b-acoustic-focusing"], "resonant.acoustics.field, resonant.analysis"),
    ("Reflectors", "acoustics.reflectors", ["manipulators/a-parabolic-reflector.resonant", "manipulators/b-elliptical-reflector.resonant"], ["demo-c-passive-focusing"], "resonant.scene.add_reflector"),
    ("Acoustic lenses", "acoustics.lenses", ["manipulators/c-acoustic-lens.resonant"], [], "-"),
    ("Acoustic waveguides and cavities", "acoustics.waveguides", ["advanced_acoustics/e-resonant-cavity.resonant"], [], "-"),
    ("Acoustic full-wave (FDTD)", "acoustics.fdtd", ["advanced_acoustics/b-reflection.resonant", "advanced_acoustics/f-passive-focusing.resonant"], [], "resonant.simulation.fdtd"),
    ("Mechanical modal analysis", "mechanics.modal", ["mechanics/a-vibrating-beam.resonant", "mechanics/b-vibrating-plate.resonant"], ["demo-d-mechanical-resonance"], "resonant.mechanics.modal"),
    ("Mechanical forced response", "mechanics.forced-response", ["mechanics/c-forced-resonance.resonant"], ["demo-d-mechanical-resonance"], "resonant.mechanics.harmonic"),
    ("Mechanical waves and transients", "mechanics.waves", ["mechanics/d-transient-vibration.resonant", "mechanics/e-wave-propagation.resonant"], [], "-"),
    ("Antennas", "em.antennas", ["electromagnetics/a-half-wave-dipole.resonant", "electromagnetics/b-quarter-wave-monopole.resonant"], ["demo-e-rf-antennas"], "resonant.electromagnetics.antenna"),
    ("Radiation patterns", "em.patterns", ["electromagnetics/a-half-wave-dipole.resonant"], ["demo-e-rf-antennas"], "resonant.electromagnetics.pattern"),
    ("Antenna arrays and beamforming", "em.beamforming", ["electromagnetics/c-linear-array.resonant", "electromagnetics/d-phased-array-steering.resonant"], ["demo-e-rf-antennas"], "resonant.electromagnetics.steer, taper"),
    ("Transmission lines and waveguides", "em.transmission-lines", ["electromagnetics/f-rectangular-waveguide.resonant"], [], "-"),
    ("Electromagnetic full-wave (Yee)", "em.solvers", ["electromagnetics/g-full-wave-scattering.resonant"], [], "-"),
    ("Optimization Studio", "opt.studio", ["optimization/a-acoustic-focal-array.resonant", "optimization/f-sidelobe-reduction.resonant"], ["demo-f-inverse-design"], "resonant.optimization"),
    ("Functional structures", "opt.functional-structures", ["functional/a-microwave-cavity.resonant", "functional/c-acoustic-horn.resonant"], [],
     "resonant_api (FunctionalStructureService)"),
    ("Resonance engineering", "opt.functional-structures", ["resonance/a-helmholtz-resonator.resonant", "resonance/b-coupled-cantilevers.resonant"], [],
     "resonant_api (FunctionalStructureService, ResonanceService)"),
    ("Metamaterials and effective properties", "opt.metamaterials", ["metamaterials/a-mass-in-mass-lattice.resonant", "metamaterials/e-layered-thermal.resonant"], [],
     "resonant_api (FunctionalStructureService.CreateEffectiveMaterial)"),
    ("Multiscale materials and lineage", "concepts.multiscale", ["multiscale/a-crystalline-argon.resonant", "multiscale/c-porous-composite.resonant"], [],
     "resonant_api (MultiscaleService)"),
    ("Energy, momentum and force accounting", "concepts.accounting", ["accounting/d-coupled-resonators.resonant", "accounting/e-microwave-absorption.resonant"], [],
     "resonant_api (AccountingService)"),
    ("Dynamic material state", "concepts.material-state", ["materialstate/c-plastic-bar.resonant", "materialstate/d-damage-resonance.resonant"], [],
     "resonant_api (MaterialStateService)"),
    ("Topology optimization", "opt.topology", [], [], "-"),
    ("Experiments and acquisition", "exp.acquisition", ["experiments/acoustic_experiments.resonant", "experiments/mechanical_experiments.resonant"], [], "resonant.experiments"),
    ("Measurement correlation", "exp.correlation", ["experiments/rf_experiments.resonant"], ["demo-g-experimental-correlation"], "resonant.measurements.compare"),
    ("Output safety", "exp.safety", ["experiments/acoustic_experiments.resonant"], [], "-"),
    ("Python API", "py.api", [], ["demo-h-python-automation"], "resonant (reference: ref.api)"),
    ("Python console", "py.console", [], ["demo-h-python-automation"], "resonant.app"),
    ("Script editor", "py.editor", [], [], "python/examples, python/templates"),
    ("Workflow automation", "py.workflows", [], [], "resonant.automation"),
    ("Plugins", "py.plugins", [], [], "resonant.plugins"),
    ("Help", "ui.help-window", [], ["demo-a-overview"], "-"),
    ("Demo Studio and presenter mode", "ui.demo-studio", [], ["demo-a-overview"], "-"),
]


def generate(help_dir):
    with open(os.path.join(help_dir, "index.json"), encoding="utf-8") as f:
        index = json.load(f)
    topics = {t["id"]: t for t in index["topics"]}
    demos = {}
    for name in os.listdir(os.path.join(ROOT, "demos", "profiles")):
        with open(os.path.join(ROOT, "demos", "profiles", name), encoding="utf-8") as f:
            p = json.load(f)
            demos[p["id"]] = p
    problems = []
    rows = []
    covered = {"topic": 0, "screenshot": 0, "example": 0, "demo": 0, "python": 0}
    for feature, topic_id, examples, demo_ids, python in FEATURES:
        t = topics.get(topic_id)
        if not t:
            problems.append(f"{feature}: no help topic '{topic_id}'")
        for e in examples:
            if not os.path.exists(os.path.join(ROOT, "examples", e)):
                problems.append(f"{feature}: no example '{e}'")
        for d in demo_ids:
            if d not in demos:
                problems.append(f"{feature}: no demonstration '{d}'")
        for module in re.findall(r"resonant\.([a-z_]+)", python):
            if not os.path.exists(os.path.join(ROOT, "python", "resonant", module + ".py")):
                problems.append(f"{feature}: no Python module 'resonant.{module}'")
        shots = t["screenshots"] if t else []
        status = t["status"] if t else "missing"
        demo_text = ", ".join(f"{d.split('-')[1].upper()}{' (partial)' if demos.get(d, {}).get('status') == 'partial' else ''}" for d in demo_ids) or "-"
        rows.append(f"| {feature} | `{topic_id}` | {status} | {', '.join(shots) or '-'} | {', '.join(examples) or '-'} | {demo_text} | {python} |")
        covered["topic"] += bool(t)
        covered["screenshot"] += bool(shots)
        covered["example"] += bool(examples)
        covered["demo"] += bool(demo_ids)
        covered["python"] += python != "-"
    n = len(FEATURES)
    limitations = [t for t in index["topics"] if t["status"] != "available"]
    lines = [
        "# Documentation coverage",
        "",
        "Generated by `tools/help/coverage.py` from the built help (`index.json`), the",
        "screenshot manifest, `demos/profiles/` and the Python package; `python/tests`",
        "fails when this report is out of date. A major user-facing feature is not complete",
        "without a help topic (RES-HELP-012).",
        "",
        f"**{n} features.** Help topic: {covered['topic']}/{n}. Illustrated with a screenshot: {covered['screenshot']}/{n}. "
        f"Example project: {covered['example']}/{n}. Demonstration: {covered['demo']}/{n}. Python API: {covered['python']}/{n}.",
        f"Help: {len(index['topics'])} topics in {len(index['categories'])} categories.",
        "",
        "| Feature | Help topic | Status | Screenshots | Example projects | Demos | Python API |",
        "|---|---|---|---|---|---|---|",
    ] + rows + [
        "",
        "## Topics that are not fully available",
        "",
        "Their pages say what exists and what does not.",
        "",
        "| Topic | Status |",
        "|---|---|",
    ] + [f"| `{t['id']}` ({t['title']}) | {t['status']} |" for t in limitations] + [
        "",
        "## Gaps",
        "",
        "Features without a screenshot are documented in text (and, where a demonstration",
        "exists, shown by it). Features without a demonstration: lenses, waveguides, full-wave",
        "solvers, transients, transmission lines, topology optimization, live acquisition",
        "(every bundled experiment drives outputs, which a demonstration never authorizes).",
        "",
    ]
    return "\n".join(lines), problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--help-dir", default=os.environ.get("RESONANT_HELP_DIR", os.path.join(ROOT, "build", "debug", "help")))
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    text, problems = generate(args.help_dir)
    for p in problems:
        print("coverage:", p, file=sys.stderr)
    if args.check:
        current = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if current != text:
            print("docs/help/DOCUMENTATION_COVERAGE.md is out of date: run tools/help/coverage.py", file=sys.stderr)
            return 1
        return 1 if problems else 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", os.path.relpath(OUT, ROOT), f"({len(problems)} problems)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
