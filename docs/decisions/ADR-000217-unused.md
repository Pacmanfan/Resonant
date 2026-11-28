# ADR-000217: unused number

**Status:** UNUSED (never a decision)
**Date recorded:** 2026-11-28
**Recorded by:** REQ-CHAT-20261128-001 (RES-REL-003, ADR-000229)

The number ADR-000217 was allocated by mistake while REQ-CHAT-20261126-014 (the
automation API) was processed. WORK-000014 records it: "ADR-000217 was
allocated by mistake and is unused (ids are never reused)". That request's
decision is ADR-000216.

The number holds no decision. It is not reassigned: the allocator never
reissues a number, and reusing one would make any reference to it ambiguous. A
later decision that touches the same subject takes a new number.
