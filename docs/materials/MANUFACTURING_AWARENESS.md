# Manufacturing Awareness

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-015, ADR-000228), `matter/Manufacturing`.

A theoretical arrangement is not a manufacturable design.

## Processes

Each region names the process that makes it. The process kinds are:

| Category | Kinds |
|---|---|
| additive | metal, polymer and ceramic additive manufacturing; multi-material printing; directed-energy deposition; powder-bed fusion |
| deposition | sputter, atomic layer, chemical vapour and physical vapour deposition; thin-film (solution) processing; thermal spraying |
| patterning | lithography; patterned electrode fabrication |
| joining | lamination; diffusion bonding |
| thermal | sintering; heat treatment |
| subtractive | machining and finishing |
| composite | composite fabrication |

Each process in an architecture states its constraints, with their source:
- minimum and maximum layer thickness;
- minimum feature size and maximum dimension;
- process temperature;
- compatible material classes;
- maximum residual stress, roughness and repeatability.

Resonant states none of these values itself.

## The check

The check reports violations:
- **Layer thickness:** a layer thinner or thicker than its process allows.
- **Feature size:** a pattern feature or gap, or a sub-volume region, below
  the minimum feature size.
- **Material class:** a class the process does not take.
- **Size:** a stack larger than the process makes.
- **Process temperature:** a temperature above the limit of the region made,
  *or of any layer made before it*.
- **Residual stress:**
  - the stress built in when the layers made so far cool from the process
    temperature, against the process's limit and against the interface's
    cohesive strength (a delamination risk);
  - it is computed by lamination theory (`coupling::processResidualStresses`),
    with the layers stress-free at the process temperature.

It also reports:
- **not assessed:** a region without a process, a missing material class or
  temperature limit, an idealized field;
- **notes:** a tolerance tighter than the repeatability; a layer thinner than
  ten times the roughness below it (a screening rule).

The status is `infeasible` (a violation), `not-assessed`, or `feasible within the
stated constraints`. Feasible is not a validation, and an idealized architecture
is never reported feasible.

The energy-converter example shows the check at work:
- **Bonded plate:** cured at room temperature; no violation.
- **Hot cure:** curing the same plate at 393 K builds about 70 MPa into the
  PZT, which violates the stated 60 MPa limit.
- **Deposited film:** a sol-gel PZT film crystallized at 923 K on steel holds
  several hundred MPa on cooling, beyond its interface strength.

## Records

A manufacturing record keeps what was made:
- batch, composition, process and parameters;
- deposition conditions and heat treatment;
- the layer sequence, interface preparation and post-processing;
- inspections, measured datasets (laboratory) and measured properties.

`MatterService.AddManufacturingRecord` adds one in one undo step. An alternative
duplicated from an architecture does not inherit its records.
