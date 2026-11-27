# Demonstration profile format (resonant.demo-profile, version 1)

One file per demonstration in `demos/profiles/<id>.json`.

| Field | Meaning |
|---|---|
| `format`, `version` | `resonant.demo-profile`, 1 |
| `id`, `name`, `description` | identity |
| `audience`, `domain` | who it is for; acoustic, mechanical, electromagnetic, cross-domain, optimization, experimental, automation |
| `status` | `available`, `partial` (then `remaining` says what is missing), `planned` (cannot be run) |
| `project` | the example project (relative to the resource root) |
| `workspace` | the initial panel |
| `requires` | capabilities (`python`) |
| `data` | datasets used, with their origin (measured or SYNTHETIC) |
| `sequence` | the sequence file |
| `expected` | expected results, in words |
| `camera`, `visualization` | the intended view (the sequence sets it) |
| `screenshots` | screenshots of the help it corresponds to |
| `notes` | the presenter notes file |
| `durationMinutes` | estimated duration |
| `completion` | completion criteria |
| `limitations`, `remaining` | assumptions and limits; what a partial demonstration lacks |
| `story` | the seven answers: `problem`, `configure`, `calculate`, `meaning`, `inspect`, `value`, `assumptions` |

Validation (`DemoProfile::validate`): id and name; a known status; a
sequence unless planned; `remaining` when partial; `problem`, `calculate`
and `assumptions` in the story. `tests/core/test_demo.cpp` loads every bundled
profile and checks that its project, notes, screenshots and every project a
step opens exist.
