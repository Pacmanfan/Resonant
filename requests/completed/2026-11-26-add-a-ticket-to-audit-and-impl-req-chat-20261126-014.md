# Request: add a ticket to audit and implement volk and cuda where and when possible.

**Request ID:** REQ-CHAT-20261126-014
**Source:** chat
**Type:** FEATURE
**Priority:** HIGH
**Created:** 2026-11-26T22:17:25-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> add a ticket to audit and implement volk and cuda where and when possible.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: HIGH
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: add, implement
  - evidence: 'add' [head]; 'implement' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Follows the question 'are we using cuda and volk acceleration?' (answer: no; all solvers are single-threaded double-precision CPU references; only acoustic analytical field evaluation is multithreaded). Machine: CUDA toolkit at /usr/local/cuda (nvcc), NVIDIA GeForce RTX 5070 Laptop GPU (driver 595.99.02), VOLK 3.1 installed. Scope discussed: audit every compute path (acoustic FDTD, electromagnetic Yee FDTD, analytical field evaluation, array factor and pattern sampling, mechanics finite elements

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Follows the question 'are we using cuda and volk acceleration?' (answer: no; all solvers are single-threaded double-precision CPU references; only acoustic analytical field evaluation is multithreaded). Machine: CUDA toolkit at /usr/local/cuda (nvcc), NVIDIA GeForce RTX 5070 Laptop GPU (driver 595.99.02), VOLK 3.1 installed. Scope discussed: audit every compute path (acoustic FDTD, electromagnetic Yee FDTD, analytical field evaluation, array factor and pattern sampling, mechanics finite elements, signal processing / acquisition pipeline from request 008) for VOLK (SIMD signal-processing kernels) and CUDA (GPU) suitability; implement where it pays off, as optional backends selected at build and run time, with the CPU double-precision reference kept as fallback and accelerated results validated against it with the existing test tolerances (ADR-000205 CPU reference first; electromagnetics requirement: no dependence on one GPU vendor); CPU multithreading of the solvers considered as part of the audit; record precision, speedups and backend in result metadata; keep both CMake and qmake builds working when CUDA or VOLK are absent. Queued in order after the existing tickets.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000009
- Evidence: pending
