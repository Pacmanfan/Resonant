# Spatial Measurement

RES-EXP-013.

## Positions

`ScanDefinition` (`Spatial.h`): fixed points, imported points
(`importPositionsCsv`: `x,y,z[,dx,dy,dz]` per line in metres, `#` comments,
an optional header), probe arrays, a linear scan (count points from start to
end), planar and volumetric grids (origin plus multiples of up to three
axes), and angular sweeps around a centre in the azimuth (XY, angle from +X)
or elevation (XZ, angle from +Z) plane with the probe pointing at the centre.
Every pose has a frame (`scene` is the project's world frame) and a position
uncertainty.

With a positioner, the run moves the probe binding's sensor to each position
and acquires there; each record carries the actual pose (and the angle for
angular scans). Without one, the sensors' own poses are recorded (a fixed
microphone or probe array).

## Spatial data

A run whose processing yields single-frequency values (`tone`) at known
poses produces a `SpatialDataset`: quantity, frequency, category, frame and
samples (pose, complex value, standard uncertainty, angle, measurement id).

## Reconstruction

`reconstructPlane` estimates values on a plane from the samples within a
tolerance of it, by inverse-distance weighting (power 2) of the complex
values within a stated radius. Every node is marked:

| Coverage | Meaning |
|---|---|
| measured | a sample lies at the node (within the node tolerance); its value is the sample |
| interpolated | estimated from samples within the radius |
| empty | no sample within the radius: no value (never extrapolated) |

Samples in another frame are refused until transformed. Interpolating
complex values assumes the spacing is small compared with the wavelength;
coarser data should be shown as points only. The viewport shows measured
points as markers coloured by magnitude and draws nothing between them;
sparse measurements are not a measured continuous field.

Automated scanning hardware is reached through the `Positioner` interface;
the simulated positioner is the only one in this version.
