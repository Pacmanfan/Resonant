# Demonstration sequence format (resonant.demo-sequence, version 1)

```python
{"format": "resonant.demo-sequence", "version": 1, "id": "demo-b-acoustic-focusing", "sequenceVersion": "1",
 "name": "B. Acoustic focusing",
 "steps": [{"id": "focused", "action": "run", "description": "Field of the focused array",
            "params": {"what": "acoustics.field", "region": "ex-b-region"},
            "expect": {"acousticResults": 1}, "timeoutSeconds": 120, "onFailure": "stop",
            "notes": "what the presenter says", "screenshot": "acoustic-focal-field.png"}]}
```

Every step: a stable `id`, a `description` (shown to the presenter), an
`action`, `params`, optional `expect`, `timeoutSeconds` (default 60; a bound
for failure, never a reason for success), `onFailure` (`stop` or `continue`),
optional `notes` and `screenshot`.

## Actions

| Action | Parameters | Does |
|---|---|---|
| `open-project` | `path`, `copyToRunDirectory` | opens a project (a copy in the run folder when asked) and records its SHA-256 and solver settings |
| `workspace` | `panel`, `tab`, `docks` (only these shown), `windowSize`, `dockHeights`, `dockWidths` | arranges the window |
| `select` | `object` (id or name) | selects it |
| `camera` | `preset` (front, top, right, iso, reset, frame-all, frame-selection), `yaw`, `pitch` (degrees), `distance`, `zoom`, `target`, `grid`, `axes` | sets the view |
| `visualization` | `domain` (acoustics, mechanics, em), `quantity`, `animate` | sets what is drawn |
| `run` | `what`: `acoustics.field` (region), `acoustics.focus` (array, target), `full-wave` (region), `mechanics.modal / harmonic / transient / static` (structure), `em.analyse / em.pattern` (object, show3d), `em.array` (object), `optimization` (problem), `experiment` (experiment; never authorized) | runs through the panel and waits |
| `show` | `panel`, `tab`, `domain`, `result`, `item`, `pattern3d` | shows a panel, tab or result (a mode shape) |
| `project-action` | `id` (a workflow action), `params` | one undoable change of the project |
| `workflow` | `id`, `parameters` | runs a workflow template, applies its result |
| `experiment-data` | `op` (`import-touchstone` with `file`, `compare`), `experiment` | imports labelled data, compares with the simulation |
| `promote` | `candidate` (`best` or an id) | promotes an optimization candidate |
| `python` | `code` (`${runDirectory}` is replaced), `label`, `sendProject` | runs code in the console's worker |
| `help` | `topic` | opens a help topic |
| `capture` | `name`, `target`, `size`, `open`, `annotations`, `directory`, `page` | writes a screenshot (and an annotated copy) |
| `pause` | `text` | narration: waits for Next |
| `presenter` | `on` | presenter mode on or off |
| `expect` | the checks | checks the state |

## Checks (`expect`)

`object`, `minModes`, `minCandidates`, `acousticResults`, `consoleContains`,
`patternContains`, `undoAvailable`, `revisions`.

Validation refuses unknown actions, duplicate step ids, missing required
parameters, non-positive time limits and screenshot names that are not plain
`.png` file names.
