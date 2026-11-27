# Workflow system

**Requirements:** RES-AUTO-002..006. **User help:** `py.workflows`.

## Format: resonant.workflow, version 1

| Field | Meaning |
|---|---|
| `id`, `name`, `description`, `workflowVersion`, `category` | identity |
| `requires` | capabilities (`python`) and action ids the workflow needs (checked before running) |
| `parameters` | `id`, `label`, `type` (number, integer, string, bool, list, json), `default`, `unit`, `description` |
| `steps` | `id`, `action`, `params`, `when` (condition), `repeat` (`parameter`, or `values`), `dependsOn`, `continueOnError`, `description` |
| `outputs` | named outputs collected from steps |
| `expectedSeconds`, `docs` | the expected duration; help topics |

### Values and conditions

- `"${name}"` substitutes a parameter, typed when it is the whole string;
  inside text it is formatted. `"${steps.<id>.<key>.<index>}"` refers to an
  earlier step's output (a repeated step's outputs form a list).
- `when` uses the optimization expression language with comparisons
  (`< > <= >= == !=`), `&&`, `||`, `!` over the parameters (flattened) and step
  outputs.
- `repeat` runs a step once per list value (the value is `${<parameter>}` in
  that iteration); `set` on analysis actions applies design changes for that
  iteration (design variants).

### Execution

A step runs if its condition holds and every step it depends on ran. A
failure stops the run unless `continueOnError`. Cancellation is checked
between steps and inside long actions. Each step records status, iteration,
value, message, outputs and time; the run records parameters, timestamps,
status and outputs (`WorkflowRun`).

## Templates

Built in: `acoustic.linear-array-focus`, `acoustic.reflector-optimization`,
`mechanical.modal-and-forced`, `em.dipole-pattern`, `em.array-steering`. The
example workflow pack plugin adds `examples.acoustic-spacing-sweep` and
`examples.em-frequency-check`. Every template is run by the tests with real
actions (`tests/core/test_automation.cpp`, `tests/core/test_plugins.cpp`).

## History

`workflowHistory(project)` / `appendHistory` keep the last 50 runs in the
project; the Workflows dialog lists them and refreshes on undo / redo.
