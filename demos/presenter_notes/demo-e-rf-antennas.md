# E. RF antenna engineering: presenter notes

Read the impedance aloud: 73 ohm resistance is the classic half-wave dipole value. For the array, compare the pattern summaries before and after steering.

## Steps

1. **Open the half-wave dipole example** (`open-project`)
2. **The Electromagnetics panel** (`workspace`)
3. **Impedance and directivity of the dipole** (`run`)
4. **The dipole's radiation pattern in 3D** (`run`)
5. **Frame the pattern** (`camera`)
6. **A doughnut: no radiation along the wire, most broadside.** (`pause`)
7. **An 8 x 8 phased array** (`open-project`)
8. **Its pattern, steered as saved (30°, 45°)** (`run`)
9. **Steer the beam to broadside by phase** (`project-action`)
10. **The broadside pattern** (`run`)
11. **Frame the array pattern** (`camera`)
12. **Same array, new phases: the beam moved. Steering widens it a little.** (`pause`)

## The story

- **problem**: Know how an antenna is matched and where it radiates, and point an array's beam without moving it.
- **configure**: A copper half-wave dipole at 1 GHz; then a planar array whose elements' phases are set for a direction.
- **calculate**: The dipole's induced-EMF impedance and directivity and its pattern; the array factor pattern before and after steering.
- **meaning**: The dipole radiates a doughnut around its axis (about 2.15 dBi); the array concentrates power into a steerable beam.
- **inspect**: The impedance report, the pattern summary (directivity, beamwidth, sidelobes) and the 3D pattern in the viewport.
- **value**: The beam is steered by phases alone, and the cost (lower directivity, larger beamwidth) is shown.
- **assumptions**: Thin-wire sinusoidal-current dipole; array factor of identical elements without mutual coupling; far field.
