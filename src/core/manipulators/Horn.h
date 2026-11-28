#pragma once

// Acoustic horns by the Webster horn equation (RES-FUNC-008):
//   d/dx (S dp/dx) + k^2 S p = 0,  i.e.  dp/dx = -j w rho U / S,  dU/dx = -j w S p / (rho c^2)
// for the pressure p and volume velocity U of the plane (one-parameter) wave
// in an axisymmetric horn of area S(x), integrated from the mouth (loaded by
// the radiation impedance of a baffled circular piston, or a given impedance)
// to the throat with fourth-order Runge-Kutta steps. Lossless walls; valid
// while the wave front is close to plane (ka of the local radius below ~1.8,
// below the first cross mode) and the flare is gradual. e^{+jwt}, peak amplitudes.

#include "foundation/Result.h"
#include "foundation/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {
class Geometry;
}

namespace resonant::manipulators {

inline constexpr const char* kHornModelId = "acoustic.horn.webster";
inline constexpr const char* kHornModelVersion = "1";

enum class HornProfile { Conical = 0, Exponential = 1, Hyperbolic = 2, Uniform = 3 };
std::string_view toString(HornProfile profile);

struct HornSpec {
    HornProfile profile = HornProfile::Exponential;
    double throatRadius = 0.0, mouthRadius = 0.0, length = 0.0;  // m
    double flare = 0.7;           // hyperbolic (hypex) T in (0, 1]; T = 1 is exponential
    double density = 1.204;       // kg/m^3
    double soundSpeed = 343.0;    // m/s
    int steps = 400;              // Runge-Kutta steps along the axis
    // Load at the mouth: the baffled piston (default) or a given acoustic impedance (Pa s/m^3).
    std::optional<Complex> mouthImpedance;
    Status validate() const;
};

// The horn of a `horn.acoustic` geometry (air at 20 degC; the caller sets the medium).
HornSpec hornSpecOf(const Geometry& geometry);

// Radius at x in [0, length] from the throat.
double hornRadius(const HornSpec& spec, double x);
// The flare constant m of S = S_t e^{m x} (exponential) or 2 / x0 (hyperbolic); 0 otherwise.
double hornFlareConstant(const HornSpec& spec);
// The low-frequency cut-off m c / (4 pi) of exponential and hyperbolic horns (0 for others).
double hornCutoffHz(const HornSpec& spec);

// Radiation impedance of a circular piston of radius a in an infinite rigid
// baffle (acoustic, Pa s/m^3): (rho c / S) [1 - J1(2ka)/(ka) + j H1(2ka)/(ka)].
Complex bafflePistonImpedance(double radius, double frequencyHz, double density, double soundSpeed);
// The Struve function H1 by quadrature, and the piston's far-field directivity 2 J1(x)/x, x = ka sin(theta).
double struveH1(double x);
double pistonDirectivity(double ka, double sinTheta);
// The directivity factor (ka)^2 / (1 - J1(2ka)/(ka)) of a baffled piston (2 as ka -> 0, the half-space).
double pistonDirectivityFactor(double ka);

struct HornSample {
    double x = 0.0;           // m from the throat
    double radius = 0.0;
    Complex pressure;         // Pa, for the drive
    Complex velocity;         // m/s, particle velocity U / S
    double intensity = 0.0;   // W/m^2, time-averaged 1/2 Re(p u*)
    double power = 0.0;       // W, 1/2 Re(p U*)
};

struct HornResponse {
    double frequencyHz = 0.0;
    Complex throatImpedance;          // Pa s/m^3
    Complex normalizedThroatImpedance;  // Z S_t / (rho c)
    Complex throatReflection;         // against a duct of the throat's area
    double powerTransmission = 0.0;   // 1 - |reflection|^2 for a wave arriving in that duct
    Complex mouthImpedance;           // the load, Pa s/m^3
    double throatVelocity = 0.0;      // m/s, the drive (amplitude of the throat particle velocity)
    double radiatedPower = 0.0;       // W, 1/2 Re(Z_t) |U_t|^2
    double mouthPower = 0.0;          // W, 1/2 Re(p_m U_m*) (equals the radiated power in a lossless horn)
    double onAxisPressureAt1m = 0.0;  // Pa, baffled-piston far field of the mouth's volume velocity
    double directivityFactor = 0.0;   // of the mouth (baffled piston)
    double directivityIndexDb = 0.0;
    double halfAngleMinus6dB = 0.0;   // rad; 0 when the main lobe fills the half-space
    std::vector<HornSample> samples;  // along the axis
};

// The response for a throat particle velocity amplitude (m/s).
Result<HornResponse> analyzeHorn(const HornSpec& spec, double frequencyHz, double throatVelocity = 1.0, int samples = 41);

}  // namespace resonant::manipulators
