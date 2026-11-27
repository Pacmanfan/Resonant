"""Workflows: repeatable procedures made of registered actions
('resonant.workflow' documents)."""
from ._native import native


def templates():
    return native.workflow_templates()


def template(workflow_id: str) -> dict:
    for w in native.workflow_templates():
        if w["id"] == workflow_id:
            return w
    raise KeyError(workflow_id)


def actions():
    return native.actions()


def run(project, workflow, parameters: dict = None, output_dir: str = "", progress=None, cancel=None) -> dict:
    """Run a workflow (an id or a workflow dict); returns the run record. A failed
    required step stops the run (status 'failed'); outputs are by step id."""
    return native.run_workflow(project, workflow, parameters or {}, output_dir, progress, cancel)


def action(project, action_id: str, params: dict = None, output_dir: str = "") -> dict:
    """Run one action directly."""
    return native.run_action(project, action_id, params or {}, output_dir)


def workflow(workflow_id: str, name: str, steps, parameters=None, description: str = "") -> dict:
    """Build a workflow document; steps are dicts {id, action, params, when?, repeat?, dependsOn?}."""
    return {"format": "resonant.workflow", "version": 1, "id": workflow_id, "name": name, "description": description,
            "parameters": parameters or [], "steps": list(steps)}
