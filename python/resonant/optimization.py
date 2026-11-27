"""Optimization: problems (dicts in the 'resonant.optimization.problem' format),
runs (run records), candidates and promotion."""
from ._native import native


def templates():
    return native.optimization_templates()


def problem_from_template(project, template_id: str) -> dict:
    return native.optimization_problem(project, template_id)


def run(project, problem: dict, progress=None, cancel=None, record: bool = True) -> dict:
    """Run a problem; returns the run record. The design is never changed by a run."""
    return native.run_optimization(project, problem, progress, cancel, record)


def best(run_record: dict):
    """The best feasible candidate of a single-objective run, or None."""
    index = run_record.get("best")
    return None if index is None else run_record["candidates"][index]


def pareto(run_record: dict):
    """The non-dominated feasible candidates (several objectives)."""
    return [run_record["candidates"][i] for i in run_record.get("pareto", [])]


def candidates(run_record: dict, feasible_only: bool = False):
    return [c for c in run_record["candidates"] if c.get("feasible") or not feasible_only]


def promote(project, run_record: dict, candidate) -> None:
    """Write a candidate into the design (recorded as a revision in the project)."""
    native.promote(project, run_record, candidate["id"] if isinstance(candidate, dict) else candidate)
