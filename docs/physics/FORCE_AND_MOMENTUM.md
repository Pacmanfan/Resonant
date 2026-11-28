# Force and Momentum Accounting

**Status:** current (REQ-CHAT-20261126-022, RES-ACC-007..009, ADR-000226). What the
framework accounts today, and how each value is obtained.

## Conventions

- Force is not conserved. Momentum is: forces act through **impulses**, the
  momentum they transfer. A force record says where a force comes from. A
  momentum exchange says where momentum goes.
- A **flux** is momentum carried across a boundary by fields or matter. An
  **impulse** is the time integral of an external force. A momentum check
  reports both, as `input` and `impulse`.
- Torque and energy are different families (both N·m), as are angular
  momentum and the action (J·s). Neither pair is ever added.

## Lumped networks (translational and rotational)

The network integrator records each element's work and impulse on each of
its two coordinates (`TransientOptions::elementFlows`). It uses the same
Runge–Kutta stages as the state, so momentum balances to rounding:

```text
spring or damper e between i and j:  F_e,i = -k (x_i - x_j)  or  -c (v_i - v_j),   F_e,j = -F_e,i
  momentum:  e -> i : integral of F_e,i dt      e -> j (or ground) : the opposite (the reaction)
  energy:    i -> e : -integral of F_e,i v_i dt   (the body's work on the element)
             damper -> heat : the work it absorbs from both ends
  force records: F_e,i and F_e,j, each naming the other as its pair; to the ground a constraint force
applied force: driver -> i, work integral of f_i v_i and impulse integral of f_i; a force record "applied"
```

- **Elements hold no momentum.** They are massless, so the impulses on their
  two ends cancel.
- **Bodies hold their momentum.** Each body holds m·v along the network's
  axis; a rotational network holds J·ω about its axis, its springs are
  torsional, and its forces are torques.
- **The system's external force** is the sum of the forces on members from
  non-members. The pairs between members are listed as internal and cancel.

## Radiation on a layered body (`electromagnetics/RadiationForce`)

A plane wave at normal incidence on layers between vacuum half-spaces. The
force on the body is computed from the solver's own fields in two ways:

```text
surfaces:  F/A = T(0-) - T(d+),   T = (eps0 |E|^2 + mu0 |H|^2) / 4       (time-averaged vacuum Maxwell stress)
volume:    F/A = integral over the layers of (1/2) Re(J conj(mu0 H)),   J = j w eps0 (eps_r - 1) E
fluxes:    F/A = (1 + R - T) I / c
```

- **The ledger** (rate basis, per the illuminated area) has momentum fluxes
  for the incident wave (I/c), the reflected wave (−R I/c, since its
  momentum points back) and the transmitted wave (T I/c). The body's
  momentum changes at the rate of the volume force; a held body passes it to
  its support.
- **Why vacuum on both sides.** With vacuum on both sides, the total force
  does not depend on the momentum formulation inside matter (Abraham or
  Minkowski).
- **Refused configurations:**
  - a dielectric half-space;
  - a magnetic layer, whose magnetization current the volume method leaves
    out.

  Each is refused with the reason.
- **Checks.** `radiation.force-methods` compares the three values. The force
  record is the volume method's; its density profile over depth is kept.

## What is not accounted

The coupled engine's participants report no momentum; their momentum check
says "insufficient data". Also not accounted:

- the acoustic radiation force (Gor'kov potential, radiation stress);
- the Maxwell stress of the Yee and FDTD solvers;
- the molecular-dynamics momentum;
- fluid momentum transport.

Each is a planned adapter, and nothing is assumed in its place.
