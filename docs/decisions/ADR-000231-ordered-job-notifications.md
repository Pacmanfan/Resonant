# ADR-000231: Job notifications are delivered in order, and wait() returns only after they are

**Status:** ACCEPTED
**Date:** 2026-11-28
**Requirements:** `docs/requirements/application_foundation.md` RES-APP-017
**Request:** REQ-CHAT-20261128-003 (Prompt 27, Part VIII)
**Amends:** the job manager of RES-APP-017

## Context

`Jobs/RunningJobIsCancelledPromptly` failed now and then under load. The
history it recorded lacked the final `cancelled` state. Two defects in
`JobManager` caused this.

1. **wait() did not wait for the listeners.**
   - A job's state changed under the manager's lock.
   - The listeners were called after the lock was released.
   - `wait()` returned as soon as the state was final. A caller descheduled
     between `cancel()` and `wait()` could return before any listener had
     heard of the final state: the recorder, the session's event bus, the
     GUI.
2. **Notifications could arrive out of order.**
   - `submit()` published `queued` after releasing the lock.
   - A worker that was not waiting (just started, or just finished a job)
     could take the new job, so listeners heard `running` before `queued`.

Lengthening the timeout would not fix either defect.

## Decision

- **One ordered outbox.** Every change is recorded in an outbox under the same
  lock as the change itself. Changes therefore leave in the order they
  happen: each job's `queued` comes before its `running`, which comes before
  its final state.
- **One thread delivers at a time.** A thread that records a change delivers
  the outbox unless another thread is delivering. In that case the active
  thread delivers the new change after the earlier ones, and the recording
  thread does not block.
  - No lock is held while a listener runs.
  - A listener may submit or cancel jobs. Re-entrant changes are delivered
    after the current one.
  - No new lock-order dependency exists between the manager and the
    listeners' own locks.
- **wait() waits for the final notification.** `wait()` returns once the job
  is finished and its final notification has reached every listener.
  - `info()` still shows a state the moment it changes.
  - A listener must not `wait()` for a job: that job's final notification is
    delivered after the listener returns.
- **A failing listener stops nothing.** An exception from a listener is
  logged. The other listeners and the waiters still run.

## Consequences

- **Regression coverage.** Four tests in `tests/core/test_jobs.cpp` hold a
  listener on purpose, so they fail every time against the old code rather
  than now and then:
  - `WaitReturnsOnlyAfterTheListenersWereToldTheJobFinished`
  - `ListenersHearEachJobsStatesInTheOrderTheyHappened`
  - `CancellationIsAnnouncedBeforeWaitReturnsEveryTime`
  - `AListenerMaySubmitAJobAndAFailingListenerStopsNothing`

  The evidence records both outcomes: against HEAD's `Job.cpp` the two
  deterministic tests failed 3 of 3; with the fix all passed, 40 of 40 under
  parallel load.
- **Notification timing.**
  - A call that causes a change usually returns after its notification was
    delivered.
  - When another thread is delivering at that moment, the notification
    arrives slightly later, but still in order.
  - Nothing in the application relied on the old timing. The GUI already
    forwards notifications to its own thread.

## Part VIII.2: the C++ gRPC transport

`libgrpc++-dev` and `protobuf-compiler-grpc` are not installed (dpkg,
2026-11-28; Ubuntu candidate 1.51.1). Installing them needs administrator
rights, which is the owner's call. This request does not skip the
integration silently:
- the prerequisite is documented in `docs/architecture/AUTOMATION_ARCHITECTURE.md`;
- the request is parked in needs-review, asking the owner to install them.

Until then, gRPC is served by the grpcio bridge (RES-API-SEC-002, ADR-000216).
