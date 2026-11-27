---
id: mechanics.waves
title: Mechanical waves
category: Mechanical Engineering
order: 70
status: available
summary: Longitudinal, torsional, bending and membrane waves in structures, and the transient (Newmark) analysis that shows them travel.
keywords: [wave, transient, Newmark, dispersion, bending wave, pulse, reflection]
related: [mechanics.forced-response, mechanics.resonators, mechanics.actuators, acoustics.physics, sim.accuracy]
---

A wave in a solid is not a sound wave in air. Each structural
idealization carries its own kinds of wave, with their own speeds, and
bending waves are dispersive: their speed depends on frequency. The
**Transient** analysis lets you watch these waves travel and reflect.

## Wave types

| Wave | Carried by | Speed | Dispersion |
|---|---|---|---|
| Longitudinal | beam, axial | c = √(E / ρ) | none in rod theory |
| Torsional | circular beam | √(G / ρ) | none |
| Bending (flexural) | beam | c_phase = √ω · (E I / (ρ A))^(1/4) | strong |
| Bending (flexural) | plate | c_phase = √ω · (D / (ρ h))^(1/4) | strong |
| Transverse | membrane | √(T / (ρ h)) | none |
| Bulk P and S | solid | √((λ + 2G) / ρ), √(G / ρ) | none in the continuum |

E is Young's modulus, G the shear modulus, ρ the density, A and I the
section's area and second moment, D the plate's bending stiffness, h the
thickness and T the membrane tension (see
[Mechanical materials](materials.md)). For bending waves the
energy travels at the group velocity

```math
c_group = 2 · c_phase
```

so higher frequencies arrive first. A bending tone burst at 2 kHz on a
4 mm steel strip travels at about 540 m/s, twice its phase speed of about
270 m/s; at 5 kHz both are 1.6 times faster (validated within 6 %).

Euler-Bernoulli and Kirchhoff theory are valid while the bending
wavelength is at least about 6 times the thickness; above that their phase
speed keeps growing, which real structures do not do.

## Transient analysis

The **Transient** tab (or **Mechanics > Run Transient Analysis**)
integrates M a + C v + K u = f(t) in time with the implicit Newmark
average-acceleration method:

```math
u_{n+1} = u_n + Δt v_n + (Δt² / 4)(a_n + a_{n+1})
v_{n+1} = v_n + (Δt / 2)(a_n + a_{n+1})
(T_num − T) / T ≈ (ω Δt)² / 12
```

It is unconditionally stable, second-order accurate and has no numerical
damping, but it lengthens periods by the amount on the last line. The
structure starts at rest.

| Field | Meaning |
|---|---|
| Simulated time | duration in ms |
| Steps per period | Δt = 1 / (steps × f_max), f_max the highest significant frequency of the actuators' signals (default 20: 0.8 % period error at f_max) |
| Time step | *Automatic* (from the line above) or a value in µs |
| Rayleigh alpha, Rayleigh beta | damping C = αM + βK; zero for none |
| Instants to keep | full displacement fields recorded, evenly spaced (default 60) |

Press **Run Transient Analysis**. The plot shows a sensor's displacement,
velocity or acceleration over time, or *Energy*: kinetic plus strain
energy and the work done by the forces. Step through the recorded instants
with **Mode / frequency / instant** in the Display group.

## Worked example: a pulse on a rod

Open `examples/mechanics/e-wave-propagation.resonant`: a free steel rod
2 m long (radius 5 mm, 400 elements of 5 mm), struck axially at one end by
a 100 N Gaussian pulse at 20 kHz, with sensors 0.5 m and 1.5 m from the
struck end. For this pulse f_max is 36 kHz, so the automatic time step is
1.39 µs. Run the transient analysis (0.8 ms) and plot the velocity at both
sensors:

- the pulse passes the second sensor 0.198 ms after the first: the rod
  speed √(E / ρ) = 5048 m/s, reproduced within 1.5 %;
- it returns from the free far end with the **same sign of velocity**
  (a free end doubles the velocity and inverts the stress).

`examples/mechanics/d-transient-vibration.resonant` strikes a clamped
steel plate with a Ricker pulse (light Rayleigh damping, alpha 2 1/s,
0.2 s, 80 instants): the energy decreases after the impulse and the
sensor's spectrum peaks at the plate's natural frequencies.

## Numerical dispersion

Finite elements add their own dispersion: consistent-mass elements make
waves slightly fast and Newmark integration slightly slow. At 20 elements
per wavelength and 20 steps per period both stay below 1 %. The study warns
below 6 elements per structural wavelength. Mesh modes that the time step
does not resolve are not damped and can add noise; the result says so.

## Common mistakes

- **Loss factor in a transient run.** A loss factor has no causal
  time-domain form and is refused; use Rayleigh damping.
- **Too short a simulated time.** Allow the wave to cross the structure:
  length divided by the wave speed, plus the pulse length.
- **Expecting acoustic behaviour.** Bending waves in plates are dispersive;
  no scalar acoustic model reproduces them.

## Limitations

- Stress and strain histories are not recorded; velocity and acceleration
  fields of the instants are not kept (sensors record all three).
- No Timoshenko or Mindlin theory for short bending waves.
- Joints between different structures, material changes along a member
  and multi-member frames are not implemented; a horn or periodic rod is
  one member of varying section ([Resonators](resonators.md)).
- Transient waves on plates are not compared with a reference solution.
