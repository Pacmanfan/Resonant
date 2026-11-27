# Transactions and Workflows

(RES-API-TX-001..003, RES-API-SVC-005..006.)

## Single changes

Every change is one undoable step in the application's history (the GUI's
Edit > Undo undoes a client's change). A change that changes nothing adds no
step. With `header.dry_run` the change is made on a copy and reported
(`changes`, warnings) without touching the project. With
`header.idempotency_key` a retried request returns the first outcome
(`header.replayed`) instead of applying twice.

## Concurrency

Several clients and the GUI may change the project at the same time; calls
are applied one after another on the application's thread. To avoid
overwriting someone else's change, send the revision you read:
`expected_revision` (one object) or `header.expected_project_revision` (the
project). A mismatch is refused with `CONFLICT` and the current revision.

## Batches

`ExecuteBatch{operations, description}` applies up to 10 000 operations
(create primitives, update objects or parameters, delete, assign materials,
create or update emitters and arrays, set excitations, focus, create field
regions) **all together or not at all**: they run in order on a copy of the
project with the live revisions; if one fails, nothing is applied, the
failing operation reports its error and the later ones "not run". If all
succeed, the copy replaces the project as **one** undo step named
`description`. A dry run validates and reports without applying.

## Transactions

For changes assembled over several calls:

```text
BeginTransaction{description}            -> transaction (base_project_revision)
StageOperations{transaction_id, operations}   (nothing changes yet)
ValidateTransaction{transaction_id}      -> per-operation results on a copy
CommitTransaction{transaction_id}        -> one undo step, or CONFLICT if the project moved on since Begin
AbortTransaction{transaction_id}
```

A conflict leaves the transaction open: abort it and begin again on the new
revision. Transactions belong to their session.

**Scope.** Batches and transactions cover changes to the project (scene,
materials, settings). Files written, jobs started and anything outside the
project are not part of them and are not rolled back; no universal rollback
is claimed.

## Workflows

`ListWorkflowTemplates` lists the `resonant.workflow` templates (built in and
from plugins) with their JSON definitions. `ExecuteWorkflow{workflow_id or
definition, parameters, apply_changes}` runs one as a job on a copy of the
project; files it writes go to a run folder on the application's side. With
`apply_changes` (needs modify) a completed run's changes become one undo step
and the run is recorded in the project's automation history.
`GetWorkflowRun{job_id}` returns the run record (steps, outputs, errors) once
finished. The workflow format is described in
[the workflow system](../automation/WORKFLOW_SYSTEM.md).
