# Demonstrations

Demo Studio's demonstrations (REQ-CHAT-20261126-010, ADR-000215): each one
tells an engineering story with Resonant's real models, step by step, with
the application's own commands. Nothing here is a result: every number a
demonstration shows is calculated when it runs.

| Folder | Holds |
|---|---|
| `profiles/` | one profile per demonstration (`resonant.demo-profile`): story, audience, project, status, expected results, limits |
| `sequences/` | the steps (`resonant.demo-sequence`): application-level actions with parameters, expected state, time limit, failure behaviour, notes |
| `presenter_notes/` | what to say at each step |
| `assets/` | data a demonstration uses (none yet beyond the example projects; see its README) |
| `screenshots/`, `reports/` | where to keep curated captures and write-ups (runs write theirs to the user's data folder) |
| `manifest.json` | every demonstration's project, dependencies, data and expected outputs |

Run one from **Tools > Demo Studio**, or unattended:

    resonant --run-demo demo-b-acoustic-focusing [--presenter] [--window-size 1920x1080] [--demo-output <folder>]

Formats: `docs/demos/DEMO_PROFILE_FORMAT.md`, `docs/demos/DEMO_SEQUENCE_FORMAT.md`.
Catalog: `docs/demos/DEMO_CATALOG.md`.
