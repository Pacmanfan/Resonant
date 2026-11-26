// Closed-form estimates for idealized manipulators. Each function states its
// assumptions; none of them is a field simulation, and results derived from
// them are labelled as analytical estimates.
#pragma once

#include "foundation/Types.h"

#include <string>
#include <vector>

namespace resonant::manipulators {

// --- reflectors -----------------------------------------------------------------
// Paraboloid z = r^2 / (4 f): depth at the rim of an aperture of diameter D.
double parabolaDepth(double focalLength, double apertureDiameter);
// Ellipse with semi-axes a >= b: distance of each focus from the centre.
double ellipseFocalDistance(double semiMajor, double semiMinor);
// Spherical mirror of radius R: where a ray parallel to the axis at height h
// crosses the axis after one reflection, measured from the vertex. The
// paraxial limit (h -> 0) is R / 2; larger h crosses closer to the mirror
// (spherical aberration).
double sphericalMirrorAxisCrossing(double curvatureRadius, double rayHeight);
// Mirror (imaging) equation 1/s + 1/s' = 1/f for a source on the axis at
// distance s from the vertex of a concave mirror of focal length f. Paraxial
// geometrical acoustics. Returns infinity for s = f.
double mirrorImageDistance(double focalLength, double sourceDistance);

// --- interfaces --------------------------------------------------------------------
// Plane wave at normal incidence from a fluid of impedance z1 onto a fluid
// of impedance z2 (z = rho c). Pressure reflection coefficient and the
// fraction of the incident intensity transmitted.
double pressureReflectionCoefficient(double z1, double z2);
double intensityTransmissionCoefficient(double z1, double z2);

// --- lenses -----------------------------------------------------------------------
struct LensEstimate {
    double refractiveIndex = 1.0;     // n = c_medium / c_lens (below 1 for a lens faster than the medium)
    double focalLength = 0.0;         // m, from the lens's second principal plane; negative = diverging;
                                      // infinity when the lens has no focusing power
    double surfaceTransmission1 = 0.0; // intensity transmission at normal incidence, first surface
    double surfaceTransmission2 = 0.0; // second surface
    std::vector<std::string> assumptions;
};
// Thick-lens (lensmaker's) estimate for spherical surfaces:
//   1/f = (n - 1) [ 1/R1 - 1/R2 + (n - 1) d / (n R1 R2) ]
// R1, R2 are signed radii of the first and second surface (positive when
// the centre of curvature is on the far side); 0 means a flat surface.
// Paraxial, fluid-like lens (longitudinal waves only), no absorption.
LensEstimate estimateLens(double radius1, double radius2, double centerThickness, double mediumSoundSpeed,
                          double mediumDensity, double lensSoundSpeed, double lensDensity);

// --- waveguides (rigid walls, lossless fluid) -----------------------------------
struct Mode {
    int m = 0;
    int n = 0;
    int q = 0;                 // axial index (cavities)
    double frequencyHz = 0.0;  // cut-off (waveguides) or resonance (cavities)
};
// Rectangular duct a x b: f_mn = (c / 2) sqrt((m/a)^2 + (n/b)^2). The plane
// wave (0,0) has no cut-off and is not listed.
std::vector<Mode> rectangularDuctCutoffs(double width, double height, double soundSpeed, int count);
// Circular duct of radius a: f_mn = c j'_mn / (2 pi a), j'_mn the n-th zero
// of J_m'. Lowest modes only (from a table of zeros).
std::vector<Mode> circularDuctCutoffs(double radius, double soundSpeed, int count);

// --- cavities (rigid walls, lossless fluid) ----------------------------------------
// Rectangular box: f = (c / 2) sqrt((nx/Lx)^2 + (ny/Ly)^2 + (nz/Lz)^2).
std::vector<Mode> rectangularCavityModes(double lx, double ly, double lz, double soundSpeed, int count);
// Closed cylinder of radius a and length L:
//   f = (c / 2 pi) sqrt((j'_mn / a)^2 + (q pi / L)^2).
std::vector<Mode> cylindricalCavityModes(double radius, double length, double soundSpeed, int count);
// Sphere of radius a: f = c z / (2 pi a), z a zero of the derivative of a
// spherical Bessel function. Lowest modes only (from a table).
std::vector<Mode> sphericalCavityModes(double radius, double soundSpeed, int count);
// Helmholtz resonator: f = (c / 2 pi) sqrt(A / (V L_eff)), with neck area A,
// cavity volume V and effective neck length L_eff = L + 1.7 r (both ends
// flanged). Valid when every dimension is small against the wavelength.
double helmholtzFrequency(double neckRadius, double neckLength, double cavityVolume, double soundSpeed);
// Tube closed at one end: f_n = (2 n - 1) c / (4 L), n = 1, 2, ... (no end correction).
double quarterWaveFrequency(double length, double soundSpeed, int n = 1);

}  // namespace resonant::manipulators
