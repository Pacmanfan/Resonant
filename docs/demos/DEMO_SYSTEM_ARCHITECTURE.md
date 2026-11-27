# Demonstration system architecture

**Requirements:** RES-DEMO-001..010. **Decision:** ADR-000215.

```text
demos/                     profiles, sequences, presenter notes, assets, manifest (data)
src/core/demo/Demo         formats: profile, sequence, run manifest; validation; library; SHA-256
src/app/DemoController     the engine: runs steps with the application's commands, waits for events
src/app/DemoStudioPanel    Demo Studio (forms/DemoStudioPanel.ui)
src/app/PresenterOverlay   the presenter bar (forms/PresenterOverlay.ui)
src/main/main.cpp          --run-demo, --capture-screenshots, --run-workflow, --presenter, --window-size
```

The demonstration infrastructure is separate from the physics: `src/core/demo`
includes no physics module (layering rule), and the controller calls the same
panel and controller functions a user's clicks call.

## Engine

`DemoController` runs one step at a time from the event loop. Immediate steps
(open, workspace, select, camera, visualization, show, help, capture,
project-action, experiment-data, promote, expect, presenter) finish at once.
Waiting steps:

| Step | Waits for |
|---|---|
| `run` | the jobs the step started (seen in the job manager before and after the call) to finish, and the controller's completion signal (`resultsChanged`, `runFinished`), connected before the call; a failed or cancelled job fails the step at once |
| `workflow` | its `WorkflowJob`; the result is applied as one undoable change |
| `python` | the worker's `done` for the request |
| `pause` | the presenter's **Next** (unattended runs pass it and record that) |

Each step has a time limit (`timeoutSeconds`): it bounds failure only; on
expiry the step fails and what it started is cancelled. A step's `expect`
checks are evaluated after it. A failure stops the run (or continues, when the
step says so); the steps not reached are recorded as skipped. **Stop**
cancels the running step's jobs (or interrupts Python) and records the run as
cancelled. **Restart** runs the same sequence again.

## Determinism

Fixed, versioned example projects (their SHA-256 is recorded), stable object
ids, explicit camera and visualization steps, fixed solver settings in the
projects, seeds recorded in optimization problems, event-based waiting, a
default layout (automated runs do not restore the user's layout). Two runs
from a clean start give identical calculated values (tested;
`tests/app/test_demo_studio.cpp`).

## Data safety

A demonstration opens its own project (the user is asked about unsaved
changes first). Steps that store data next to the project use
`copyToRunDirectory`, so nothing is written among the bundled examples. No
step can authorize a physical output.
