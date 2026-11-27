---
id: mechanics.forced-response
title: Forced response
category: Mechanical Engineering
order: 60
status: available
summary: Harmonic frequency sweeps with explicit damping, frequency-response functions, measured responses and the sound a plate radiates.
keywords: [harmonic, frequency response, FRF, damping, loss factor, Rayleigh, accelerance]
related: [mechanics.modal, mechanics.actuators, mechanics.waves, exp.correlation, acoustics.field-analysis]
---

A forced-response (harmonic) analysis calculates the steady vibration of a
structure driven by its active actuators at each frequency of a sweep. Use
it to find resonance peaks, response levels and the effect of damping.

## The problem solved

With the time convention e^{+jωt} and peak amplitudes:

```math
(K (1 + jη) − ω² M + jω C) U = F(ω)
V = jω U,      A = −ω² U
```

K, M and C are the stiffness, mass and damping matrices, η a structural
loss factor, U the complex displacement, F the sum of the active actuators'
forces ([Actuators](actuators.md)), V the velocity and A the
acceleration.

## Settings (Frequency Response tab)

| Field | Meaning |
|---|---|
| From, To | sweep limits in Hz |
| Frequencies, Logarithmic spacing | number of frequencies and their spacing |
| Method | *Direct (exact for the model)* or *Modal superposition (truncated)* |
| Modes for superposition | modes used by the modal method |
| Damping | *None*, *Rayleigh: C = alpha M + beta K*, *Structural loss factor*, *Modal damping ratio* |
| alpha (1/s), beta (s) | Rayleigh coefficients |
| Loss factor eta | the structural loss factor (default 0.01) |
| Use each material's own loss factor where it states one | per-material η |
| Damping ratio zeta | modal ratio, modal superposition only |

Press **Calculate Response** (or **Mechanics > Calculate Frequency
Response**). Settings are stored in the project.

## Methods and damping

- **Direct** factorizes the dynamic stiffness at every frequency: exact for
  the discretized model with every damping model except a modal ratio.
- **Modal superposition** sums the lowest modes:

```math
U = Σ_r φ_r (φ_rᵀ F) / (ω_r² − ω² + 2j ζ_r ω_r ω)
```

Truncation leaves out the higher modes' contribution; with every mode the
modal result equals the direct one.

Rayleigh damping gives each mode the ratio α/(2ω) + βω/2. Without damping
the response at a natural frequency is unbounded. Damping is never added
silently: a modal ratio with the direct method is refused with the reason,
and the result always states the damping model.

## Reading the result

The plot shows the response at the point chosen in **Response at** (a
sensor or a vibration target) per newton of drive: *Displacement*,
*Velocity* or *Acceleration*, as *Magnitude (dB re 1 unit/N)* or *Phase
(deg)*. Dotted lines mark the natural frequencies of the latest modal
result of the same structure. In the **Display** group, pick a frequency in
**Mode / frequency / instant** to see the deformed shape (the frequency of
the largest response is shown first), coloured by displacement, velocity
or acceleration amplitude.

Full complex fields are kept for every frequency when they fit half the
memory limit (at most 512 MiB); otherwise only the sensors and targets, and
the result says so.

## Worked example

Open `examples/mechanics/c-forced-resonance.resonant`: the aluminium
cantilever with a 1 N force at the tip and two accelerometers, swept from
5 to 500 Hz at 800 logarithmic frequencies with loss factor 0.02. Run
**Calculate Modes**, then **Calculate Response**, and set the quantity to
*Acceleration* for the accelerance. The response at the tip peaks at the
bending frequencies 16.3, 102 and 287 Hz, within the sweep spacing (0.6 %). Each peak has a half-power bandwidth of η f (0.33 Hz at the
first mode) and the displacement lags the force by 90° at resonance.

## Measured responses

**Import Measured Response...** reads a CSV file and draws it on the plot
of the same quantity:

```text
# sensor: accelerometer A1
# quantity: displacement
# unit: m/N
frequency_Hz, magnitude, phase_deg
10, 0.001, 0
20, 0.004, -90
```

Columns are `magnitude, phase_deg` or `real, imag`; `#` lines of the form `key: value` may give
`sensor`, `quantity`, `unit`, `position`, `direction`, `sensitivity` and
`source`. Frequencies must increase and values be finite. The note below
the plot gives the RMS level difference in dB and the largest peak of both.
Imported data is stored in the project (undoable). Data is not acquired
from hardware here; see [Simulation correlation](../experiments/correlation.md).

## Sound radiated by a plate

The **Periodic Rod and Sound** tab turns a plate's or membrane's harmonic
result into an acoustic field: choose an acoustic region in **Onto
acoustic region**, select the harmonic result and frequency under
**Display**, and press **Calculate Sound at the Shown Frequency**.

```math
p(x) = (jωρ / 2π) · Σ_e v_e A_e e^{−jkR_e} / R_e,      v_e = jω w_e
```

This is the Rayleigh integral of a flat surface in an infinite rigid
baffle: each element of area A_e radiates from its centre with its normal
velocity v_e; ρ and k are the fluid's density and wavenumber and R_e the
distance to the field point. The field appears among the acoustic results.
It is one-way: the fluid does not load the plate (no radiation damping or
added mass), which is a fair assumption in air but not in water.

## Limitations

- Linear, small amplitudes; a loss factor is a frequency-domain model only.
- Beams, solids and unbaffled plates do not radiate sound; there is no
  two-way fluid-structure coupling.
- Results are kept for the session, not in the project file.
