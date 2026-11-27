#pragma once

// Analytical antenna models.
//
//   Hertzian dipole     infinitesimal current element I l: exact fields at
//                       every distance.
//   Thin-wire dipole    centre-fed, total length L, radius a << L, with the
//                       assumed sinusoidal current I(z) = Im sin(k (h - |z|)),
//                       h = L / 2. Its fields are the exact fields of that
//                       current (Schelkunoff); its input impedance is the
//                       induced-EMF value referred to the feed. Accurate for
//                       thin wires up to about one wavelength; the assumed
//                       current is not the true current.
//   Monopole            a quarter-wave (or any length) monopole over an
//                       infinite perfectly conducting ground plane: by image
//                       theory the field above the plane is that of a dipole
//                       of twice the length, the input impedance half of it.
//   Small loop          a loop of circumference << wavelength with uniform
//                       current: a magnetic dipole.
//
// Everything is in the antenna's local frame, axis along +Z, feed at the
// origin, time convention e^{+j omega t}.
#include "electromagnetics/Conventions.h"
#include "foundation/Types.h"

#include <Eigen/Core>

#include <functional>

#include <string>

namespace resonant::em {

using Vec3c = Eigen::Vector3cd;

struct FieldPoint {
    Vec3c e = Vec3c::Zero();  // V/m
    Vec3c h = Vec3c::Zero();  // A/m
    bool singular = false;    // on the wire or the current element: not evaluated
};

// Sine and cosine integrals Si(x), Ci(x) for x > 0 (series and continued fraction).
double sineIntegral(double x);
double cosineIntegral(double x);

FieldPoint hertzianDipoleField(const Vec3& point, Complex currentMoment, double frequencyHz);
FieldPoint smallLoopField(const Vec3& point, Complex magneticMoment, double frequencyHz);  // moment = I x area (A m^2)
FieldPoint thinDipoleField(const Vec3& point, double length, Complex feedCurrent, double frequencyHz);

// Far-field pattern of the thin dipole, |E_theta| r / (eta Im / 2 pi) = (cos(k h cos t) - cos(k h)) / sin t.
double thinDipolePattern(double theta, double length, double frequencyHz);

struct AntennaImpedance {
    Complex terminal;  // referred to the current maximum
    Complex input;     // at the feed
    bool valid = true; // false near antiresonance (sin(kh) -> 0), where the current at the feed vanishes in this model
};

// Induced-EMF impedance of the thin dipole (Balanis eq. 8-60 a/b).
AntennaImpedance thinDipoleImpedance(double length, double radius, double frequencyHz);
// Monopole over an infinite ground plane: half the dipole of twice the length.
AntennaImpedance monopoleImpedance(double height, double radius, double frequencyHz);
// Small loop: radiation resistance 20 pi^2 (C / lambda)^4 and the inductive
// reactance of a thin circular loop omega mu0 a (ln(8a/b) - 2).
AntennaImpedance smallLoopImpedance(double loopRadius, double wireRadius, double frequencyHz);

// The length of a thin dipole whose input reactance is zero near half a
// wavelength (first resonance), from the induced-EMF model.
double resonantDipoleLength(double radius, double frequencyHz);

// Directivity of a pattern U(theta) (rotationally symmetric about Z) by
// numerical integration, max U / mean U.
double directivityOfAxisymmetric(const std::function<double(double)>& intensity, bool upperHalfOnly = false);

// Matching quantities against a reference impedance.
struct MatchQuantities {
    Complex reflection;
    double returnLossDb = 0.0;  // -20 log10 |Gamma|, positive
    double vswr = 0.0;          // infinite for |Gamma| = 1
    double mismatchEfficiency = 0.0;  // 1 - |Gamma|^2
};
MatchQuantities match(const Complex& impedance, double referenceImpedance);

}  // namespace resonant::em
