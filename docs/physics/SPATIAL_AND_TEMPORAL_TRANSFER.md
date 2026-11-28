# Spatial and Temporal Transfer

**Status:** grid-to-grid transfer implemented (RES-CEME-008, ADR-000220):
`src/core/multiphysics/Transfer`. Mesh-to-mesh mapping, coordinate frames other
than the world and temporal interpolation are planned.

## Supports

Field data live on a rectilinear cell grid: per axis an ascending list of
edges (world meters), one value per cell (per component). One cell across an
axis makes it 1D or 2D. Participants with other discretizations convert at
their boundary: the finite-element column reports element-centre
displacements and stresses on its element grid; the layered wave model
reports per-sublayer absorbed power.

## Two operators

| Operator | Formula | Conserves | For |
|---|---|---|---|
| conservative | v_t = Σ_s v_s · |t ∩ s| / |t| (overlap volume of cells) | the integral over the overlap of the grids, to rounding | densities: power, heat, force, mass, charge |
| interpolating | tensor-product linear interpolation between cell centres, clamped at the outermost centres (no extrapolation) | nothing | intensive quantities: temperature, displacement, stress |

A conservative transfer of a quantity whose descriptor is not a conserved
density is refused, as is a transfer between different frames. Every
transfer reports its source and target integrals, their relative difference
and the covered fraction of the source; the engine records these with the
transfer (`Multiphysics.ConservativeTransferPreservesTheIntegral`,
`.InterpolatingTransferIsExactForLinearFieldsAndClampsAtTheEnds`).

## Reference and current configurations

The wave solution of a layered system is computed in the current (deformed)
geometry, conduction and mechanics in the reference one. The absorbed power
is mapped to the reference configuration per material element (power
absorbed in the deformed sublayer, divided by its reference thickness), which
is exact: the transfer error of the reference cases is below 1e-12.

## Time

Every field carries its time character. The steady schemes exchange steady
or time-averaged quantities; a harmonic quantity becoming a steady one is a
conversion the coupling model declares (the period average). A transient
thermal participant driven by a steady wave receives a constant source; its
ledger counts the wave's power over the interval. Synchronization of
participants with different time steps (sub-cycling, temporal interpolation
of exchanged fields) is planned with multirate coupling.

## Planned

Unstructured mesh-to-mesh mapping (common refinement or supermesh for
conservative transfer, consistent interpolation for intensive fields),
boundary-to-volume and surface transfers (tractions, heat fluxes), local
frames, temporal interpolation and region aggregation.
