# Resonance and Modal Analysis

**Status:** current for REQ-CHAT-20261126-005. **No eigenmode solver
exists.** What exists: analytical mode frequencies of simple shapes, and
resonance identification from the response of a full-wave run.

## Analytical (`docs/manipulators/RESONANT_CAVITIES.md`, `WAVEGUIDES.md`)

Rigid-walled box, cylinder and sphere resonances; duct cut-offs; Helmholtz
and quarter-wave resonators.

## From a full-wave run

Excite the enclosure with a broadband pulse (a Ricker wavelet), record a
probe, simulate long enough, and look at the spectrum: the peaks are where
the enclosure responds strongly.

`findPeaks` returns local maxima of the spectrum magnitude above a
fraction of the largest, refined by a parabola through three points.

**These are response peaks, not eigenmodes.**

* A mode with a node at the source is not excited; one with a node at the
  probe is not seen. Put both in corners, away from symmetry planes.
* A peak's frequency is uncertain by about half the resolution (`2/T` for
  the Hann window).
* Two modes closer than the resolution appear as one peak.
* The peak heights depend on source and probe positions and on the pulse's
  spectrum; they are not modal amplitudes.
* No mode shape is obtained. A steady-state run at a peak frequency shows
  the field pattern at that frequency, which near an isolated resonance
  resembles the mode shape; nodal lines can be seen in it.

## Validation

A rigid 2D box 60 × 45 mm, Ricker pulse, 50 ms, Hann window (resolution
40 Hz): the peaks nearest the analytical frequencies of modes (1,0), (0,1),
(1,1), (2,0), (2,1), (0,2) are within 20 Hz of them (2833 Hz to 7556 Hz).

A rigid cavity object 40 × 30 mm in a scene (example E), 30 cells per
wavelength, 10 ms (resolution 200 Hz): modes (1,0), (0,1), (1,1), (2,0)
within 100 Hz of the frequencies of the cavity **as the grid holds it**
(its inner size counted in cells), and within 4 % of the designed cavity.
The difference is staircasing: the walls are where the grid puts them,
within one cell.

## What a real eigenmode analysis would need

The generalized eigenvalue problem `K p = w^2 M p` of a finite-element or
finite-difference discretization, a sparse eigensolver (shift-invert
Lanczos), a mesh that follows curved walls, and for open or lossy systems
complex eigenvalues. None of it is present.
