# WORK-000027: Process change: the deployment process (one Linux ZIP) and a concrete package lane in AGENTS.md and scripts/test_manifest.json

**Status:** needs-review
**Type:** process-change
**Request:** requests/processing/2026-11-28-part-ix-packaging-and-distribu-req-chat-20261128-002.md
**Requirements:** RES-REL-005
**Decision:** ADR-000230
**Evidence:** VER-000027
**Test-Suites:** scripts, audits
**Test-Run:** 20261128T115100-2235108

## Scope

Process change: the deployment process (one Linux ZIP) and a concrete package lane in AGENTS.md and scripts/test_manifest.json

## Implementation

- `AGENTS.md` (protected), Deployment section: the one mechanism (the Linux ZIP from a committed tree), the runtime closure discovered under the bundle policy, the `package` lane run once per deployment, the publication gates, and the practices it rules out.
- `scripts/test_manifest.json` (protected), the `package` suite:
  - the placeholders are replaced by `tools/release/package_lane.sh validate` (needs xvfb-run) and `tools/release/package_lane.sh clean-machine` (needs docker);
  - the suite needs unzip and sha256sum.
- `tools/release/package_lane.sh`: finds the ZIP (the newest in `dist/` or `$RESONANT_PACKAGE_ZIP`), verifies the checksum, and runs `validate_bundle.py` under a virtual display or the clean-container tests. A missing tool fails the group.
- `docs/release/DEPLOYMENT.md`: the process, verification, publication gates, reproducibility and compatibility baseline. `docs/release/LINUX_ZIP_DISTRIBUTION.md` points to it.
- RES-REL-005 (`docs/requirements/release_readiness.md`) and ADR-000230 (PROPOSED until reviewed).
- Indexes: decisions.txt, DECISIONS.md, requirements.txt, docs/README.md, IMPLEMENTATION_STATUS.md.

## Tests

- The lane resolves: `python3 scripts/run_tests.py --plan --tier package` lists `package/validate` and `package/clean-machine`.
- `bash -n tools/release/package_lane.sh` passes.
- The framework's own tests (`scripts/unit`), the audits and the structure checks run through the linked t2 run.
- The lane itself runs on a built ZIP in REQ-CHAT-20261128-010; this request does not build one.

## Regression impact and documentation

- No application code changed.
- The `package` lane is explicit, never implied by a tier, so the development tiers are unaffected.

## Escalation 2026-11-28

Reason: process change (`docs/process/process-protection.md`): AGENTS.md and `scripts/test_manifest.json` are protected artifacts and need human review.

Evidence: the diff of this request (AGENTS.md Deployment, the `package` suite), ADR-000230, `docs/release/DEPLOYMENT.md`.

Options:
- (A) Approve as is. ADR-000230 becomes ACCEPTED and the request completes.
- (B) Request changes (for example, different publication gates or lane groups).
- (C) Reject. The placeholders stay, and packaging verification remains outside the framework.

## Traceability

REQ-CHAT-20261128-002 -> RES-REL-005 -> WORK-000027 -> evidence (pending)

## Transitions

- 2026-11-28T16:41:53.233764+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T16:58:28.726399+00:00 active -> needs-review by agent-dev-20261126151713-76f632: process change (docs/process/process-protection.md): AGENTS.md Deployment and the scripts/test_manifest.json package lane are protected artifacts and need human review; options A approve / B request changes / C reject in the work record's Escalation section
