# ADR-000211: Electromagnetic domain — analytical antennas, array-factor arrays, own Yee solver, shared simulation bodies

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/electromagnetics.md` RES-EM-001..024
**Request:** REQ-CHAT-20261126-007
**Builds on:** ADR-000205, ADR-000209, ADR-000210

## Decisions

1. **Analytical antennas first.** Dipoles and monopoles use the exact
   fields of the assumed sinusoidal current and the induced-EMF impedance;
   small loops the magnetic-dipole model. These are classical, closed-form
   and checkable to many digits. A method-of-moments wire solver, which
   would compute the current instead of assuming it, is deferred; patches,
   horns and helices get labelled design estimates, not fields.
2. **Arrays are an array-factor model** (element pattern x array factor)
   with per-element amplitude, phase, delay and on/off kept by index.
   Mutual coupling is not modelled and every array result says so.
3. **An own Yee FDTD solver** as the full-wave method, for the same reasons
   as the acoustic FDTD (ADR-000209): no suitable library is installed or
   vendorable, the method is small, and it is validated against series
   solutions. Split fields with a graded perfectly matched layer
   (exponential update, quadratic grading, design reflection 1e-6 by default),
   perfect conductors by zeroing edge coefficients, periodic boundaries,
   two-dimensional TM when the domain is a plane, thin wires as zeroed
   edges, double precision on one thread. The steady state is extracted by
   a running DFT over the last periods into an ordinary field result.
4. **Materials in the Yee solver** are frequency-independent: a loss
   tangent becomes the equivalent conductivity at the run frequency; good
   conductors (loss ratio above 100) become perfect conductors and the
   plan says so; relative permittivity below 1 is raised to 1 with a
   warning (the solver itself refuses values below 1).
5. **Setup separate from the numerics.** `prepareFullWave` turns a project
   and a domain box into a Yee specification and a plan (grid, memory,
   steps, estimated time, represented and left-out objects); nothing is
   coarsened automatically, a plan over the limits is refused.
6. **Simulation bodies move to `src/core/geometry`** (`SolidModel`): one
   inside test per geometry kind, used by the acoustic and electromagnetic
   full-wave setups alike, with antenna and guide kinds added.
7. **Guides and cavities are objects** (a guide geometry kind and a guide
   component) analysed with closed-form modes; walls are perfect conductors.
8. **One-port Touchstone only** for measured data; multi-port files are
   refused rather than half-read.
9. **The viewport shows one field at a time** (the last one chosen,
   acoustic or electromagnetic) and gains a decibel colour scale, which is
   the default for electromagnetic fields because they span decades.
10. **Electromagnetic settings live in project extensions**
    (`electromagnetics`, `electromagneticFullWave`,
    `electromagneticMeasurements`); results stay in memory with a run
    record in the project, like the other domains.

## Consequences

* Numbers about antennas are trustworthy within the thin-wire assumption
  and are labelled with it; thick or electrically long wires need the
  deferred moment-method solver.
* Array results ignore coupling; the panel says so wherever it shows them.
* Full-wave runs cost memory and time that the plan states up front; 3D
  runs at fine resolution are refused by the default 1 GiB memory limit.

## Alternatives considered

* openEMS / MEEP as the full-wave engine: not installed, GPL licensing
  and a process boundary; revisit when large 3D problems are needed.
* NEC-2 for wires: Fortran, external; the analytical models cover the
  first milestone's validated cases.
