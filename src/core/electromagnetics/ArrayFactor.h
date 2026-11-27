#pragma once

// Idealized antenna arrays: element pattern times array factor.
//
//   E(r^) ~ F_element(r^) sum_n w_n exp(j k r^ . r_n)        (far field, e^{+j omega t})
//
// This model assumes identical, isolated elements: it does NOT include
// mutual coupling, changes of element impedance, the feed network,
// currents induced on neighbouring elements or structures, or scattering.
// It is not a full-wave array simulation, and every result says so.
#include "electromagnetics/Conventions.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace resonant::em {

enum class ElementPattern {
    Isotropic,       // 1
    ShortDipole,     // sin(alpha), alpha from the element axis
    HalfWaveDipole,  // cos(pi/2 cos alpha) / sin alpha
    CosineHemisphere // cos(theta) in front of the element (+axis), 0 behind: an approximation of a patch over ground
};
std::string_view toString(ElementPattern pattern);

struct ArrayElement {
    Vec3 position = Vec3::Zero();  // m, array frame
    Vec3 axis = Vec3::UnitZ();     // element axis (dipoles) or boresight (cosine pattern)
    Complex weight = Complex(1.0, 0.0);  // excitation at the frequency of interest (amplitude and phase)
    double delay = 0.0;            // s, a true time delay, applied as exp(-j omega delay)
    bool active = true;
};

struct ArrayModel {
    std::vector<ArrayElement> elements;
    ElementPattern pattern = ElementPattern::Isotropic;
};

Vec3 directionOf(double theta, double phi);  // unit vector from spherical angles
double elementPatternAmplitude(ElementPattern pattern, const Vec3& axis, const Vec3& direction);
Complex arrayFactor(const ArrayModel& array, const Vec3& direction, double frequencyHz);
// |F_element|^2 |AF|^2 (element patterns may differ per element axis, so the product is summed per element).
Complex farFieldResponse(const ArrayModel& array, const Vec3& direction, double frequencyHz);

enum class SteeringMethod { Phase, TimeDelay };
// Weights that point the main beam at `direction`: phases -k r^0 . r_n at
// `frequencyHz` (valid at that frequency only), or true time delays
// (r^0 . r_n - min) / c (valid at every frequency). Amplitudes are kept.
void steer(ArrayModel& array, const Vec3& direction, double frequencyHz, SteeringMethod method);

// Dolph-Chebyshev amplitude weights for N equally spaced elements with all
// sidelobes `sidelobeDb` below the main lobe (broadside, spacing >= lambda/2
// gives equal-level sidelobes), normalized to a largest weight of 1.
std::vector<double> chebyshevWeights(int count, double sidelobeDb);

struct PatternAnalysis {
    Vec3 mainLobe = Vec3::UnitZ();
    double mainTheta = 0.0, mainPhi = 0.0;     // rad
    double directivity = 0.0;                  // linear, of the idealized model
    double directivityDbi = 0.0;
    double beamwidthTheta = 0.0;               // rad, half-power width along theta through the main lobe
    double beamwidthPhi = 0.0;                 // rad, along phi (an angle in the plane of constant theta, measured on the sphere)
    double sidelobeDb = 0.0;                   // largest other local maximum relative to the main lobe (negative); 0 when none
    // Other directions within 1 dB of the main lobe: grating lobes, or the
    // mirror image of a planar array (for grating lobes of linear and
    // rectangular lattices use predictedGratingLobes).
    std::vector<Vec3> equalMaxima;
    double resolutionDeg = 0.0;
};

// Sampled analysis over the sphere (theta step `resolutionDeg`, phi step twice that).
PatternAnalysis analyzePattern(const ArrayModel& array, double frequencyHz, double resolutionDeg = 0.5);

// Grating lobes predicted from the lattice of a uniformly spaced linear
// (spacing along one vector) or rectangular (two vectors) array steered to
// `steering`: directions r^0 + m lambda b1 + n lambda b2 (direction-cosine
// space, reciprocal vectors b) that are visible. Empty when none.
std::vector<Vec3> predictedGratingLobes(const std::vector<Vec3>& latticeVectors, const Vec3& steering, double frequencyHz);

// Weights for directional constraints: |sum_n A_in w_n - b_i| minimal with
// Tikhonov regularization, optionally with |w_n| <= amplitudeLimit
// (iterative clipping). Directions with `null` get b = 0.
struct BeamTarget {
    Vec3 direction;
    Complex desired = Complex(1.0, 0.0);  // relative far-field value (element pattern included)
    double weight = 1.0;
};
struct BeamSynthesis {
    std::vector<Complex> weights;
    std::vector<Complex> achieved;
    double relativeResidual = 0.0;
    double largestWeight = 0.0;
    bool amplitudeLimited = false;
    std::string notes;
};
Result<BeamSynthesis> synthesizeBeams(const ArrayModel& array, const std::vector<BeamTarget>& targets, double frequencyHz,
                                      double regularization = 1e-3, std::optional<double> amplitudeLimit = {});

}  // namespace resonant::em
