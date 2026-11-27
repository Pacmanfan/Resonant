# B. Acoustic focusing: presenter notes

Show the uniform result long enough for the audience to see the beam. Then add the target and focus: point out that Edit > Undo would restore the original phases. Compare the peak pressures in the Field Analysis panel.

## Steps

1. **Open the linear array example** (`open-project`)
2. **Acoustics panel and properties** (`workspace`)
3. **Frame the scene** (`camera`)
4. **Field with every element in phase (uniform excitation)** (`run`): A broad beam straight ahead: no focus.
5. **Decibels show the beam and sidelobes** (`visualization`)
6. **In phase, the array makes a beam, not a spot.** (`pause`)
7. **Add a focus target 5 cm in front of the array** (`project-action`)
8. **Focus the array on the target by phase** (`project-action`): Each element gets the phase that makes its wave arrive in step with the others at the target.
9. **Field of the focused array** (`run`)
10. **The Field Analysis summary compares the results** (`show`)
11. **Now the energy is concentrated at the target: a focus several times stronger than the beam.** (`pause`)

## The story

- **problem**: Concentrate ultrasound at a chosen point (haptics, levitation, sensing) with a fixed array.
- **configure**: A focus target 5 cm in front of the array, then the focusing command (phase control).
- **calculate**: The element phases that make the waves arrive in phase at the target, and the pressure field before and after.
- **meaning**: Uniform excitation makes a broad beam; focusing makes a tight, much stronger spot where the target is.
- **inspect**: The field in decibels on the plane, and the Field Analysis summary (peak, focal size) of each result.
- **value**: The phases are calculated, not guessed, and the result is checked by a field calculation of the whole array.
- **assumptions**: Free field, omnidirectional point-source elements, phase control at one frequency (40 kHz).
