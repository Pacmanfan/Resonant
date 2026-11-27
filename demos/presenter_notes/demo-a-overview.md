# A. Resonant overview: presenter notes

Start by saying what Resonant is for: designing systems that shape waves. Keep the mouse still while the field calculates; the viewport updates when the job finishes. Finish on the help window: everything shown has a help page.

## Steps

1. **Open the focused planar array example** (`open-project`)
2. **A clean layout: scene tree, properties, acoustics** (`workspace`)
3. **Frame the scene from the side** (`camera`)
4. **Select the array: its properties appear on the right** (`select`): Point at the element table: 8 x 8 elements, 4.3 mm apart, each with its own phase.
5. **The scene tree holds every object: the array, the focus target and the field plane.** (`pause`)
6. **Calculate the pressure field on the plane** (`run`)
7. **Show the field in decibels** (`visualization`)
8. **Look at the focal plane** (`camera`)
9. **The bright spot is the focus: every element's wave arrives there in phase.** (`pause`)
10. **The same project hosts mechanical studies** (`workspace`)
11. **... and electromagnetic ones** (`workspace`)
12. **Save a copy of the project (project management)** (`project-action`)
13. **Every panel has its help page (F1)** (`help`)

## The story

- **problem**: Engineers who shape waves need one place to build a design, calculate what it does and see it.
- **configure**: Nothing: the demonstration opens a saved project with a focused planar array, a target and a field plane.
- **calculate**: The free-field pressure of the 64 elements on the field plane (analytical superposition of point sources).
- **meaning**: The bright spot is where the elements' waves arrive in phase: the focus the array was set up for.
- **inspect**: The scene tree lists the objects; the property editor shows the selected array; the viewport shows the field in 3D.
- **value**: Design, simulation and results stay in one project file, for acoustics, mechanics and electromagnetics alike.
- **assumptions**: Free field (no walls), linear acoustics, ideal point-source elements at 40 kHz in air.
