# Plugin development

## Start from an example

| Example (`plugins/examples/`) | Kind | Shows |
|---|---|---|
| `spiral-array-generator` | python | a geometry generator: a Fermat-spiral acoustic array |
| `array-report` | python | an analysis: element spacing in wavelengths, grating-lobe risk, directivity and sidelobes |
| `csv-field-importer` | python | a measurement importer comparing measured magnitudes with the model (bundled data is synthetic and says so) |
| `workflow-pack` | workflows | two workflow templates |

Copy one into `~/.local/share/Resonant/plugins/<your-id>/` (or a folder on
`RESONANT_PLUGIN_PATH`), change its `id`, enable it in **Tools > Plugins**,
press **Reload Plugins**.

## A Python plugin

```python
# plugin.json: {"id": "me.hello", "version": "1.0.0", "apiVersion": 1, "kind": "python",
#               "entry": "hello.py", "requires": ["python"], "provides": ["analysis"],
#               "resonant": {"min": "0.1.0", "max": "0.99.99"}}
import resonant

def count_objects(project):
    return {"objects": len(project.objects())}

def register(api):
    api.add_analysis("count", "Count objects", count_objects)
```

- A generator receives a **copy** of the project; what it adds is applied by
  the application as one undoable step.
- An analysis must not change the project; it returns a dict.
- An importer returns what it read; label imported data with its origin.
- Use only the public `resonant` API. Raise exceptions with clear messages:
  they are shown to the user as the plugin's failure reason.

## A workflow pack

`"kind": "workflows"`, `"entry": "workflows"` and one `resonant.workflow` JSON
file per template (`docs/automation/WORKFLOW_SYSTEM.md`). Every file is
validated; one bad file fails the plugin with the file's name.

## A native plugin

Include `src/core/plugins/PluginApi.h`, export
`resonant_plugin_api_version()` (return 1) and
`resonant_plugin_register(const ResonantPluginHost*)` (return 0), and call
`host->addWorkflow(host->context, json, error, size)`. Build a shared library
for the same platform. A native plugin runs inside Resonant: test it well.

## Testing

`tests/core/test_plugins.cpp` and `python/tests/test_plugins_python.py` show how
the examples are tested; a plugin's own logic is best tested with the Python
API directly (`resonant.plugins.load_plugin(directory)`).
