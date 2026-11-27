---
id: trouble.solver-failures
title: Solver failures
category: Troubleshooting
order: 40
status: available
summary: Why a calculation is refused or fails, where the reason is shown, and what to change for each solver.
keywords: [refused, preflight, unstable, solver, error, cannot run]
related: [sim.solver-selection, trouble.memory, trouble.mesh, ref.numerical-limits, sim.convergence]
---

Resonant checks every calculation before it starts. When a request cannot
be met as asked, it is **refused with the reason and what to change**: the
resolution is never lowered, the backend never switched and nothing left
out without saying so. A run that starts can still **fail** (for example
when the numbers stop being finite); its partial results are then not
offered as results.

## Where the reason appears

| Calculation | Check it with | A refusal reads |
|---|---|---|
| Acoustic field (analytical or reflector model) | **Acoustics** panel, **Simulation** tab: the estimate under **Region** | the reason below the estimate; **Run Field Simulation** stays disabled |
| Acoustic full-wave (FDTD) | **Full-Wave Solver** panel, **Check** | `CANNOT RUN: ...` in **Preflight** |
| Electromagnetic full-wave (Yee) | **Electromagnetics** panel, **Fields** tab, **Plan** | `REFUSED: ...` |
| Mechanical analyses | **Mechanics** panel, **Check** | `CANNOT BE ANALYSED: ...` |
| Optimization candidates | **Optimization Studio**, **Candidates** tab | status `invalid-design`, `refused` or `failed` with its message |

Failures during a run are reported in the panel's status, in
**Analysis > Simulation Status** and in **Analysis > Console**.

## Nothing to calculate

| Message | What to do |
|---|---|
| `select a field region: it defines the computational domain` | add **Acoustics > Field Plane** or **Field Volume** and choose it under **Region** |
| `there is no active emitter in the scene` | add an emitter or array, or tick **Active** |
| `no active emitter lies in the plane of the region (within one cell of it)` | a plane region gives a 2D calculation: move the emitter into the plane |
| `There are no active sources at this frequency; the field would be zero.` | only active emitters whose own frequency equals the panel's **Frequency** take part: give them the same frequency |
| `there is no dipole or monopole antenna and no electric-dipole source in the scene` | the Yee solver represents only these radiators |
| `no active analytical radiator at <f> Hz in the scene` | analytical EM fields need an active antenna or source at that frequency |
| `select a structure: an object with geometry made structural (Mechanics menu)` | use **Mechanics > Make Beam / Plate / Membrane / Solid** first |
| `assign a material with mechanical properties (density, Young's modulus, Poisson's ratio) to '<name>'` | assign a material from the library that has a mechanical property set |
| `there is no active actuator on the structure` | add **Mechanics > Add Actuator** for a frequency response |

## Settings out of range

Settings are checked before anything else, for example
`the resolution must be between 4 and 200 cells per wavelength`,
`an absorbing layer must be between 4 and 200 cells thick` or
`the number of modes must be between 1 and 500`. The full list of ranges is
in [Numerical limitations](../reference/numerical-limits.md).

## Too large to run

Memory and work limits refuse a run with the numbers, for example
`The calculation needs N MiB (C cells), more than the memory limit of
1024 MiB. Reduce the region, the frequency, the cells per wavelength or the
number of snapshots, or raise the limit. The resolution is not lowered
automatically.`, where N and C are the figures of your problem. See
[Memory issues](memory.md) for what each limit counts and how to
bring a problem within it.

**Worked example.** `examples/advanced_acoustics/a-free-field-pulse-3d.resonant`
runs a 34 mm cube at 40 kHz in under a second. Enlarging its region to a
300 mm cube at 12 cells per wavelength needs about 88 million cells and
4.6 GiB: the preflight refuses it at the default 1 GiB limit. A 100 mm cube
(4.4 million cells, about 240 MiB) runs in about 45 seconds.

## The chosen backend cannot run

`CUDA device 0 (<name>) cannot be used: ...`, `the problem needs N MiB on the
GPU, but <name> has M MiB free` or `CUDA was chosen, but this build has no
CUDA support ...`: choose *CPU (reference, double precision)* under
**Compute**, or free the GPU. See [Missing dependencies](dependencies.md).

## A run failed

| Message | Cause and remedy |
|---|---|
| `the field stopped being finite at step N (t = ... s): the calculation is numerically unstable and its results are not usable` | an unstable configuration; keep **Time step / stability limit** at or below 1 (default 0.95) and check material values |
| `the dynamic stiffness is singular at <f> Hz: an undamped natural frequency. Add damping or move the frequency` | a frequency of the sweep hit a natural frequency of an undamped model: choose a **Damping** model |
| `the structure is not held: it can move as a rigid body. Add supports before a static analysis` | set at least one **Support** to *Pinned* or *Clamped* |
| `every degree of freedom is held: nothing can move` | too many supports; free some |
| `the dense eigenvalue solver did not converge` or `K - sigma M could not be factorized: ...` | check the material values and the supports; a free-free structure has rigid-body modes near 0 Hz |
| `the solution stopped being finite at step N` | a transient run diverged; check the time step and the damping |
| `cancelled` | the job was cancelled; the data recorded so far is not a result |

## It runs, but the result looks wrong

Read the warnings first: each one names a limitation of this run. Frequent
ones are a coarse resolution (`... cells per wavelength is coarse: the
numerical phase velocity is up to ... % low ...`), a simulated time shorter
than one crossing of the domain, reflecting domain faces with a continuous
signal, emitters outside the domain (`N emitter(s) lie outside the domain
and are NOT simulated.`) and objects the solver cannot represent
(`'<name>' ... cannot be represented by this solver and does NOT affect the
result.`). Hidden objects are left out of every calculation. The analytical
acoustic field ignores all objects; the reflector model includes only
reflectors. [Choosing a solver](../simulation/solver-selection.md) explains which
solver represents what.

## Related topics

- [Choosing a solver](../simulation/solver-selection.md)
- [Memory issues](memory.md)
- [Mesh problems](mesh.md)
- [Numerical limitations](../reference/numerical-limits.md)
