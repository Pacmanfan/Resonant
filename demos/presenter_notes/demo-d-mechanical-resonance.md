# D. Mechanical resonance: presenter notes

Let each mode animate for a few seconds. Tie the forced response peaks back to the mode table.

## Steps

1. **Open the vibrating beam example** (`open-project`)
2. **The Mechanics panel** (`workspace`)
3. **Frame the beam** (`camera`)
4. **Modal analysis: natural frequencies and shapes** (`run`)
5. **First bending mode** (`show`): The first mode: the tip moves most. About 16 Hz for this beam.
6. **The first natural frequency: the beam bends once, the tip moves most.** (`pause`)
7. **Second mode** (`show`)
8. **Third mode** (`show`)
9. **Higher modes bend more times, at higher frequencies.** (`pause`)
10. **The same beam with a shaker at the tip** (`open-project`)
11. **The harmonic (forced response) tab** (`workspace`)
12. **Forced response over frequency** (`run`)
13. **The response peaks at the natural frequencies: resonance.** (`pause`)

## The story

- **problem**: Know at which frequencies a part resonates, and how it moves there, before it is built.
- **configure**: An aluminium beam clamped at one end (material, supports and mesh are in the project).
- **calculate**: The natural frequencies and mode shapes (finite-element modal analysis), then the steady forced response to a tip force.
- **meaning**: At a natural frequency a small force produces a large motion: the first bending mode is the easiest to excite.
- **inspect**: The mode table (frequency, effective mass), the animated mode shape in the viewport, the frequency response plot.
- **value**: Resonances found in minutes, with the shape that tells where to stiffen, damp or place a sensor.
- **assumptions**: Linear elasticity, small displacements, the beam idealization and mesh of the project, light damping.
