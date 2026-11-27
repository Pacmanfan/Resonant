"""Scene construction. Every helper returns an ObjectRef; ``add(project, dict)``
is the general form (see ``resonant.native_types()`` for the types)."""
from ._native import native


def add(project, description: dict):
    return project.add(description)


def _place(d, position, rotation, material=None, object_id=None):
    if position is not None:
        d["position"] = list(position)
    if rotation is not None:
        d["rotation"] = list(rotation)
    if material is not None:
        d["material"] = material
    if object_id is not None:
        d["id"] = object_id
    return d


def add_geometry(project, name, kind="box", parameters=None, position=None, rotation=None, material=None, object_id=None):
    return project.add(_place({"type": "geometry", "name": name, "kind": kind, "parameters": parameters or {}}, position, rotation, material, object_id))


def add_emitter(project, name, frequency=40000.0, amplitude=1.0, phase=0.0, position=None, rotation=None, object_id=None, **settings):
    component = {"frequencyHz": frequency, "amplitudePa": amplitude, "phaseRad": phase, **settings}
    return project.add(_place({"type": "emitter", "name": name, "component": component}, position, rotation, None, object_id))


def add_array(project, name, count=8, spacing=0.0043, layout="linear", frequency=40000.0, rows=1, position=None, rotation=None, object_id=None, **settings):
    component = {"layout": layout, "countX": count, "countY": rows, "spacingX": spacing, "spacingY": spacing, "element": {"frequencyHz": frequency}, **settings}
    return project.add(_place({"type": "array", "name": name, "component": component}, position, rotation, None, object_id))


def add_target(project, name, position=(0.0, 0.0, 0.1), amplitude=1.0, suppress=False, object_id=None):
    return project.add(_place({"type": "target", "name": name, "component": {"desiredAmplitudePa": amplitude, "suppress": suppress}}, position, None, None, object_id))


def add_region(project, name, size=(0.1, 0.01, 0.1), samples=(61, 61), kind="planeXZ", position=None, object_id=None):
    component = {"kind": kind, "size": list(size), "countU": samples[0], "countV": samples[1]}
    return project.add(_place({"type": "region", "name": name, "component": component}, position, None, None, object_id))


def add_reflector(project, name, kind="reflector.parabolic", parameters=None, position=None, rotation=None, material="mat-aluminum", object_id=None):
    return project.add(_place({"type": "manipulator", "name": name, "kind": kind, "parameters": parameters or {}}, position, rotation, material, object_id))


def add_structure(project, name, kind="box", parameters=None, idealization="beam", supports=None, position=None, material="mat-aluminum", object_id=None):
    component = {"idealization": idealization}
    if supports is not None:
        component["supports"] = list(supports)
    return project.add(_place({"type": "structure", "name": name, "kind": kind, "parameters": parameters or {}, "component": component},
                              position, None, material, object_id))


def add_actuator(project, name, position, amplitude=1.0, phase=0.0, object_id=None):
    return project.add(_place({"type": "actuator", "name": name, "component": {"amplitude": amplitude, "phase": phase}}, position, None, None, object_id))


def add_sensor(project, name, position, object_id=None):
    return project.add(_place({"type": "sensor", "name": name}, position, None, None, object_id))


def add_antenna(project, name, kind="antenna.dipole", parameters=None, frequency=1e9, position=None, rotation=None, material="mat-copper", object_id=None):
    return project.add(_place({"type": "antenna", "name": name, "kind": kind, "parameters": parameters or {}, "component": {"frequencyHz": frequency}},
                              position, rotation, material, object_id))


def add_em_array(project, name, count=8, spacing=0.15, frequency=1e9, pattern="halfWaveDipole", layout="linear", rows=1, position=None, object_id=None):
    component = {"layout": layout, "countX": count, "countY": rows, "spacingX": spacing, "spacingY": spacing, "frequencyHz": frequency, "pattern": pattern}
    return project.add(_place({"type": "em-array", "name": name, "component": component}, position, None, None, object_id))


def find(project, reference):
    return project.object(reference)


def remove(project, obj) -> None:
    project.remove(obj.id if hasattr(obj, "id") else obj)


def types():
    """The object types ``add`` accepts."""
    return native.object_types()
