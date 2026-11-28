# Energy Conversion Mechanisms

**Status:** current (REQ-CHAT-20261128-005, RES-CONV-001..006, ADR-000233).
Tests: `tests/core/test_energy_conversion.cpp`, `tests/core/test_piezoelectric_solid.cpp`.

## One architecture

Every mechanism reports a **conversion stage** (`conversion/ConversionStage`),
with these fields:
- input and output, in an energy form (mechanical, acoustic, thermal,
  electrical, electromagnetic, optical, elastic, dissipative);
- other inputs and outputs: a cooler's absorbed heat, a generator's rejected
  heat, a reflected wave;
- stored and dissipated energy;
- coupling coefficients, boundary conditions, assumptions, validity limits and
  uncertainty;
- the contributions it does not compute.

A **chain** of stages becomes an accounting ledger (`accounting/`). Each stage
is a body, its output the next stage's input, and its losses go to the
surroundings. The accounting framework checks each stage's balance, and the
chain checks every join. A stage that declares missing contributions is
*insufficient data*, never *verified*.

## The mechanisms

| Mechanism | Model | Fidelity | Validation |
|---|---|---|---|
| Piezoelectric continuum | `mechanics/PiezoelectricSolid`: 20-node elements with a nodal potential, stress-charge constants rotated with the surface and the poling, electrodes and terminals | spatially resolved 3D, fully coupled, linear | free-plate -d33 V, -d31 V L / t and the free capacitance (exact); open circuit -g33 p t (exact); series bimorph 1.5 d31 V L^2 / t^2 (3 %); unimorph against the lumped reference |
| Piezoelectric cantilever (reference) | `mechanics/PiezoelectricBeam`: lumped first mode, Rayleigh's static shape | lumped | unchanged |
| Thermoelectric couple | `thermal/Thermoelectric`: 1D legs, alpha(T), rho(T), kappa(T); Joule, Peltier, Thomson | 1D, steady | constant-property closed forms (1e-9); eta_max with ZT; cooler Delta T_max = Z T_c^2 / 2; second-order energy residual with temperature-dependent legs; entropy >= 0 |
| Magnetostrictive rod | `mechanics/Magnetostrictive`: linear piezomagnetic constants about a bias; actuator and coil harvester | lumped, LINEAR | shorted stiffening k^H / (1 - k^2) (exact); blocked force = k^H x free elongation; exact harvester balance |
| Photovoltaic cell | `conversion/Photovoltaic`: single-diode circuit, spectral photocurrent, Varshni gap, I_0(T) | equivalent circuit (no transport) | V_oc closed form; Lambert W against Newton (1e-9); Green's fill factor (0.5 %); stage residual (1e-9); dV_oc/dT about -2 mV/K |
| Acoustic to electrical chain | `coupling/ConversionChains`: duct plane wave on a piezoelectric plate | plane wave + 3D coupled plate | every stage verified at and off resonance |

## Piezoelectric continua

- **Constitutive relations:** `T = c^E S - e^T E`, `D = e S + eps^S E`, with
  `E = -grad phi`. Strain-charge data `(s^E, d, eps^T)` convert by
  `c^E = (s^E)^-1`, `e = d c^E`, `eps^S = eps^T - d c^E d^T`. Constants that
  are not positive definite are refused.
- **Axes:** the material axes put 3 along the layer normal (or against it for
  reversed poling) and 1 along the surface's u direction. Constants rotate
  with the Bond matrix: `c' = M c M^T`, `e' = Q e M^T`, `eps' = Q eps Q^T`.
- **Electrical boundary conditions:** electrodes are equipotential faces or
  segments of faces. Each is wired to a terminal:
  - ground;
  - driven (a voltage);
  - floating (zero net charge);
  - a resistive load, where `I = j omega Q = V / R` in harmonic analysis and
    the load is grounded in static analysis.
- **Modes:** short-circuit modes ground the loads; open-circuit modes float
  them. The massless electric unknowns are condensed exactly,
  `K_uu + K_uphi K_phiphi^-1 K_phiu`, so the existing eigensolver applies.
- **Harmonic analysis:** the supports move with a base acceleration (relative
  coordinates), and the structure has a loss factor. The base work, the
  damping and the load powers are computed from separate terms; their
  balance checks the implementation.
- **Numerics:** the electric unknowns are scaled to the mechanical ones, whose
  diagonals differ by many orders of magnitude, and refined. Accuracy is
  judged by the normwise backward error, which stays meaningful at a
  resonance.
- **Against the lumped reference:**
  - the unimorph's short-circuit frequency agrees within 3 %;
  - the coupling `kappa^2 = f_oc^2 / f_sc^2 - 1` agrees within 8 % once the
    reference is corrected. The lumped model uses the static tip-load shape
    and overestimates it against the exact first mode by
    `(1.5 / (phi'(L) L / phi(L)))^2 (0.25 (beta L)^4 / 3)`, about 22 %. That
    is a property of the reference, now documented;
  - the harvested power agrees within 30 %.
- **Not modelled:** dielectric and piezoelectric losses, electrode thickness
  and stiffness, ferroelectric switching and depolarization. The coupled
  engine's piezoelectric participant is still planned.

## Thermoelectric couples

Each leg obeys `d/dx(kappa dT/dx) + rho J^2 - T (d alpha/dT)(dT/dx) J = 0` and
carries the heat flux `q = alpha T J - kappa dT/dx`.

- **Numerics:** finite differences, properties iterated, and the generator's
  current iterated with its load.
- **Open-circuit voltage:** `int (alpha_p - alpha_n) dT`, which does not
  depend on the profile.
- **Energy check:** the junction heats come from second-order end gradients
  and the power from the terminal voltage, so `Q_h - Q_c - P` is an
  independent check. It is exact for constant properties and second order
  otherwise.
- **Entropy:** the generation `Q_c / T_c - Q_h / T_h` must not be negative.
- **Not modelled:** junction thermal resistance, side losses, radiation,
  transient operation. The junction temperatures are imposed.

## Magnetostrictive rods

- **Constitutive relations:** linear piezomagnetic, `S = s^H T + d33 H`,
  `B = d33 T + mu^T H`, about a stated bias. The dynamic field is checked
  against a stated limit.
- **Rod model:** one degree of freedom, Rayleigh mass, a long coil.
- **Coupling:** `Theta = A d33 N / (s^H L)`, `L_c = mu^S N^2 A / L`.
- **Not modelled:** hysteresis, saturation, the Delta-E effect, eddy currents.
  No nonlinear constitutive model is implemented, and none is claimed.

## Photovoltaic cells

The single-diode equivalent circuit, with the photocurrent from the spectrum
(`EQE = (1 - R)(1 - exp(-alpha w)) eta_c` below the gap wavelength). The
incident power splits into:
- reflected and transmitted light (below-gap light included);
- thermalization above the gap; uncollected carriers;
- the voltage deficit `I_ph (E_g / q - V_j)`;
- the diode, shunt and series losses;
- the electrical output.

The circuit's own identity `I_ph V_j = diode + shunt + series + P` is checked,
then the stage balance.

The circuit parameters are inputs. Carrier transport (drift, diffusion,
recombination kinetics) is not solved. The built-in spectrum is a scaled black
body, labelled as a stand-in for a standard spectrum.

## The acoustic-to-electrical chain

1. **Acoustic stage.** A plane wave of amplitude `p_i` in a rigid duct meets a
   plate that closes it. Solved once under a unit pressure, the coupled plate
   gives its volume velocity `U` and surface impedance `Z_s = A / U`. Then
   `p = 2 p_i Z_s / (Z_s + rho c)` and `p_r = p - p_i`. The stage balances the
   incident power `p_i^2 A / 2 rho c` against the reflected power and the
   pressure's work on the plate.
2. **Electromechanical stage.** It balances that work against the damping and
   the terminal power `1/2 Re(V I*)`.
3. **Delivery stage.** It balances the terminal power against the load's
   `1/2 |V|^2 / R`.

Assumptions:
- a 1D plane wave (below the first cross mode);
- nothing behind the plate;
- fluid loading through the plane-wave impedance only.
