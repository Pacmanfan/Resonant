---
id: concepts.accounting
title: Energy, Momentum and Force Accounting
category: Core Concepts
order: 34
status: available
summary: Where the energy goes - inputs, storage, transfers between domains, dissipation and outputs for any system you draw a boundary around; momentum, forces and torques with their origins; conservation checks that say honestly when they could not be made.
keywords: [energy, power, momentum, force, torque, angular momentum, entropy, conservation, ledger, balance, residual, Q factor, radiation pressure, units]
related: [concepts.material-state, concepts.multiscale, opt.functional-structures]
---

When energy enters a structure, where does it go? **Accounting**
(**Panels > Accounting**) follows it through the calculation's own
numbers. It tracks five kinds of flow:

- what comes in;
- what is stored, and in which form;
- what moves from one part or physical domain to another;
- what is dissipated as heat;
- what leaves.

Open a project in `examples/accounting`, choose a **Study** and **Run**.

## Pick the system

The same exchange looks different depending on where you draw the
boundary. Energy passing between two coupled resonators is *internal* to
the pair, but it *leaves* resonator 1 if resonator 1 is all you look at.
To change the boundary:

- choose a **System** the study defines; or
- choose **Custom**, tick the members you want and press **Apply
  boundary**.

Nothing runs again: the same numbers are classified for the new boundary.

## What the tabs show

- **Flows:** bands from what gives to what receives. Each band is as wide
  as its amount and labelled with its value and unit. Switch **Quantity**
  to see momentum.
- **Conservation:** one row per law or relation, with its status, residual,
  tolerance and why. The statuses are:
  - *verified* or *warning*;
  - *exceeded*;
  - *insufficient data*: something needed is missing, and it says what;
  - *not applicable*.
- **Breakdown:** input, stored, internal, dissipated, output and residual,
  with every exchange and stored quantity under them. Energy locked in
  plastic hardening is marked *not recoverable*.
- **Forces:** each force with its origin (spring, damper, applied,
  constraint, radiation pressure), its target, vector and method. The two
  ends of a spring are an action-reaction pair.
- **Over time:** stored energy, cumulative input and dissipation, and each
  check's residual over time.

**Save record** keeps the accounting in the project, and you can undo it.
**Export…** writes Markdown, CSV or JSON.

## Reading a residual

- **Numerical error is not loss.** Only what a model reports as dissipated
  is dissipated. A leftover residual is compared with the numerical errors
  the solvers state (integration, mapping, quadrature) and is labelled
  either *within* them or *unaccounted*.
- **Units keep meanings apart.** Torque and energy both have the unit N·m,
  but they are never added. Power (W) is not energy (J). A momentum flow
  is not a force.
- **Q of a resonator:** Q = ω × stored energy / power lost, per loss
  mechanism. It is checked against the resonator's own modes, and only for
  light damping.

## Limits

- **Momentum is accounted for:** lumped networks (moving and rotating
  parts), and the radiation force of a plane wave on a layered body between
  vacuum half-spaces.
- **Momentum is not accounted for:** the coupled multiphysics runs report
  none (shown as *insufficient data*), and there is no acoustic radiation
  force or Maxwell stress from the full-wave solvers.
- **Entropy:** only the melting slab and isothermal material models give
  it.
