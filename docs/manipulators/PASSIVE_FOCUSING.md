# Passive Focusing — the Reflector Field Model and Focus Metrics

**Status:** current for REQ-CHAT-20261126-004.

## The model

Field model id `acoustic.reflector.kirchhoff`, selected in the Acoustics
panel ("Free field + reflectors"). Sources radiate as in the free-field
model. For each source with incident field `p_i`, every surface element of
a reflector, with unit normal `n` towards the source, is given the total
pressure and normal derivative an infinite plane with its reflection
coefficient `R` would have:

```text
p = (1 + R) p_i          ∂p/∂n = (1 − R) ∂p_i/∂n
```

and the scattered field is the Helmholtz–Kirchhoff integral over the
surface `S`:

```text
p_s(x) = ∫_S [ p ∂G/∂n − G ∂p/∂n ] dS,      G(x, y) = e^{−jk|x−y|} / (4π |x−y|)
```

`R = +1` rigid, `−1` pressure release, `0` absorbing, and
`(Z cos θ − ρc)/(Z cos θ + ρc)` for a locally reacting surface of impedance
`Z` at the angle of incidence `θ` (`Z` from the assigned material). The
total field is the free field plus `p_s`.

For many sources the surface values are summed once, so a field sample costs
one pass over the surface elements whatever the number of sources.

## What it includes and what it does not

Includes: the phase of every path, the interference of direct and reflected
sound, diffraction from the finite aperture (so focal spots have a
wavelength-limited size), the shadow behind a reflector.

Does **not** include, and every result says so: more than one reflection
(no resonance between surfaces), shadowing of one surface or reflector by
another, the thickness of the shell (a thin surface reflects on both
sides), anything for lenses, waveguides, cavities, diffraction structures
or ordinary geometry, and particle velocity or intensity. Its accuracy
falls where a reflector or its curvature radius is not large against the
wavelength. It is a high-frequency approximation, **not a full-wave
solution**, and neither is the ray picture.

## Choosing a model

| Situation | Model |
|---|---|
| No reflector matters | analytical free field |
| Reflectors several wavelengths across, one reflection dominates | reflector model (this document) |
| Geometry check of a shape | ray paths |
| Wavelength-scale structures, enclosures, lenses, multiple reflections | a full-wave solver (request 005); not available in this milestone |

The application never switches model by itself.

## Resolution and cost

The surface is sampled at a chosen number of elements per wavelength (2 to
40, default 6). The preflight shows the number of surface elements and
`samples × elements`, warns above 5 billion evaluations and refuses above
200 billion. A convergence test shows the result at 8 elements per
wavelength within 1 % of that at 16.

## Focus metrics

`analyzeFocus` works on a sampled field result and an optional intended
point. Definitions and units:

| Metric | Definition | Unit |
|---|---|---|
| Peak magnitude | largest `|P|` on the grid outside source and surface exclusion zones | Pa (peak) |
| Peak location | position of that sample | m |
| Width along U, V, W | full width of the region around the peak where `|P| ≥ peak/2` (−6 dB), along each sampled grid axis through the peak, with linear interpolation of the crossings; **0** when the half level is not reached on both sides inside the grid | m |
| Focal displacement | distance from the peak to the intended point | m |
| Magnitude at the intended point | trilinear sample of `|P|` there, when the point lies in the sampled region | Pa (peak) |
| Sidelobe ratio | largest other local maximum of `|P|` along U through the peak, divided by the peak | — |
| Focal gain | `|P|` with the reflectors divided by `|P|` of the same sources without them, at a point | — |

The intended point is the first target point of the scene, or else a
reflector's intended focus. There is no single "focusing efficiency": no
energy-based efficiency is reported, because the model provides no
intensity and a fair definition differs between configurations.

## Comparison workflow

Run the same region with different models or designs; each run is a stored
result with its own configuration. The **Result Comparison** tab lists, for
every result of the session: model, peak, peak location, the three widths,
distance to the intended point, magnitude there, sidelobe ratio and solver
time. Free field, active, passive and hybrid configurations are compared
this way.
