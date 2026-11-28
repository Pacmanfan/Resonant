# Heat Conduction

**Status:** current (REQ-CHAT-20261126-016, RES-CEME-010):
`src/core/thermal/HeatConduction`, `ThermalMaterial`. The thermal domain is
used by the Coupled Energy–Matter Engine; it has no panel of its own yet.

## Equation and discretization

ρ c_p ∂T/∂t = ∇·(k ∇T) + q, on a rectilinear cell grid (cell widths may
differ along each axis), one temperature per cell (finite volumes). The
conductance between neighbouring cells is the series combination of their
half-cell resistances, A / (w₁/2k₁ + w₂/2k₂), so interfaces between
materials are exact for a 1D flux. Outer faces: adiabatic, fixed temperature
(conductance A / (w/2k)), or convective with h and an ambient temperature
(A / (w/2k + 1/h)).

- **Steady:** the symmetric positive definite system is factorized (sparse
  LDLᵀ); at least one face must fix a temperature or convect.
- **Transient:** implicit Euler (unconditionally stable, first order in
  time); the discrete energy balance source = stored + heat out holds to
  rounding.

Reports: cell temperatures, the heat leaving through each face (W, or J over
a transient run), the source power, the stored energy change and the
balance residual.

## Validation (`tests/core/test_heat_conduction.cpp`)

Uniform source between fixed temperatures (parabolic profile, 0.2 % of the
rise with 200 cells; exact balance); convection on both sides (the series
resistance flux, exact); a 3D box with two fixed faces (linear, exact);
insulated transient heating (exact); a sudden surface temperature against
the Fourier series (within 0.5 K of a 100 K change); invalid problems refused.

## Materials

The thermal property set: `density` (kg/m³), `thermalConductivity`
(W/(m K)), `specificHeat` (J/(kg K)) and the linear `thermalExpansion` (1/K,
used by the mechanics). The example library has no thermal values yet; the
multiphysics example defines its own material.

## Limits

Linear within a solve (temperature-dependent properties come from material
laws between coupled passes); no radiation, no internal convection, no phase
change, no contact resistance.
