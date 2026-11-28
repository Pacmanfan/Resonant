# ADR-000229: Milestone 27 in phases; release governance records (the licensing decision awaits the owner; ADR-000217 recorded as unused)

**Status:** ACCEPTED
**Date:** 2026-11-28
**Type:** process
**Requirements:** `docs/requirements/release_readiness.md` RES-REL-001..004
**Request:** REQ-CHAT-20261128-001 (phases REQ-CHAT-20261128-002..011)
**Builds on:** ADR-000218 (the Linux ZIP), ADR-000215 (Demo Studio), ADR-000228 (architected matter)

## Context

Prompt 27 spans:
- new conversion physics, three-dimensional layered structures, non-ideal
  interfaces, material fields, a multiscale bridge;
- manufacturing-aware optimization, visualization, CAD export, laboratory
  expansion;
- reliability, packaging, licensing and demonstration readiness.

A single request of that size could not pass one gate honestly, and some
parts need decisions or actions that are not the agent's:
- the deployment process (protected);
- Resonant's license;
- installing system packages.

## Decisions

1. **The milestone is processed as eleven requests, in the prompt's order.**

   | Request | Phase | Scope |
   |---|---|---|
   | -001 | baseline | the gap analysis, the licensing decision document, decision bookkeeping, the demonstration audit |
   | -002 | process change | the deployment section and the package lane (human review) |
   | -003 | reliability | the cancellation race; the C++ gRPC transport |
   | -004 | 3D structures | layered shells and non-ideal interfaces |
   | -005 | energy conversion | piezoelectric, thermoelectric, magnetostrictive and photovoltaic models, cross-domain ledgers |
   | -006 | fields and multiscale | tensor fields and the atomistic bridge |
   | -007 | manufacturing | the new physics in optimization |
   | -008 | visualization | 3D rendering, CAD export, the Matter editor |
   | -009 | laboratory | the laboratory extended to the new domains |
   | -010 | packaging | the Linux distribution and clean-machine validation |
   | -011 | demonstrations | the demonstration scenarios, documentation and the final report |

   Each request has its own requirement, work record, test run and gate. The
   plan and the gaps are in `docs/architecture/MILESTONE_27_GAP_ANALYSIS.md`.

2. **Licensing is recorded, not chosen.** `docs/release/LICENSING_DECISION.md`
   presents the options and their consequences, with the obligations that hold
   for every option. The owner's decision is written into it, and into this
   record's addendum, when given. Until then the package's LICENSE grants
   nothing, and the release tooling keeps checking the third-party obligations.

3. **ADR-000217 is recorded as unused.** It was allocated by mistake during
   REQ-CHAT-20261126-014 (WORK-000014 says so) and never held a decision.
   `ADR-000217-unused.md` and the index line state this, and the number is not
   reassigned. Decision numbers are identities: reusing one would make
   existing references ambiguous.

4. **Owner actions are requested, never assumed:**
   - the process change is left in review for its approval;
   - the gRPC packages need the owner to run the system install;
   - the license needs the owner's choice.

   A request that depends on one of these says so in its work record and does
   not report the dependent capability as done.

## Consequences

- Phases can be committed and pushed one by one. Each one's gaps and
  limitations are stated in its own records.
- The final engineering report (request -011) collects the phases, their
  commits and the owner decisions.

## Addendum 2026-11-28: the licence is MIT

The owner decided Resonant's own licence: **the MIT License** ("License is
MIT. record that", REQ-CHAT-20261128-181). It is recorded in:
- the Decision section of `docs/release/LICENSING_DECISION.md`;
- the repository's `LICENSE`;
- the release template `tools/release/LICENSE.txt`, which the package copies
  to `LICENSE` and `licenses/application/LICENSE`.

The copyright line is `Copyright (c) 2026 the Resonant authors`. The
third-party obligations of decision 2 are unchanged: LGPL sources,
replaceable shared libraries, notices. Point 2's "until then" no longer
applies.
