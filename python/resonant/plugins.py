"""The Python plugin API (plugin API version 1).

A Python plugin is a directory with a ``plugin.json`` manifest ("kind":
"python", "entry": the module file) whose module defines
``register(api)``. Through ``api`` it adds extensions:

* ``api.add_geometry_generator(id, title, function, parameters=None)`` —
  ``function(project, **parameters)`` adds objects to the project;
* ``api.add_analysis(id, title, function)`` — ``function(project)`` returns
  a dict (shown as a result) and must not change the project;
* ``api.add_importer(id, title, extensions, function)`` —
  ``function(path, project)`` reads a file and returns a dict, adding objects
  or records to the project when it imports a design.

Plugins run with the user's permissions: they are NOT sandboxed. The
application loads only plugins the user enabled. Extensions use the public
``resonant`` API; they never reach application internals, and they never
start a physical output.
"""
import importlib.util
import json
import os

PLUGIN_API_VERSION = 1


class PluginApi:
    def __init__(self, plugin_id: str):
        self.plugin_id = plugin_id
        self.extensions = {}

    def _add(self, kind, ext_id, title, function, **extra):
        if not callable(function):
            raise TypeError(f"{self.plugin_id}: the {kind} '{ext_id}' needs a function")
        if ext_id in self.extensions:
            raise ValueError(f"{self.plugin_id}: the extension id '{ext_id}' is used twice")
        self.extensions[ext_id] = {"kind": kind, "id": ext_id, "title": title, "function": function, **extra}

    def add_geometry_generator(self, ext_id, title, function, parameters=None):
        self._add("geometry-generator", ext_id, title, function, parameters=parameters or {})

    def add_analysis(self, ext_id, title, function):
        self._add("analysis", ext_id, title, function)

    def add_importer(self, ext_id, title, extensions, function):
        self._add("measurement-importer", ext_id, title, function, extensions=list(extensions))


def load_plugin(directory: str) -> PluginApi:
    """Imports a Python plugin's entry module from its directory and calls ``register``."""
    with open(os.path.join(directory, "plugin.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    if manifest.get("kind") != "python":
        raise ValueError(f"{directory}: not a Python plugin")
    if int(manifest.get("apiVersion", 0)) != PLUGIN_API_VERSION:
        raise ValueError(f"{manifest.get('id')}: plugin API {manifest.get('apiVersion')} is not supported (this is {PLUGIN_API_VERSION})")
    entry = os.path.join(directory, manifest["entry"])
    spec = importlib.util.spec_from_file_location(f"resonant_plugin_{manifest['id'].replace('-', '_').replace('.', '_')}", entry)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not hasattr(module, "register"):
        raise ValueError(f"{manifest['id']}: the entry module defines no register(api)")
    api = PluginApi(manifest["id"])
    module.register(api)
    return api


def describe(api: PluginApi):
    """The extensions of a loaded plugin without their functions (for the application)."""
    return [{k: v for k, v in e.items() if k != "function"} for e in api.extensions.values()]
