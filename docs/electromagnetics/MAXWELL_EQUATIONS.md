# Maxwell's Equations and Conventions

## Equations

In a linear, isotropic medium with permittivity eps, permeability mu and
conductivity sigma:

    curl E = -mu dH/dt
    curl H =  eps dE/dt + sigma E + J

With time dependence `e^{+j omega t}` the phasor form is

    curl E = -j omega mu H
    curl H =  j omega eps_c E + J,   eps_c = eps0 er (1 - j tan d) - j sigma / omega

The analytical models solve these exactly for their sources (the fields of
a sinusoidal current filament, of a current element, of a small loop, the
modes of an ideal guide). The Yee solver discretizes the time-domain form
on a staggered grid (`ELECTROMAGNETIC_SOLVERS.md`).

## Conventions (RES-EM-002)

* **Units:** SI throughout: V/m, A/m, W/m^2, ohm, S/m, Hz, m, s.
* **Time dependence:** `e^{+j omega t}`. A wave travelling along +z is
  `exp(-j k z)`; the wavenumber of a lossy medium has a negative imaginary
  part; reactance of an inductor is positive.
* **Phasors are peak values.** A feed current of 1 A means a current
  i(t) = 1 A cos(omega t + phase); time-averaged power is 1/2 Re{V I*}.
* **Angles:** theta from local +Z, phi from local +X towards +Y, in the
  object's own frame (dipoles along local +Z).
* **Constants:** mu0 = 1.25663706212e-6 H/m (CODATA 2018), c = 299 792 458
  m/s, eps0 = 1 / (mu0 c^2), eta0 = sqrt(mu0 / eps0) = 376.730313668 ohm.
* **dB:** field levels 20 log10, powers 10 log10; directivity and gain in
  dBi (relative to an isotropic radiator); return loss positive.

## What is shown as absolute

Field results from analytical models and from the Yee solver are absolute
(V/m for a stated feed current or source amplitude). Patterns are
normalized (|E| / max) and say so. The display states whether its colour
scale is normalized to the result or absolute.
