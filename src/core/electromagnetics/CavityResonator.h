#pragma once

// Rectangular cavity resonators with losses (RES-FUNC-006): the closed-form
// TE/TM modes of an ideal box, their fields, stored energies, wall loss from
// the surface resistance of the wall material (perturbation: the ideal mode's
// tangential H on walls of finite conductivity), loss in a dielectric or
// conducting fill, Q, bandwidth and the reflection of a coupled port; and a
// full-wave model (the Yee solver with perfectly conducting walls) that finds
// the resonances numerically. Axes: x along a, y along b, z along d; TE and
// TM with respect to z; e^{+jwt}, peak amplitudes.

#include "electromagnetics/Lines.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::em {

inline constexpr const char* kCavityModelId = "em.cavity.rectangular.analytical";
inline constexpr const char* kCavityYeeModelId = "em.cavity.rectangular.yee";
inline constexpr const char* kCavityModelVersion = "1";

struct CavityResonatorSpec {
    double a = 0.0, b = 0.0, d = 0.0;  // inner dimensions, m
    // The fill.
    double fillPermittivity = 1.0;   // relative, real part
    double fillPermeability = 1.0;   // relative
    double fillLossTangent = 0.0;
    double fillConductivity = 0.0;   // S/m
    // The walls: conductivity <= 0 means perfectly conducting.
    double wallConductivity = 0.0;   // S/m
    double wallPermeability = 1.0;   // relative
    // A port: its external Q (> 0), or none (0).
    double externalQ = 0.0;
    Status validate() const;
};

struct CavityResonance {
    ModeKind kind = ModeKind::TE;
    int m = 0, n = 0, p = 0;
    std::string label;            // "TE101"
    double frequencyHz = 0.0;
    // For a mode normalized to a peak electric field of 1 V/m:
    double electricEnergy = 0.0;  // J, time-averaged
    double magneticEnergy = 0.0;  // J, time-averaged
    double storedEnergy = 0.0;    // J, electric + magnetic
    double wallLoss = 0.0;        // W
    double fillLoss = 0.0;        // W
    double wallQ = std::numeric_limits<double>::infinity();
    double fillQ = std::numeric_limits<double>::infinity();
    double unloadedQ = std::numeric_limits<double>::infinity();
    double externalQ = std::numeric_limits<double>::infinity();
    double loadedQ = std::numeric_limits<double>::infinity();
    double coupling = 0.0;        // beta = Q0 / Qe (0: no port)
    double bandwidthHz = 0.0;     // f0 / QL (0 for an ideal lossless cavity)
};

// The lowest `count` resonances, ascending in frequency.
Result<std::vector<CavityResonance>> analyzeRectangularCavity(const CavityResonatorSpec& spec, int count);

struct CavityModeField {
    std::array<Complex, 3> e{}, h{};  // V/m, A/m at a point, mode normalized to a peak |E| of 1 V/m
};
CavityModeField cavityModeField(const CavityResonatorSpec& spec, const CavityResonance& mode, const Vec3& point);

// The closed-form wall Q of a TE10p mode (Pozar, Microwave Engineering, eq. 6.46),
// for checking the general computation.
double te10pWallQ(const CavityResonatorSpec& spec, int p, double frequencyHz);

// The reflection coefficient of a port coupled to one resonance (a parallel
// resonance seen through an ideal transformer): y = (1 + j Q0 (f/f0 - f0/f)) / beta,
// Gamma = (1 - y) / (1 + y). Requires a port (externalQ > 0).
Complex portReflection(const CavityResonance& mode, double frequencyHz);

// d f / d(dimension) of a mode, analytically; dimension 0 = a, 1 = b, 2 = d.
double resonanceSensitivity(const CavityResonatorSpec& spec, const CavityResonance& mode, int dimension);

// --- full-wave ------------------------------------------------------------------------

struct CavityYeeSettings {
    int cellsAcrossSmallest = 8;     // cells across the smallest dimension
    double durationPeriods = 200.0;  // of the lowest analytical resonance
    double maxFrequencyHz = 0.0;     // 0: 2.5 x the lowest resonance
};

struct CavityYeeResult {
    double cell = 0.0;
    std::array<int, 3> cells{};
    std::array<double, 3> simulatedSize{};    // the box the grid represents (rounded to cells)
    std::vector<double> resonancesHz;         // spectral peaks, ascending
    double energyDecayRate = 0.0;             // 1/s from the stored-energy record (lossy fill)
    double fillConductivityUsed = 0.0;        // S/m: the fill's conductivity plus its loss tangent at the lowest resonance
    long steps = 0;
    double dt = 0.0;
    double seconds = 0.0;
    std::string backend;
    std::vector<std::string> notes;
};

// Runs the Yee solver on a box of PEC walls with the fill, excited by a
// broadband pulse; the loss tangent is represented by an equivalent
// conductivity at the lowest resonance (the Yee solver has no dispersive
// loss). Wall losses are not modeled in this model.
Result<CavityYeeResult> simulateRectangularCavity(const CavityResonatorSpec& spec, const CavityYeeSettings& settings,
                                                  JobContext* context = nullptr);

}  // namespace resonant::em
