# Automation architecture

**Requirements:** RES-AUTO-001..008. **Decision:** ADR-000215.

```text
src/core/automation/     Workflow model and engine (domain independent: layering rule)
    Workflow.h/.cpp      resonant.workflow v1: parameters, steps, outputs, runs
    WorkflowEngine.h/.cpp  ActionRegistry, WorkflowLibrary, parameter resolution,
                         ${...} substitution, conditions, repetition, dependencies,
                         cancellation, WorkflowJob (on a snapshot), history
src/core/coupling/WorkflowActions.cpp    every domain action (registered here)
src/core/coupling/WorkflowTemplates.cpp  built-in templates
src/core/commands/ProjectChange          a changed copy -> one undoable composite
```

## One automation surface, many front ends

| Front end | Uses |
|---|---|
| Python | `resonant.automation.run / action`, and the API directly |
| Workflows dialog | `WorkflowJob` on a snapshot; changes applied as one undoable step |
| Command line | `resonant <project> --run-workflow <id>` (through Demo Studio's engine) |
| Demo Studio | `project-action` and `workflow` steps |
| Plugins | workflow packs (JSON) and native plugins add templates |

Actions are the unit: every action validates its parameters, works on the
project it is given and returns JSON outputs. They cover scene construction
and changes, settings, acoustic fields / pressure / focusing, mechanical modal
and harmonic analyses, antenna analysis / patterns / steering / tapers,
optimization runs, file output, HTML reports and checks.

## Background and safety

Workflows run as jobs on the application's job manager, on a copy of the
project, with progress and cancellation between steps; the result is applied
only when the run completes and only as one undoable change. The run (any
outcome) is recorded in the project (`automation` extension, last 50 runs).
No action drives hardware: experiments with outputs run only from the
Experiments panel, with authorization.
