"""Materials of a project's library. Values carry their provenance; the
example library is illustrative, not authoritative."""


def list_materials(project, domain: str = None):
    out = project.materials()
    if domain is not None:
        out = [m for m in out if domain in m.get("properties", {})]
    return out


def get(project, reference: str) -> dict:
    return project.material(reference)


def add(project, material: dict) -> None:
    project.add_material(material)
