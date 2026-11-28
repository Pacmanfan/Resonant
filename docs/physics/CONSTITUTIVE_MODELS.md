# Constitutive Models with State

**Status:** current (REQ-CHAT-20261126-021, RES-DMS-004..008, ADR-000225).
Each model updates its state the same way: a trial state is computed from the
committed one, and is committed only when the step converges. A failed step
is reported with its residual, and the committed state is kept (RES-DMS-004).
All models are small-strain and isothermal unless stated.

## J2 plasticity (`src/core/mechanics/Plasticity`)

The model uses von Mises yield with linear isotropic hardening (`H`) and
linear kinematic hardening (Prager, `Hk`):

```text
f = |dev σ − α| − √(2/3) (σy + H ēp)                     yield function
Δεp = Δγ n,  Δα = (2/3) Hk Δγ n,  Δēp = √(2/3) Δγ          flow and hardening, n = (dev σ_trial − α)/|·|
Δγ = f_trial / (2G + (2/3)(H + Hk))                       radial return (closed form for linear hardening)
C_ep = C − β n⊗n − θ (P_dev − n⊗n),  β = 4G²/(2G + (2/3)(H+Hk)),  θ = 4G²Δγ/|ξ_trial|   consistent tangent
```

- **Notation:** Mandel inside, Voigt with engineering shear at the
  interface (`T_V = S C S`, `S = diag(1,1,1,1/√2,1/√2,1/√2)`).
- **Energy:**
  - elastic: `½ σ:εe`;
  - stored hardening: `½ H ēp² + ¾ |α|²/Hk`;
  - dissipated: `σy Δēp` per step, which is exact for linear hardening.
  The work equals stored plus dissipated.
- **Uniaxial stress:** the axial strain is given and the five other
  stresses are driven to zero by Newton on the 5×5 block of the tangent. The
  condensed tangent is `dσ11/dε11`.
- **Indeterminate bar (`j2Bar`):**
  - The bar is fixed at both ends with a load `P` at `x = a`. The segment
    `[0, a]` is in tension and `[a, L]` in compression, one material point
    each.
  - Equilibrium `A σ1(u/a) − A σ2(−u/b) = P` is solved by Newton on the
    load-point displacement.
  - The steps follow the load path. On unloading the residual stresses stay
    in balance.

**Excluded:** finite strain, rate dependence, nonlinear hardening, softening
and thermal coupling.

## Scalar damage (`src/core/mechanics/Damage`, `DamagedBeam`)

```text
σ = (1 − D(κ)) E ε,   κ = max over the history of |ε|          (history, irreversible)
exponential:  D = 1 − (κ0/κ) exp(−(κ − κ0)/κf)                  softening
power:        D = 1 − (κ0/κ)^(1−m)  (stress ∝ κ^m, m < 1)        hardening, non-localizing
dissipation:  ∫ ½ E κ² dD, trapezoidal in κ over each increment (second order, like the work);
              secant unloading (no residual strain)
```

**The damaged cantilever (Euler–Bernoulli, Hermite elements):**

- The tip is displacement-controlled. Each element's damage follows the
  largest curvature strain at its outer fibre.
- Within a step, a fixed-point iteration converges the damage with the
  deflected shape.
- Dissipation is trapezoidal, `½(U0_prev + U0_new) ΔD`, which converges at
  second order.
- The modes are computed from the damaged stiffness and the consistent mass
  (a dense generalized symmetric eigenproblem). No damage-to-frequency law is
  assumed (RES-DMS-006).
- Committable: the uniform modulus with the damaged tip stiffness. This is
  an equivalent value, not a local one.

The exponential law localizes in a structure, so its result depends on the
mesh, and the run says so. The power law is the beam's default.

## Standard linear solid (`src/core/mechanics/Viscoelasticity`)

```text
σ = E∞ ε + E1 (ε − εv),   τ dεv/dt = ε − εv                     (Zener)
update over Δt with ε linear in the step: exact exponential integral; dissipation ∫ E1 (ε − εv) dεv exact
E'(ω) = E∞ + E1 ω²τ²/(1+ω²τ²),   E''(ω) = E1 ωτ/(1+ω²τ²),   loss per cycle π E'' ε0²
```

## Phase change (`src/core/thermal/PhaseChange`)

- **Method:** explicit enthalpy method on a 1D slab. The cells hold
  enthalpy, from which temperature and liquid fraction follow. Each phase
  has its own specific heat and conductivity (harmonic means at the faces).
- **Faces:** adiabatic, a fixed temperature or a fixed flux.
- **Stability limit:** `Δt ≤ ρ c_min Δx² / (4 k_max)`. A larger step is
  refused, not reduced.
- **Energy per snapshot:** heat in, sensible energy and latent energy.
  Their residual is at rounding level.
- **Reference:** the one-phase Stefan (Neumann) solution
  `s(t) = 2λ √(α t)` with `λ e^{λ²} erf λ = St/√π`, computed by `stefanLambda`.

## Preisach hysteresis (`src/core/electromagnetics/Hysteresis`)

```text
M(H) = Σ over relays (α ≥ β) of w(α, β) γ_αβ(H)  +  χ_rev H         relays ±1, switch up at α, down at β
density: uniform on the triangle, or gaussian in coercivity (α − β)/2 and interaction (α + β)/2
diagonal relays weighted ½ (the trapezoidal rule)
```

- **What the relays give:** the relay states are the memory. Return-point
  memory (wiping out) and congruent minor loops follow from them; neither
  is imposed.
- **Loss:** the loss per cycle is the loop area `μ0 ∮ H dM`, measured on the
  second cycle at each amplitude. A minor loop entered from a larger
  excursion is not closed on its first traversal; return-point memory closes
  the second. The reversible part adds none. The area agrees with the sum
  over the switched relays of `2 μ0 Ms w (α − β)` within `2 μ0 Ms ΔH`
  (each switch falls inside one field increment).
- **Core summary:** from the turns, mean radius, cross-section and
  frequency, the study gives the power at that frequency.

**Excluded:** eddy currents, rate dependence and vector (rotational)
hysteresis.
