#pragma once

// A duct loaded periodically by Helmholtz side branches (RES-META-007): an
// acoustic locally resonant metamaterial. Plane waves in the duct; each cell
// is half a duct segment, the resonator's shunt admittance Y = 1/Z_H (its
// lumped impedance, the neck opening into the duct), and another half
// segment. Bloch: cos(q a) = (T11 + T22) / 2 of the cell transfer matrix.
// Effective medium (long wavelength): the resonators add compressibility,
//   1 / B_eff = 1 / B0 + Y / (j w S a),
// so B_eff < 0 just above the resonance: the hybridization gap; the Bragg gap
// lies near c / 2a.

#include "foundation/Result.h"
#include "foundation/Types.h"
#include "manipulators/Helmholtz.h"

#include <array>
#include <vector>

namespace resonant::manipulators {

inline constexpr const char* kResonatorDuctModelId = "acoustic.resonator-duct.transfer-matrix";

struct ResonatorDuctSpec {
    double ductRadius = 0.0;   // m
    double spacing = 0.0;      // a, m
    HelmholtzSpec resonator;   // its medium is the duct's
    Status validate() const;
    double ductArea() const;
};

// The cell transfer matrix [p, U]_in = T [p, U]_out (row-major: T11, T12, T21, T22).
std::array<Complex, 4> resonatorDuctCell(const ResonatorDuctSpec& spec, double frequencyHz);
// The complex Bloch wavenumber (1/m): Re q a in [0, pi] (the phase per cell), Im q a >= 0 (the attenuation, Np).
Complex resonatorDuctBloch(const ResonatorDuctSpec& spec, double frequencyHz);
// The effective bulk modulus (Pa, complex).
Complex resonatorDuctBulkModulus(const ResonatorDuctSpec& spec, double frequencyHz);
// Power transmission of `cells` cells between anechoic ducts.
double resonatorDuctTransmission(const ResonatorDuctSpec& spec, double frequencyHz, int cells);

struct DuctGap {
    double lowerHz = 0.0, upperHz = 0.0;
};
// Stop bands below fmax (|cos q a| > 1 for a lossless resonator), edges to a relative 1e-9.
std::vector<DuctGap> resonatorDuctGaps(const ResonatorDuctSpec& spec, double fmaxHz, int samples = 4000);

}  // namespace resonant::manipulators
