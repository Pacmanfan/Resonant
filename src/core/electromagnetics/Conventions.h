#pragma once

// Conventions of the electromagnetic domain, used by every model in it.
//
//   Time dependence   e^{+j omega t}: a phasor X means x(t) = Re{X e^{j omega t}}.
//                     Outgoing waves vary as e^{-jkr}; a lossy medium has
//                     epsilon_c = epsilon - j sigma / omega (negative imaginary part).
//   Amplitudes        peak values (time-averaged power has the factor 1/2).
//   Units             SI: E in V/m, H in A/m, current in A, power in W,
//                     impedance in ohm, angles in rad internally.
//   Coordinates       spherical angles theta from +Z, phi from +X towards +Y,
//                     in the antenna's or array's local frame.
#include <complex>

namespace resonant::em {

using Complex = std::complex<double>;

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kSpeedOfLight = 299792458.0;           // m/s, exact
inline constexpr double kMu0 = 1.25663706212e-6;               // H/m (CODATA 2018)
inline constexpr double kEpsilon0 = 8.8541878128e-12;          // F/m (CODATA 2018)
inline constexpr double kEta0 = 376.730313668;                 // ohm, sqrt(mu0 / epsilon0)

inline double wavenumber(double frequencyHz) { return 2.0 * kPi * frequencyHz / kSpeedOfLight; }
inline double wavelength(double frequencyHz) { return kSpeedOfLight / frequencyHz; }

}  // namespace resonant::em
