# Request: # PART IX — PACKAGING AND DISTRIBUTION

This work requires a PROCESS-CHANGE requ

**Request ID:** REQ-CHAT-20261128-002
**Source:** chat
**Type:** PROCESS-CHANGE
**Priority:** NORMAL
**Created:** 2026-11-28T11:25:49-05:00
**Status:** needs-review
**Attachments:** - none

## Original User Statement

> # PART IX — PACKAGING AND DISTRIBUTION

This work requires a PROCESS-CHANGE request because the existing package test lane and Deployment section of AGENTS.md need modification.

## 9.1 Process change

Create the required PROCESS-CHANGE request.

Review and update the deployment process to accommodate:

- Clean-machine testing.
- Reproducible builds.
- Runtime dependency discovery.
- Qt libraries and plugins.
- C++ dependencies.
- Resources.
- Offline documentation.
- Example projects.
- Demonstration assets.

Preserve the project's existing engineering governance.

## Classification

- Type: PROCESS-CHANGE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: PROCESS-CHANGE (override)
  - final_type: PROCESS-CHANGE
  - heuristic_type: PROCESS-CHANGE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as process-change: AGENTS.md, PROCESS-CHANGE
  - evidence: 'PROCESS-CHANGE' [head]; 'AGENTS.md' [head]; 'PROCESS-CHANGE' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: investigate process-change per classification; derive requirement and verify.
Conversational context: Phase 1b of REQ-CHAT-20261128-001 (Prompt 27): the process change for packaging - the package test lane and the Deployment section of AGENTS.md. Needs human review before completion.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Phase 1b of REQ-CHAT-20261128-001 (Prompt 27): the process change for packaging - the package test lane and the Deployment section of AGENTS.md. Needs human review before completion.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000027
- Evidence: pending
