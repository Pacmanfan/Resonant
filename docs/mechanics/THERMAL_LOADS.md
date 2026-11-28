# Thermal Loads

**Status:** current (REQ-CHAT-20261126-016, RES-CEME-012):
`src/core/mechanics/ThermalLoads`.

A temperature change ΔT = T − T_ref in an element gives the free thermal
strain ε_th = α ΔT on the normal components. Its equivalent nodal load is
f = ∫ Bᵀ D ε_th dV for solid hexahedra (2×2×2 Gauss points) and ∓E A α ΔT
along the axis at the two nodes of a frame element. Stresses are recovered
as σ = D (B u − ε_th) at the element centre (solids) and σ = E (ε_axial − αΔT)
(frames). The coefficient α comes from the material's thermal property set.

## Validation (`tests/core/test_thermal_loads.cpp`)

A free block expands by αΔT L without stress; a laterally confined column has
σ_xx = σ_yy = −EαΔT/(1−ν), σ_zz = 0 and ε_zz = αΔT(1+ν)/(1−ν); a fully
confined element −EαΔT/(1−2ν) hydrostatic; a free bar extends by αΔT L and a
bar between supports carries −EαΔT. Plates and membranes are refused.

## Limits

Linear thermoelasticity, small strain, properties independent of
temperature within one solve, uniform temperature over a frame section (no
thermal bending), no thermoelastic damping.
