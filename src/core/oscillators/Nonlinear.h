#pragma once

// Nonlinear and parametric resonance of single lumped oscillators (RES-RSN-010).
// Lumped models with stated assumptions; nothing here is inferred from a
// linear sweep.
//
//  Duffing:  m x'' + c x' + k x + k3 x^3 = F cos(w t)
//     backbone (free, harmonic balance): w_b(A) = sqrt((k + 3/4 k3 A^2) / m)
//     forced response: [(k - m w^2 + 3/4 k3 A^2)^2 + (c w)^2] A^2 = F^2, up to three
//     amplitudes (bistability, hysteresis)
//  Mathieu (parametric): x'' + 2 zeta w0 x' + w0^2 (1 + h cos(W t)) x = 0
//     stability from the Floquet multipliers of one period of W

#include "foundation/Result.h"

#include <vector>

namespace resonant::oscillators {

struct DuffingOscillator {
    double mass = 1.0, damping = 0.0, stiffness = 1.0, cubic = 0.0;
    Status validate() const;
};

// Angular frequency of free oscillation of amplitude A (first harmonic balance).
double duffingBackbone(const DuffingOscillator& d, double amplitude);

struct DuffingBranch {
    double amplitude = 0.0;
    bool stable = true;  // the middle branch of three is unstable
};
// Steady first-harmonic amplitudes at a drive angular frequency (one or three).
std::vector<DuffingBranch> duffingResponse(const DuffingOscillator& d, double force, double omega);

// Free vibration from x(0) = A, x'(0) = 0 by fourth-order Runge-Kutta; the mean
// angular frequency from the zero crossings over `periods` periods.
Result<double> duffingMeasuredFrequency(const DuffingOscillator& d, double amplitude, int periods = 40);

// Forced steady amplitude by time integration from a given initial amplitude
// (to show hysteresis: start low or high), after `settle` drive periods.
Result<double> duffingForcedAmplitude(const DuffingOscillator& d, double force, double omega, double initialAmplitude, int settle = 400);

struct MathieuOscillator {
    double naturalOmega = 1.0;  // w0
    double dampingRatio = 0.0;  // zeta
    double modulation = 0.0;    // h
    Status validate() const;
};
// The largest Floquet multiplier magnitude at a pump angular frequency W (> 1: unstable; the
// stable side gives 1 to within the integrator's rounding).
double mathieuMultiplier(const MathieuOscillator& m, double pumpOmega);
// The edges of the instability tongue around W = 2 w0 by bisection; empty when stable throughout.
std::vector<double> mathieuTongue(const MathieuOscillator& m);

}  // namespace resonant::oscillators
