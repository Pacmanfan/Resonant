# Functionally Architected Matter

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-001..020, ADR-000228).

The organization of matter is an engineering variable. An *architecture* records
how the matter of one or more scene objects is organized. The existing solvers
predict what that organization does, each only within what it declares it can
represent.

| Document | Covers |
|---|---|
| this one | the specification, the data model, the workflow, what is not done |
| [ARCHITECTED_MATTER_AUDIT.md](ARCHITECTED_MATTER_AUDIT.md) | what existed and what is reused |
| [INTERFACE_ENGINEERING.md](INTERFACE_ENGINEERING.md) | interfaces, their models and derived quantities |
| [ARCHITECTED_MATTER_INTEGRATION.md](ARCHITECTED_MATTER_INTEGRATION.md) | multiscale, the coupled engine, inverse design, the laboratory |
| [MANUFACTURING_AWARENESS.md](MANUFACTURING_AWARENESS.md) | processes, constraints, residual stress, records |
| [SOLVER_CAPABILITY_MATRIX.md](SOLVER_CAPABILITY_MATRIX.md) | what each analysis represents and refuses |
| [ARCHITECTED_MATTER_VALIDATION.md](ARCHITECTED_MATTER_VALIDATION.md) | benchmarks, tests, performance |
| [ARCHITECTED_MATTER_ROADMAP.md](ARCHITECTED_MATTER_ROADMAP.md) | the phases that remain |

## The data model

```text
Architecture (extension "matter", format resonant.matter; revision = SHA-256 of the content)
 |- objects ............ scene objects it describes
 |- regions[] .......... FunctionalRegion: object, extent (object | layer | subvolume), role,
 |    |                  materialId (library) | composition (constituents, fractions, method)
 |    |                  phases[], microstructure (crystal, grains, porosity, texture -> multiscale
 |    |                  representation), orientation (Bunge ZXZ), anisotropic sets (elastic,
 |    |                  thermal, expansion, piezoelectric + poling), stated properties,
 |    |                  operating conditions, limits, process, functions
 |- stacks[] ........... Stack: object, axis, length, width
 |    '- layers[] ...... Layer: region, thickness (+uncertainty), in-plane angle, pattern
 |                       (uniform | segmented | periodic | inclusions)
 |- interfaces[] ....... Interface: two regions, models[] (perfect, perfect-bonding,
 |                       finite-stiffness, cohesive-zone, frictional-contact,
 |                       thermal- and electrical-contact-resistance, dielectric,
 |                       charge-transfer, diffusive, reactive) with sourced parameters,
 |                       adhesion, residual stress, preparation
 |- fields[] ........... PropertyField: region, "domain.key" or "fraction:<constituent>",
 |                       constant | linear | power-law | exponential | table, realizable or idealized
 |- functions[] ........ Function: kind, regions, targets, stages[] (from, to, mechanism)
 |- adaptive ........... static | passive-adaptive | externally-controlled | reconfigurable,
 |                       control input, states, reversibility (declared)
 |- processes[] ........ Process: kind, parameters, constraints (stated, with their source)
 '- records[] .......... ManufacturingRecord: batch, process, sequence, inspections,
                         measured datasets (laboratory), measured properties
```

Relationships to the rest of Resonant:

```text
scene object <-- region.object, stack.object         library material <-- region.materialId, constituent
multiscale representation <-- microstructure          material-state study <-- phase, adaptive state
laboratory study --> link "matter-architecture"       physical system <-- stackToPhysicalSystem
optimization problem --> adapter "matter"             accounting ledger <-- pathway stages
```

Every value that is not a library value carries its provenance:
- measured, manufacturer or literature;
- first-principles, simulation or estimated;
- assumed, or one of the library's own words.

The validation rejects:
- dangling references and fractions that do not sum to one;
- non-positive thicknesses and interfaces between regions that do not touch;
- a piezoelectric set without its poling;
- an externally controlled class without its input.

It warns about consecutive layers without an interface: nothing is assumed about one.

## Property classes and resolution

Every analysis reads a region's property through one resolution:
1. a field;
2. a value stated on the region;
3. the composition's effective value by its stated method;
4. the library material.

Anything else is missing, and never substituted. Effective values come only from
a named method, with the rigorous bounds (Hashin–Shtrikman, Wiener,
Voigt–Reuss) and the method's validity (`matter/Mixing`).

## Workflow

**Design from function (Direction A).**
- **Mechanisms** lists, for an input and an output domain, the direct
  mechanisms and the two-stage cascades. Each is marked as implemented by an
  analysis, implemented by the coupled engine (with its own status), or not
  implemented. Each also lists the regions and library materials that have
  what it needs.

**Explore an existing architecture (Direction B).**
- **What can it do?** lists the analyses each stack admits and those that
  refuse it, with the reasons. It lists the pathways and what is missing.
- It computes risks with their numbers:
  - thermal mismatch against the interface strength;
  - a transduction layer near the neutral surface;
  - dielectric loss and leakage;
  - a missing electrode;
  - the manufacturing violations;
  - the frequency against its target.
- Findings (from records and computations) are kept apart from hypotheses.

In the **Matter** panel an engineer:
- creates or opens an architecture;
- inspects its regions and stacks (drawn as an idealized model, thicknesses to
  scale, interfaces styled by their model);
- runs an analysis and reads its assumptions and warnings;
- checks manufacturability and explores;
- duplicates it as an alternative, compares the alternatives side by side,
  and sends a stack to the coupled engine;
- saves and reopens without loss (`MatterArchitecture.EveryExampleRoundTripsWithoutLossAndValidates`).

## Known limitations

These are also listed in ADR-000228 and `IMPLEMENTATION_STATUS.md`.

**Structures:**
- Layered bodies are analyzed in three dimensions by the `layered-solid`
  analyses (REQ-CHAT-20261128-004, `../mechanics/LAYERED_SOLIDS.md`). The
  surface may be flat, cylindrical, spherical or a surface of revolution (a
  bell's profile), with explicit interfaces. They are linear, without damage
  evolution or contact in debonded regions. A cohesive zone, a segmented layer
  and an in-plane orthotropic set are refused there.
- Piezoelectric conversion is a lumped single-mode cantilever model. The
  coupled engine's piezoelectric coupling stays planned.

**Physics not modeled:**
- thermoelectric, magnetostrictive, photovoltaic, triboelectric, induction and
  chemical conversion;
- finite-stiffness, cohesive (beyond a bonded criterion), frictional,
  diffusive, reactive and charge-transfer interfaces (recorded and refused).

**Fields and anisotropy:**
- Fields vary along one axis.
- Anisotropy is used by lamination theory and through-thickness conduction
  only.

**Not delivered:**
- microstructure generation beyond the multiscale RVEs;
- CAD export of architectures;
- a 3D viewport rendering of fields;
- machine-learned discovery.
