"""Experiment definitions (sensors, actuators, acquisition, calibration
references). Running an experiment with physical outputs is done in the
application, where outputs need an explicit authorization; Python never
starts a physical output."""
from ._native import native


def workspace(project) -> dict:
    return native.experiments(project)


def set_workspace(project, value: dict) -> None:
    native.set_experiments(project, value)


def list_experiments(project):
    return workspace(project).get("experiments", [])


def validate(project, experiment_id: str) -> dict:
    return native.validate_experiment(project, experiment_id)
