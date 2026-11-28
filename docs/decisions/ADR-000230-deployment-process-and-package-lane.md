# ADR-000230: The deployment process: one Linux ZIP, verified once by a concrete package lane (process change)

**Status:** ACCEPTED (process change approved by the owner, 2026-11-28)
**Date:** 2026-11-28
**Type:** process-change
**Requirements:** `docs/requirements/release_readiness.md` RES-REL-005
**Request:** REQ-CHAT-20261128-002 (Prompt 27, Part IX.1)
**Builds on:** ADR-000218 (the Linux ZIP), ADR-000186 (packaging verifies the artifact once), ADR-000185 (protected artifacts)

## Context

The Linux ZIP and its tools exist (REQ-CHAT-20261126-013, ADR-000218):
- the build: `build_linux_zip.sh`, `package.py`;
- the validators: `validate_bundle.py`, `validate_licenses.py`, `validate_help.py`;
- the clean-container tests: `clean_machine_test.sh`;
- the dependency inspection: `inspect_dependencies.sh`.

The development system did not describe them, in two places:
- AGENTS.md's Deployment section still held the template's placeholder.
- The `package` lane of `scripts/test_manifest.json` named placeholder
  scripts and a placeholder required file, so it could never run.

Both are protected artifacts (`docs/process/process-protection.md`), so the
change is a process change with human review.

## Decision

1. **The ZIP is the one mechanism.** `tools/release/build_linux_zip.sh`, from a
   committed tree, writes `dist/RESONANT-Linux-x86_64-<version>.zip` and its
   checksum. Runtime dependencies are discovered from the ELF files under
   `tools/release/bundle_policy.json`. AGENTS.md says so and points to
   `docs/release/DEPLOYMENT.md`.
2. **The package lane is concrete.**
   - `validate`: `tools/release/package_lane.sh validate` verifies the
     checksum and runs `validate_bundle.py` under a virtual display (needs
     xvfb-run).
   - `clean-machine`: `tools/release/package_lane.sh clean-machine` runs the
     clean-container tests (needs docker).

   The lane verifies the newest `dist/` ZIP or `$RESONANT_PACKAGE_ZIP`. It
   never builds and never re-runs the development suites (ADR-000186), and a
   missing tool fails a group rather than downgrading it.
3. **Publication gates.** A ZIP is published only after:
   - the lane passed on that ZIP;
   - the owner's licensing decision is recorded;
   - the LGPL corresponding sources are published with it.

## Consequences

- `python3 scripts/run_tests.py --tier package` runs on any machine with the
  tools. The packaging request (REQ-CHAT-20261128-010) records its run as
  evidence.
- The template placeholders are gone from this project's manifest. The
  framework's own template (`scripts/templates/test_manifest.json`) is
  unchanged.

## Review

Approved by the owner on 2026-11-28: "you have my perission to touch the
agents.md go ahead and address that". That is option (A) of WORK-000027's
escalation: the AGENTS.md Deployment section and the package lane as
proposed.
