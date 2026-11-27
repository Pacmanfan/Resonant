# Plugin API reference (version 1)

The user-facing reference is the help topic `ref.plugin-api`
(`help/src/reference/plugin-api.md`); this page is the developer summary.

## plugin.json

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | unique; no `/` |
| `name` | no (id) | shown in the dialog |
| `version` | yes | major.minor.patch |
| `author`, `description`, `license` | no | shown in the dialog |
| `resonant.min`, `resonant.max` | no (0.0.0 – 999.0.0) | supported Resonant versions, inclusive |
| `apiVersion` | yes | 1 |
| `kind` | yes | `workflows`, `python`, `native` |
| `entry` | yes | a path inside the plugin folder |
| `dependencies` | no | `[{"id", "min"}]` |
| `requires` | no | capabilities (`python`) or workflow action ids |
| `provides` | no | `workflow-template`, `geometry-generator`, `analysis`, `measurement-importer` |
| `permissions` | no | declarations shown to the user (not enforced) |

## Python: `register(api)`

| Method | Function signature |
|---|---|
| `api.add_geometry_generator(id, title, function, parameters=None)` | `function(project, **parameters) -> dict` |
| `api.add_analysis(id, title, function)` | `function(project) -> dict` |
| `api.add_importer(id, title, extensions, function)` | `function(path, project) -> dict` |

Extension ids are unique within a plugin; functions must be callable.

## Native: C ABI (`src/core/plugins/PluginApi.h`)

```c
#define RESONANT_PLUGIN_API_VERSION 1
typedef struct ResonantPluginHost {
    size_t size; int apiVersion; void* context;
    int  (*addWorkflow)(void* context, const char* json, char* error, size_t size);
    void (*log)(void* context, int level, const char* message);  /* 0 info, 1 warning, 2 error */
} ResonantPluginHost;
int resonant_plugin_api_version(void);
int resonant_plugin_register(const ResonantPluginHost* host);
```

## States reported to the user

disabled, ready, loaded, incompatible, missing dependency, invalid, failed —
each with a message (`resonant::plugins::PluginRecord::message`).
