# Plugin architecture

**Requirements:** RES-PLUG-001..008. **Decision:** ADR-000215.
**User help:** `py.plugins`, `ref.plugin-api`, `trouble.plugins`.

## Model

A plugin is a directory with `plugin.json` (`docs/plugins/PLUGIN_API_REFERENCE.md`).
`resonant::plugins::PluginManager` (src/core/plugins) discovers, validates and
loads them; the application shows them in the Plugins dialog and their
extensions in Tools > Workflows and Tools > Extensions.

| Kind | Extension points | Loaded by | Runs in |
|---|---|---|---|
| `workflows` | workflow templates | the manager (JSON) | the application, as data |
| `python` | geometry generators, analyses, measurement importers | the Python worker (`resonant.plugins.load_plugin`) | the worker process |
| `native` | workflow templates (C ABI host table) | the manager (`dlopen`) | the application process |

## Lifecycle

```text
discover  -> search paths: $RESONANT_PLUGIN_PATH, ~/.local/share/Resonant/plugins, bundled examples
validate  -> manifest fields, version format, kind, entry inside the folder and present,
             plugin API version, supported Resonant range, required capabilities,
             duplicate ids (first wins), dependencies (present, enabled, new enough, not failed)
state     -> disabled | ready | incompatible | missing dependency | invalid
load      -> only ready (= enabled and valid) plugins, in dependency order:
             workflows -> WorkflowLibrary; native -> resonant_plugin_register(host);
             python -> handed to the worker when it starts (markLoaded / markFailed)
state     -> loaded | failed (with the reason)
```

Enabling is explicit and persisted in `~/.config/Resonant/plugins.json`; a
change takes effect on **Reload Plugins** (or the next start). Nothing is
loaded that the user did not enable. A failing plugin is reported with its
reason and leaves the project, the application and the other plugins
untouched (tested with deliberately broken plugins in `tests/data/plugins/`).

## Security

Plugins are code with the user's permissions. Python plugins run in the
worker process (a crash ends only the worker); native plugins run in the
application (a crash is a crash of Resonant) and are never unloaded. No
plugin is sandboxed, and neither the UI nor the documentation claims so.
`permissions` in the manifest are declarations shown to the user, not
enforcement. No plugin interface can start a physical output.

## Stability

The plugin API version is 1. The native host table is versioned by its
`size`; functions are only appended. A new API version is introduced only for
an incompatible change; plugins state the version they implement and the
Resonant versions they support.
