# Licensing Decision

**Status:** decided: the MIT License (the owner, 2026-11-28; RES-REL-002,
ADR-000229 addendum, REQ-CHAT-20261128-181). The options below are kept as the
record of what was weighed. The component-by-component audit is
`LICENSE_AUDIT.md`.

## Where things stand

- **The MIT License.** Resonant's own code is under the MIT License: the
  repository's `LICENSE` and the package's `LICENSE` (from
  `tools/release/LICENSE.txt`). Before the decision, the package's `LICENSE`
  granted nothing beyond what the law allows.
- **No component constrains the choice.** No bundled component requires a
  particular licence for Resonant: no GPL-only library is linked or shipped.
  GNU Readline and gdbm are left out of the bundled Python for that reason.
- **Third-party obligations are independent of the choice:**
  - LGPL libraries (Qt and fifteen others) are shipped as unmodified,
    replaceable shared libraries;
  - the GPL with the GCC runtime exception (libgomp);
  - MPL-2.0 (Eigen, unmodified);
  - Apache-2.0, BSD, MIT and similar licences, with notices;
  - Attachment A of the CUDA EULA (the static runtime).

## The choices

| Option | What others may do | Commercial distribution by the owner | Consequences |
|---|---|---|---|
| All rights reserved (proprietary) | use only under a separate agreement | free to sell, license per customer | no outside contributions without agreements; the LGPL obligations still apply to the bundled libraries |
| Permissive (MIT, BSD-3-Clause, Apache-2.0) | use, change, redistribute, also in closed products | allowed; competitors may do the same | Apache-2.0 adds a patent grant and a NOTICE file; MIT and BSD are shortest |
| Weak copyleft (MPL-2.0, LGPL-3.0) | use in any product; changes to Resonant's own files stay open | allowed | MPL works per file; LGPL-3.0 requires that users can replace the library (the application is one program, so it acts much like the GPL here) |
| Strong copyleft (GPL-3.0) | use and redistribute only under the GPL | allowed, but recipients get the source and the same freedoms | compatible with the LGPL, MPL-2.0 and Apache-2.0 components; the statically linked CUDA runtime is proprietary, and whether the GPL's system-library exception covers it needs a legal check |
| Network copyleft (AGPL-3.0) | as GPL, also for use over a network | as GPL | matters only if Resonant is offered as a service |
| Dual licensing (GPL or AGPL, plus commercial) | the open licence, or a paid one | the owner sells exceptions | needs a contributor licence agreement for outside contributions |

Two cautions:
- With a GPL-family licence, the statically linked CUDA runtime is the one
  component that needs a legal check. An alternative is a build without CUDA.
- With a proprietary licence, the Python wheels and the bundled interpreter
  stay under their own licences, which allow it.

## What every release must do (any option)

1. **LGPL corresponding source.** Publish the source packages of the LGPL
   components next to the ZIP, one per name and version in
   `THIRD_PARTY_NOTICES.txt` (`apt-get source <package>=<version>`). The
   alternatives are a written offer valid for three years (LGPL-2.1 section 6)
   or clear directions to a server that keeps the sources available
   (LGPL-3.0). A pointer to Ubuntu's archive alone may not suffice.
2. **Replaceability.** Keep the LGPL libraries as separate, unmodified shared
   libraries with relative search paths, and never link them statically. The
   package tooling already does this.
3. **Notices.** Ship `THIRD_PARTY_NOTICES.txt`, the licence texts in `licenses/`,
   the Cyrus SASL attribution and Attachment A of the CUDA EULA.
   `tools/release/validate_licenses.py` checks them.
4. **Resonant's own notice.** Ship the chosen licence text as `LICENSE`, with a
   copyright line naming the owner and the year. Under Apache-2.0, also ship
   a `NOTICE`.
5. **Source availability of Resonant.** Only the copyleft options require it.
   The source of the exact release tag must then be available to every
   recipient of the binaries.

## Recording the decision

The owner states the option (and, for dual licensing, the commercial terms'
contact). The decision is then recorded:
- here, under *Decision*;
- in an addendum to ADR-000229;
- in `LICENSE`, with the release tooling's template updated.

No release is published before this section is filled in.

## Decision

**The MIT License**, decided by the owner on 2026-11-28 ("License is MIT.
record that", REQ-CHAT-20261128-181).

What it means for a release, against the list above:
1. **LGPL corresponding source:** unchanged. It is required for the bundled
   LGPL libraries, whatever Resonant's licence is.
2. **Replaceability:** unchanged.
3. **Notices:** unchanged. `THIRD_PARTY_NOTICES.txt`, `licenses/`, the
   Cyrus SASL attribution and Attachment A of the CUDA EULA ship as before.
4. **Resonant's own notice:** the MIT text with `Copyright (c) 2026 the
   Resonant authors`.
   - It is the repository's `LICENSE`, and `tools/release/LICENSE.txt` puts
     it into the package as `LICENSE` and `licenses/application/LICENSE`.
   - The owner may name a specific copyright holder in that line at any time.
   - No `NOTICE` file is needed (that is Apache-2.0's).
5. **Source availability of Resonant:** not required by MIT. The source is
   public in the project's repository all the same.

Compatibility:
- MIT is compatible with every bundled component's licence: LGPL-2.1 and
  LGPL-3.0 (shared and replaceable), MPL-2.0, GPL with the GCC runtime
  exception, Apache-2.0, BSD, and the CUDA runtime's Attachment A.
- The legal check that a GPL-family licence would have needed for the static
  CUDA runtime does not arise.

Publication still needs the `package` lane to pass on the exact ZIP, and the
LGPL sources next to it (`DEPLOYMENT.md`).
