# Atomistic Models

**Status:** current (REQ-CHAT-20261126-020). **Requirements:** RES-MSC-008..010. **Code:** `src/core/atomistic`.

These are *reference implementations* for pair potentials, validated against
lattice sums and conservation laws. They do not replace a molecular-dynamics
package; external engines are reached through adapters (none integrated).

## Structures and import

`Crystal`: lattice vectors (columns, m), atoms (species, fractional
positions), or non-periodic Cartesian atoms. Builders: sc, bcc, fcc,
diamond. `supercell(n1, n2, n3)`. Import (angstrom → m): plain XYZ (no
cell: not periodic), extended XYZ (`Lattice="..."`), VASP 5 POSCAR (Direct
or Cartesian, selective dynamics ignored). Refused rather than guessed: a
POSCAR without species names (VASP 4), atom counts that disagree, a
multi-frame XYZ, CIF and PDB.

## Force fields

`ForceField`: id, version, form (12-6 Lennard-Jones, Lorentz–Berthelot
mixing), species parameters (ε, σ, mass) - the applicability - default
cutoff, source, validity and limitations. Built in: `lj.argon` (ε/k_B =
119.8 K, σ = 3.405 Å, 39.948 u) and `lj.reduced` (ε = σ = m = 1). A
structure with species outside the force field is refused.

## Lattice statics

E per atom: the pair sum within the cutoff plus the tail of a uniform
density beyond it. Relaxation: dE/ds = 0 by bisection (1e-13 in the scale).
Elastic constants C_ij (Voigt) by central differences of the energy density
at strain 1e-4; Born stability (C positive definite), the Cauchy deviation
|C12 − C44|/C44, the internal force under shear (non-zero: the basis would
relax, and the constants are unrelaxed upper estimates), Voigt–Reuss–Hill
moduli, density and isotropic sound speeds. The derivation repeats the
calculation at 1.5 times the cutoff and reports the difference as the
uncertainty.

## Molecular dynamics (NVE)

Velocity Verlet, orthorhombic periodic cells (at least twice the cutoff),
minimum image, a shifted-force cutoff (2.5 σ by default), Maxwell–Boltzmann
initial velocities with zero momentum. Observables: energies, temperature,
g(r), MSD. The derivation also runs at half the time step: the energy drift
must fall by about four (velocity Verlet), and a drift above 1e-3 of the
kinetic energy marks the run not reliable. MD derives no library property.

## Example: solid argon

| Quantity | Lennard-Jones (0 K, static) | Note |
|---|---|---|
| lattice parameter | 5.250 Å | measured near 0 K: 5.31 Å (zero-point motion expands it) |
| cohesive energy | 0.0889 eV per atom | 8.6102 ε (Kittel's lattice sums) |
| C11, C12, C44 | 4.40, 2.52, 2.52 GPa | C12 = C44 (Cauchy): real argon violates it - a limit of pair potentials |
| bulk modulus | 3.15 GPa | 75.2 ε/σ³ |
| Young's modulus, Poisson's ratio (Hill) | 4.32 GPa, 0.271 | |
| density, longitudinal speed | 1834 kg/m³, 1718 m/s | |

## External engines and the electronic scale

LAMMPS, OpenMM, Quantum ESPRESSO, CP2K, GPAW and phonopy are listed
(`MultiscaleService.ListExternalSolvers`) with their licenses, what they
provide, how they would be installed (administrator rights for the
distribution packages) and whether an executable is on PATH; each is "not
integrated". An adapter would write inputs, run the engine as a separate
process (never linked) and read its outputs into a derivation record
(`docs/physics/MULTISCALE_MODELING.md`). Electronic-structure
representations exist; no electronic-structure result is ever shown.
