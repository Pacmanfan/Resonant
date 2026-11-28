# WORK-000028: Reliability: ordered job notifications and wait() after delivery (the flaky cancellation test's race); the C++ gRPC transport's system packages requested from the owner

**Status:** needs-review
**Type:** feature
**Request:** requests/processing/2026-11-28-part-viii-reliability-and-test-req-chat-20261128-003.md
**Requirements:** RES-APP-017
**Decision:** ADR-000231
**Evidence:** VER-000028
**Test-Suites:** core, gui, cmake, audits
**Test-Run:** 20261128T120814-2358487

## Scope

Reliability: ordered job notifications and wait() after delivery (the flaky cancellation test's race); the C++ gRPC transport's system packages requested from the owner

## Implementation

### Part VIII.1: the flaky cancellation test

**Root cause** (`src/core/simulation/Job.cpp`): two defects.
1. `wait()` returned as soon as a job's state was final. The worker told the
   listeners only after releasing the lock, so a caller descheduled between
   `cancel()` and `wait()` read the recorder before it heard `cancelled`.
2. `submit()` published `queued` after releasing the lock, so a worker that
   was not waiting could announce `running` first.

**Fix:**
- every change is recorded in one outbox under the lock that makes the change;
- one thread at a time delivers the outbox in order, and a concurrent or
  re-entrant caller hands its notification to that thread;
- no lock is held while listeners run;
- `wait()` waits for the final notification (`finishDelivered`);
- a failing listener is logged and stops nothing.

`Job.h` documents the contract.

### Part VIII.2: the C++ gRPC transport

- **Checked:** `dpkg -s libgrpc++-dev protobuf-compiler-grpc` reports neither
  installed; the Ubuntu candidate is 1.51.1.
- **Documented:** the prerequisite and the install command are in
  `docs/architecture/AUTOMATION_ARCHITECTURE.md`, and `IMPLEMENTATION_STATUS.md`
  lists the C++ transport as not implemented.
- **Blocked:** the transport cannot be built or tested without the libraries.
  The request is parked in needs-review for the owner's installation (see
  the Escalation section). It is not skipped silently.

## Tests

**Deterministic regression coverage** (`tests/core/test_jobs.cpp`):
- `Jobs.WaitReturnsOnlyAfterTheListenersWereToldTheJobFinished`: a
  deliberately slow listener.
- `Jobs.ListenersHearEachJobsStatesInTheOrderTheyHappened`: `queued` is held
  while the worker takes the job without being woken.
- `Jobs.CancellationIsAnnouncedBeforeWaitReturnsEveryTime`: the original
  scenario, 50 rounds.
- `Jobs.AListenerMaySubmitAJobAndAFailingListenerStopsNothing`: re-entrancy
  and a throwing listener.

**Proof against both versions** (a standalone binary of these tests linked
against the core library):
- **Old code** (HEAD's `Job.cpp`): the ordering and wait tests failed 3 of 3
  runs. The 50-round test failed in 1 of 3 runs, reproducing the original race.
- **New code:** all passed 3 of 3, and 40 of 40 with 8 runs in parallel.

The suites (core, gui, cmake/ctest, audits) run through the linked t2 run.

## Regression impact and documentation

- **Callers:** the job manager is used by every job (simulations, optimizer,
  workflows, the services' job tracking and the GUI). Notifications are now
  delivered in order. A notification may arrive slightly after the call that
  caused it, when another thread is delivering at that moment. No caller
  depended on the old timing; the GUI forwards to its own thread.
- **Documentation:** RES-APP-017 amended; ADR-000231;
  `AUTOMATION_ARCHITECTURE.md` (the gRPC prerequisite); `IMPLEMENTATION_STATUS.md`;
  the Milestone 27 gap analysis rows.

## Escalation 2026-11-28

**Reason:** Part VIII.2 requires `libgrpc++-dev` and `protobuf-compiler-grpc`.
They are not installed, and installing them needs administrator rights,
which the agent does not use.

**Evidence:** the `dpkg -s` output (not installed); `apt-cache policy`
(candidate 1.51.1-4.1build5).

**Options:**
- (A) The owner runs `sudo apt install libgrpc++-dev protobuf-compiler-grpc`.
  The request then resumes: build the C++ transport and test it against the
  Python client and the MCP tools.
- (B) The owner declines. The grpcio bridge remains the gRPC transport, and
  Part VIII.2 is closed as not planned.

## Traceability

REQ-CHAT-20261128-003 -> RES-APP-017 -> WORK-000028 -> evidence (pending)

## Transitions

- 2026-11-28T17:01:42.029801+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T17:14:09.909199+00:00 active -> needs-review by agent-dev-20261126151713-76f632: Part VIII.2 needs the owner to install libgrpc++-dev and protobuf-compiler-grpc (administrator rights); Part VIII.1 (the flaky cancellation test) is fixed and verified; options A install / B decline in the Escalation section
