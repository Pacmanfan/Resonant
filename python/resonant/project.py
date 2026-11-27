"""Projects: create, open, save, inspect."""
from ._native import native


def new(name: str = "Untitled"):
    """A new project with the example material library and an empty scene."""
    return native.Project(name)


def open(path: str):  # noqa: A001 - the natural name
    """Open a project file (.resonant)."""
    return native.Project.load(path)


def save(project, path: str) -> None:
    project.save(path)


def summary(project) -> dict:
    """What the project holds: objects by type, materials, settings present."""
    by_type = {}
    for obj in project.objects():
        by_type[obj.type] = by_type.get(obj.type, 0) + 1
    return {"name": project.name, "id": project.id, "objects": by_type, "materials": len(project.materials())}
