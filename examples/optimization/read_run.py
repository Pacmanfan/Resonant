#!/usr/bin/env python3
"""Reads an optimization run exported from Resonant (Optimization Studio,
Candidates tab, "Export Run...") and summarizes it.

The run record ("resonant.optimization.run", version 1) is a versioned JSON
interface: the problem as it was run, every candidate with its design values,
objective values, constraint results and the exact model that evaluated it.
Only the Python standard library is needed; with NumPy and matplotlib
installed, --plot draws the objective values (a Pareto view for two
objectives).

    python3 examples/optimization/read_run.py run.json [--plot]
"""
import json
import sys


def load(path):
    with open(path, encoding="utf-8") as f:
        record = json.load(f)
    if record.get("format") != "resonant.optimization.run":
        raise SystemExit(f"{path}: not a Resonant optimization run record")
    if record.get("version", 0) > 1:
        raise SystemExit(f"{path}: run record version {record['version']} is newer than this reader")
    return record


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    record = load(argv[1])
    problem = record["problem"]
    objectives = [o.get("label") or o["id"] for o in problem.get("objectives", [])]
    candidates = record.get("candidates", [])
    feasible = [c for c in candidates if c.get("feasible")]
    models = sorted({c["evaluation"]["model"].get("modelId", "") for c in candidates})
    print(f"run {record['runId']}: {problem.get('name', '')} - {record['algorithm']}, {len(candidates)} candidates, "
          f"{len(feasible)} feasible, models: {', '.join(models)}")
    print(f"stopped: {record['termination']['reason']}")
    if record.get("best") is not None:
        best = candidates[record["best"]]
        print(f"best: {best['id']} {dict(zip(objectives, best['objectiveValues']))}")
    front = [candidates[i] for i in record.get("pareto", [])]
    if len(objectives) > 1:
        print(f"Pareto front: {len(front)} non-dominated feasible candidates (no single one is optimal for every objective)")
    if "--plot" in argv:
        try:
            import numpy as np
            import matplotlib.pyplot as plt
        except ImportError:
            print("--plot needs NumPy and matplotlib")
            return 1
        values = np.array([c["objectiveValues"] for c in feasible], dtype=float)
        if len(objectives) >= 2:
            plt.scatter(values[:, 0], values[:, 1], s=8, label="feasible")
            f = np.array([c["objectiveValues"] for c in front], dtype=float)
            if len(f):
                order = np.argsort(f[:, 0])
                plt.plot(f[order, 0], f[order, 1], "o-", color="tab:orange", label="Pareto front")
            plt.xlabel(objectives[0])
            plt.ylabel(objectives[1])
        else:
            plt.plot(values[:, 0], ".")
            plt.xlabel("feasible candidate")
            plt.ylabel(objectives[0])
        plt.legend()
        plt.title(problem.get("name", record["runId"]))
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
