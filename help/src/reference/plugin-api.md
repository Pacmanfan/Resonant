---
id: ref.plugin-api
title: Plugin API reference
category: Technical Reference
order: 90
status: available
summary: The plugin manifest, the Python plugin API, the native C interface and the versioning rules (plugin API 1).
keywords: [plugin, api, manifest, native, python, extension, version]
related: [py.plugins, ref.api, py.workflows, trouble.plugins]
---

## The manifest: plugin.json

```python
{
  "id": "examples.array-report",          # unique; letters, digits, '.', '-'
  "name": "Array report",
  "version": "1.0.0",                     # major.minor.patch
  "author": "...", "description": "...", "license": "...",
  "resonant": {"min": "0.1.0", "max": "0.99.99"},   # supported Resonant versions (inclusive)
  "apiVersion": 1,                        # the plugin API it implements
  "kind": "python",                       # workflows | python | native
  "entry": "report.py",                   # a file (or folder) inside the plugin folder
  "dependencies": [{"id": "other.plugin", "min": "1.2.0"}],
  "requires": ["python"],                 # capabilities or workflow action ids it needs
  "provides": ["analysis"],               # workflow-template, geometry-generator, analysis, measurement-importer
  "permissions": ["reads the open project"]  # what it does, shown to the user (declared, not enforced)
}
```

Resonant refuses a plugin whose manifest cannot be read, whose version is not
major.minor.patch, whose kind is unknown, whose entry points outside its folder or
does not exist, or whose API version or Resonant range does not match.

## Kind "workflows"

`entry` is a folder of [workflow](../python/workflows.md) JSON files. Each is checked like a
built-in template; one invalid file fails the plugin.

## Kind "python"

`entry` is a module that defines `register(api)`. It runs in the Python worker.

```python
import resonant

def generate(project, count=32, radius=0.03):
    """Adds objects to `project` (a copy; the application applies it, undoable)."""
    return {"elements": count}

def analyse(project):
    """Reads `project`; returns a dict. Must not change the project."""
    return {"objects": len(project.objects())}

def import_file(path, project):
    """Reads a file; returns a dict (labelled as imported data)."""
    return {"source": path}

def register(api):
    api.add_geometry_generator("my-generator", "My generator", generate,
                               parameters={"count": {"default": 32, "unit": ""}})
    api.add_analysis("my-analysis", "My analysis", analyse)
    api.add_importer("my-importer", "My file format", [".csv"], import_file)
```

An exception in `register` fails the plugin with its message. Extensions use only
the public [Python API](api.md).

## Kind "native"

`entry` is a shared library with two C functions (header `plugins/PluginApi.h`):

```python
int resonant_plugin_api_version(void);                       /* returns 1 */
int resonant_plugin_register(const ResonantPluginHost* host);  /* 0 = success */
```

The host table holds `size`, `apiVersion`, `context` and the functions
`addWorkflow(context, json, error, size)` and `log(context, level, message)`. Only
C types cross the boundary; later versions append functions, so a plugin checks
`size` before using a newer one. A native plugin runs inside the application: a
crash in it is a crash of Resonant. It is never unloaded while Resonant runs.

## Versions

The plugin API is version 1. A new version is introduced only when the manifest or
an interface changes incompatibly; a plugin states the version it implements and
the Resonant versions it supports. Additions that old plugins can ignore keep the
version.
