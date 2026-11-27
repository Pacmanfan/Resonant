# C. Passive acoustic focusing: presenter notes

Explain that nothing is driven except the single distant source. The focus comes from the shape. Mention that lenses and waveguides are modelled by the full-wave solver (experimental).

## Steps

1. **Open the parabolic reflector example** (`open-project`)
2. **Scene, properties and acoustics** (`workspace`)
3. **The reflector: a paraboloid of aluminium** (`select`): Show its focal length and diameter, and its material (aluminium) in the property editor.
4. **Frame the reflector and the field plane** (`camera`)
5. **A source 1 m away lights the reflector; the plane cuts through its focus.** (`pause`)
6. **Calculate the field with the reflector model** (`run`)
7. **Show the field in decibels** (`visualization`)
8. **The reflected waves meet at the focus: a bright spot where the probe sits.** (`pause`)

## The story

- **problem**: Concentrate sound without an array: with a shaped, reflecting surface.
- **configure**: A parabolic aluminium reflector, a source 1 m away on its axis, a field plane through the focus.
- **calculate**: The field scattered by the reflector with the Kirchhoff (physical-optics) reflector model plus the incident field.
- **meaning**: The reflector's surface sends the reflected waves to one point: the geometric focus of the parabola.
- **inspect**: The reflector's geometry and material in the property editor; the field in decibels; the probe at the focus.
- **value**: Passive focusing is cheap and robust; Resonant shows how well a given shape and size focus at a given frequency.
- **assumptions**: Kirchhoff approximation: the surface is large compared to the wavelength, no multiple reflections, rigid reflector.
