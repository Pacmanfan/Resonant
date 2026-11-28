#pragma once

// A locally resonant (mass-in-mass) lattice (RES-META-006): host masses m1 on
// springs k1 of spacing a, each carrying a resonator m2 on a spring k2.
//   dispersion:  4 k1 sin^2(q a / 2) = w^2 m_eff(w)
//   effective dynamic mass of a cell:  m_eff(w) = m1 + m2 w2^2 / (w2^2 - w^2),  w2 = sqrt(k2 / m2)
// m_eff < 0 between w2 and w2 sqrt(1 + m2/m1): the locally resonant gap, which
// the effective-medium (long-wavelength) picture predicts when the lattice's own
// Bragg frequency lies far above w2. Derived, never assigned.

#include "foundation/Result.h"
#include "oscillators/BlochLattice.h"
#include "oscillators/LumpedNetwork.h"

#include <array>
#include <complex>

namespace resonant::mechanics {

struct MassInMassCell {
    double hostMass = 0.0, hostStiffness = 0.0;          // m1 (kg), k1 (N/m)
    double resonatorMass = 0.0, resonatorStiffness = 0.0;  // m2, k2
    double spacing = 0.0;                                  // a (m)
    double resonatorLossFactor = 0.0;                      // eta of k2
    Status validate() const;
};

double localResonanceHz(const MassInMassCell& cell);
// The effective dynamic mass of a cell at a frequency (complex with loss).
std::complex<double> effectiveMass(const MassInMassCell& cell, double frequencyHz);
// The two branches (Hz) at a reduced wavenumber q a / pi in [0, 1] (closed form).
std::array<double, 2> massInMassDispersion(const MassInMassCell& cell, double reducedWavenumber);
// The complex Bloch wavenumber q (1/m) at a frequency: cos(q a) = 1 - w^2 m_eff / (2 k1);
// Re q a in [0, pi] (the phase per cell), Im q a >= 0 (the attenuation per cell, Np).
std::complex<double> massInMassBlochWavenumber(const MassInMassCell& cell, double frequencyHz);

struct LatticeGap {
    double lowerHz = 0.0, upperHz = 0.0;
    double negativeMassLowerHz = 0.0, negativeMassUpperHz = 0.0;  // the effective-medium prediction [w2, w2 sqrt(1 + m2/m1)]
};
LatticeGap massInMassGap(const MassInMassCell& cell);

oscillators::BlochCell massInMassBlochCell(const MassInMassCell& cell);
// A finite chain of `cells` cells with free ends: coordinates host 0, resonator 0, host 1, ...
oscillators::Network massInMassChain(const MassInMassCell& cell, int cells);

}  // namespace resonant::mechanics
