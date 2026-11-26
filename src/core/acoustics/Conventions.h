// Conventions of the acoustic module. Everything in src/core/acoustics and
// every result it produces follows them.
//
// TIME CONVENTION
//   A harmonic quantity is the real part of a complex amplitude times
//   exp(+j omega t):   p(x, t) = Re{ P(x) exp(j omega t) },  omega = 2 pi f.
//   An outgoing spherical wave is therefore exp(-j k r) / r, k = omega / c.
//   With this convention a positive phase LEADS and a time delay tau
//   multiplies the amplitude by exp(-j omega tau).
//
// AMPLITUDES
//   Complex amplitudes are PEAK amplitudes. For a harmonic signal
//   p_rms = |P| / sqrt(2).
//
// SOURCE STRENGTH
//   A point source is described by its free-field pressure amplitude at the
//   reference distance r_ref = 1 m on its axis: "amplitude" A, in pascal.
//   Its field is  P(r) = A * r_ref * D(theta) * exp(-j k r) / r.
//   The strength belongs to each source. Adding sources adds their fields;
//   nothing is renormalized by the number of sources.
//
// LEVELS
//   SPL = 20 log10(p_rms / p_ref). p_ref is 20 micropascal in air and
//   1 micropascal in water by convention; the reference used is always
//   stated with the value.
//
// EMITTER AXIS
//   An emitter radiates along its local +Z axis. Directivity angles theta
//   are measured from that axis.
#pragma once

namespace resonant::acoustics {

inline constexpr double kReferenceDistance = 1.0;          // m
inline constexpr double kReferencePressureAir = 20e-6;     // Pa (rms)
inline constexpr double kReferencePressureWater = 1e-6;    // Pa (rms)
inline constexpr const char* kTimeConvention = "exp(+j omega t); outgoing waves exp(-j k r)";

}  // namespace resonant::acoustics
