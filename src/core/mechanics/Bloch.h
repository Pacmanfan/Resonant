#pragma once

// Band structure of a periodic rod (longitudinal waves), by a Bloch-
// periodic finite-element unit cell, and the closed-form reference.
//
// The unit cell is two segments of lengths lA, lB and areas AA, AB of one
// material. Longitudinal waves of the 1D rod theory; the cell is
// discretized with linear two-node rod elements, consistent mass. With
// u(right end) = exp(j q a) u(left end) the cell's matrices become Hermitian
// and their eigenvalues are the frequencies of the Bloch waves of
// wavenumber q. The reference is the classical dispersion relation of a
// layered 1D medium,
//
//   cos(q a) = cos(k lA) cos(k lB) - (1/2) (ZA/ZB + ZB/ZA) sin(k lA) sin(k lB),
//
// k = omega / c, Z proportional to the area (same material). A frequency
// where the right side exceeds 1 in magnitude has no propagating wave: a
// stop band.
#include "geometry/Geometry.h"
#include "mechanics/ElasticMaterial.h"

#include <utility>
#include <vector>

namespace resonant::mechanics {

struct PeriodicRod {
    ElasticMaterial material;
    double areaA = 0.0, areaB = 0.0;     // m^2
    double lengthA = 0.0, lengthB = 0.0; // m
    int elementsPerSegment = 24;
    double period() const { return lengthA + lengthB; }
};

struct BandStructure {
    std::vector<double> reducedWavenumber;           // q a / pi, 0 .. 1
    std::vector<std::vector<double>> frequencies;    // [band][q], Hz
    std::vector<std::pair<double, double>> stopBands;  // between consecutive bands, Hz
    std::string method;
};

BandStructure rodBandStructure(const PeriodicRod& rod, int bands, int wavenumbers);

// Right side of the dispersion relation at a frequency.
double rodDispersionRhs(const PeriodicRod& rod, double frequencyHz);
// Stop bands of the closed-form relation below fmax, found by scanning.
std::vector<std::pair<double, double>> analyticalStopBands(const PeriodicRod& rod, double fmax, int samples = 200000);

// The unit cell of a rod.periodic geometry (scaled by `lengthScale` along
// the rod and `radiusScale` across it).
Result<PeriodicRod> periodicRodOf(const Geometry& geometry, const ElasticMaterial& material, double lengthScale = 1.0, double radiusScale = 1.0);

}  // namespace resonant::mechanics
