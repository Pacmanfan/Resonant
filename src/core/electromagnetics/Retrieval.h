#pragma once

// Effective-parameter retrieval of a slab from its reflection and
// transmission (RES-META-004): the impedance and index method (Smith et al.,
// PRB 65, 195104, 2002; Chen et al., PRE 70, 016608, 2004) in the e^{+jwt}
// convention of Resonant:
//   z = +- sqrt(((1 + r)^2 - t^2) / ((1 - r)^2 - t^2)),  Re z >= 0
//   P = e^{-j n k0 d} = t / (1 - r (z - 1)/(z + 1))
//   Re n = -unwrap(arg P) / (k0 d),  Im n = ln|P| / (k0 d)   (Im n <= 0 when passive)
//   eps = n / z,  mu = n z
// The branch of Re n is fixed by phase continuity from the lowest frequency,
// which must satisfy |Re n| k0 d < pi. Points are flagged where the slab is
// not passive (Im n > 0 or Re z < 0: the wave grows), where the impedance is
// ill-conditioned (Fabry-Perot zeros of the denominator) or where the cell is
// not small against the effective wavelength (a / lambda_eff > 0.1: not a
// local material). A passive slab whose retrieved Im eps or Im mu is positive
// shows the antiresonance of a periodic cell (Koschny et al., PRE 68, 065602,
// 2003): spatial dispersion; the parameters are effective, not strictly local.

#include "foundation/Result.h"
#include "foundation/Types.h"

#include <string>
#include <vector>

namespace resonant::em {

struct RetrievedPoint {
    double frequencyHz = 0.0;
    Complex index, impedance, permittivity, permeability;
    bool passive = true;          // Im n <= 0 and Re z >= 0
    bool antiresonance = false;   // passive, but Im eps > 0 or Im mu > 0 (spatial dispersion)
    double signViolation = 0.0;   // max(Im eps, Im mu, 0)
    bool illConditioned = false;
    bool nonlocal = false;
    double cellOverWavelength = 0.0;
};

struct Retrieval {
    std::vector<RetrievedPoint> points;
    std::vector<std::string> warnings;
};

// Frequencies ascending; r and t referenced to the slab's faces.
Result<Retrieval> retrieveSlab(const std::vector<double>& frequenciesHz, const std::vector<Complex>& r, const std::vector<Complex>& t,
                               double thickness, double cellSize);

}  // namespace resonant::em
