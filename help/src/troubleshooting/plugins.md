---
id: trouble.plugins
title: Plugin failures
category: Troubleshooting
order: 110
status: available
summary: Why a plugin is disabled, incompatible, invalid, missing a dependency or failed, and how to fix it.
keywords: [plugin, failed, incompatible, invalid, dependency, extension]
related: [py.plugins, ref.plugin-api, trouble.python]
---

**Tools > Plugins...** shows each plugin's state and the reason. A plugin that
fails is left out; Resonant, the other plugins and your project are unaffected.

| State and message | Cause | Fix |
|---|---|---|
| disabled: `not enabled (plugins load only when you enable them)` | nothing is wrong | tick **Enabled**, then **Reload Plugins** |
| invalid: `no plugin.json in ...` / `... is not valid JSON` | the manifest is missing or broken | fix `plugin.json` ([reference](../reference/plugin-api.md)) |
| invalid: `the version '...' is not major.minor.patch` | a version like `1.0` | write `1.0.0` |
| invalid: `the entry '...' does not exist` | the file named by `entry` is missing | install the whole plugin folder |
| invalid: `another plugin with the id '...' was found first` | two plugins share an id | remove one; earlier search paths win |
| incompatible: `plugin API N is not supported (this is 1)` | the plugin was written for another plugin API | update the plugin |
| incompatible: `supports Resonant A to B, this is C` | made for other Resonant versions | update the plugin or Resonant |
| incompatible: `requires 'python', which is not available` | a Python plugin, but no Python or no package | see [installation](../python/installation.md) |
| missing dependency: `needs the plugin '...', which is not installed` (or `not enabled`, or `... or newer`) | a plugin it builds on | install, enable or update that plugin |
| failed: `the library could not be loaded: ...` | a native library that is not a valid library for this system | rebuild it for this system |
| failed: `the library does not export resonant_plugin_api_version ...` | not a Resonant native plugin | check the plugin |
| failed: `RuntimeError: ...` (a Python plugin) | its `register(api)` raised | the message is the plugin's own; report it to its author |
| failed: `<file>.json: ...` (a workflow pack) | a workflow file is invalid | the message names the file and the problem |

## Extensions are missing from Tools > Extensions

Python plugins load into the Python worker. When they are enabled Resonant starts
the worker; if Python is not available they stay **ready** and no extension
appears. Open the console to see why ([Python errors](python.md)).
