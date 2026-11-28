# Release Readiness and Milestone Governance

**Owner of:** `RES-REL-001..`
**Source:** REQ-CHAT-20261128-001 (Prompt 27, "Scientific Completeness, Multiscale Physics, and Release Readiness") and its phase requests REQ-CHAT-20261128-002..011
**Refines:** `distribution.md` RES-DIST-* (the Linux ZIP), `automation.md` RES-DEMO-* (Demo Studio)
**Decisions:** ADR-000229 (builds on ADR-000218, ADR-000215)
**Documents:** `docs/architecture/MILESTONE_27_GAP_ANALYSIS.md`, `docs/release/LICENSING_DECISION.md`, `docs/demos/DEMONSTRATION_READINESS.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-REL-001 — Baseline audit and phase plan

Before a milestone that spans the application changes anything, a gap analysis
is recorded. It covers:
- scientific capabilities and multiphysics integration;
- material representation, interfaces and boundary conditions;
- atomistic and multiscale integration;
- GUI and visualization;
- testing and reliability, packaging and deployment;
- demonstration automation and video production.

Each gap names its evidence (the implementation status, a decision, a test). The
milestone is processed as phase requests, each with its own requirement, work
record, test run and gate. The plan maps every part of the milestone to a request.
*Verification:* `docs/architecture/MILESTONE_27_GAP_ANALYSIS.md`; the phase requests in `requests/`.

### RES-REL-002 — Licensing decision record

Resonant's own license is the owner's decision. Until it is recorded, the
package grants nothing beyond what the law allows. The licensing decision
document lists:
- the choices and their consequences for commercial distribution;
- compatibility with the bundled components;
- the LGPL source obligations and the notices and source availability a
  release requires.

The owner's decision is recorded in the document and in its ADR before a public
release. The release tooling keeps the LGPL obligations whatever the choice is.

**Decided 2026-11-28: the MIT License** (REQ-CHAT-20261128-181, ADR-000229
addendum). It is the repository's `LICENSE`, and the package's `LICENSE`
comes from `tools/release/LICENSE.txt`.
*Verification:* `docs/release/LICENSING_DECISION.md`; `tools/release/validate_licenses.py` (the package lane).

### RES-REL-003 — Decision-number integrity

Every allocated decision number is either a decision record or recorded as
unused, with why. A number is never reused or reassigned.
*Verification:* `docs/decisions/decisions.txt` (ADR-000217); `scripts/drift_detect.py` (every record exists).

### RES-REL-004 — Demonstration readiness

The demonstration system is audited against what flagship demonstrations need:
- reproducible setup and deterministic configuration;
- controlled excitation and observable quantities;
- synchronized visualization and comparison with measurements;
- energy accounting and exported numbers;
- screenshots, interaction traces and video recording.

Each planned scenario names the physics it depends on and the requests that
provide it. A demonstration distinguishes application output from
post-production graphics, and never presents synthetic data as measured.
*Verification:* `docs/demos/DEMONSTRATION_READINESS.md`.

### RES-REL-005 — The deployment process

Resonant is deployed by one mechanism: the self-contained Linux ZIP built by
`tools/release/build_linux_zip.sh` from a committed tree. Its runtime closure
(Qt libraries and plugins, C++ and Python dependencies) is discovered from the
built files under the bundle policy, and it carries:
- the offline help, the examples and the demonstrations;
- the reference data and the licences.

The artifact is verified once per deployment by the `package` lane
(`validate`, `clean-machine`), which never builds and never re-runs the
development suites. A missing tool fails the lane instead of weakening it.
Publication also requires the owner's licensing decision and the LGPL
corresponding sources. The process is recorded in AGENTS.md (Deployment), a
protected artifact changed only through a reviewed process change.
*Verification:* `python3 scripts/run_tests.py --plan --tier package`; `docs/release/DEPLOYMENT.md`; the package lane's run on a built ZIP (REQ-CHAT-20261128-010).

