# WORK-000032: Record the owner's licensing decision: the MIT License

**Status:** completed
**Type:** documentation
**Request:** requests/processing/2026-11-28-license-is-mit-record-that-req-chat-20261128-181.md
**Requirements:** RES-REL-002
**Decision:** ADR-000229
**Evidence:** VER-000032
**Test-Suites:** scripts, audits
**Test-Run:** 20261128T145046-3242849

## Scope

Record the owner's licensing decision: the MIT License

## Implementation

The owner's decision, in the session: "License is MIT. record that". It answers `docs/release/LICENSING_DECISION.md` (RES-REL-002, ADR-000229).

- `LICENSE` (new, repository root): the MIT License text, `Copyright (c) 2026 the Resonant authors`.
- `tools/release/LICENSE.txt`: the package's `LICENSE` and `licenses/application/LICENSE` (copied by `tools/release/package.py`) now carry the MIT text. They state that the third-party components keep their own licences. Before, the file granted nothing.
- `docs/release/LICENSING_DECISION.md`: the status, the Decision section (the five release obligations under MIT, and compatibility with the bundled components), and where things stand.
- `docs/decisions/ADR-000229-...`: an addendum recording the decision; `decisions.txt` and `DECISIONS.md` updated to match.
- `docs/requirements/release_readiness.md`: RES-REL-002 states the decision. Its `requirements.txt` row is unchanged: it sits next to a row that REQ-CHAT-20261128-006 has not yet committed, and it already names the decision record.
- `docs/release/LICENSE_AUDIT.md` ("Resonant itself"), `docs/release/DEPLOYMENT.md` (the publication list), `IMPLEMENTATION_STATUS.md` and the Milestone 27 gap analysis state the decision.

## Tests

- t2 on this request: the scripts and audit suites, the documentation integrity and drift checks. The run is linked below as **Test-Run:**.
- The package lane's licence check (`tools/release/validate_licenses.py`) checks that `LICENSE` and `licenses/application/LICENSE` exist in a built ZIP. It runs with the packaging request (REQ-CHAT-20261128-010).

## Regression impact and documentation

- **No code changes:** the release tooling copies the same file name, and the third-party obligations are unchanged.
- **Root `README.md`:** unchanged. It is at the 80-line limit of its test, and the repository's `LICENSE` is where hosting services look.
- **Copyright line:** names "the Resonant authors". The owner may name a specific holder at any time.

## Traceability

REQ-CHAT-20261128-181 -> RES-REL-002 -> WORK-000032 -> evidence (pending)

## Transitions

- 2026-11-28T19:50:34.490875+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T19:57:43.386844+00:00 active -> completed by agent-dev-20261126151713-76f632
