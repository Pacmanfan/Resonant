// Evaluates acoustic field quantities at arbitrary world points for a set
// of sources at ONE frequency, with a propagation model and a medium.
//
// Quantities and their units:
//   pressure            complex peak amplitude P, Pa
//   magnitude           |P|, Pa (peak)
//   phase               arg P, rad, in (-pi, pi]
//   rms                 |P| / sqrt(2), Pa
//   SPL                 20 log10(rms / p_ref), dB re p_ref
//   particle velocity   V = -grad P / (j omega rho), m/s (complex peak)
//   active intensity    I = 1/2 Re{P conj(V)}, W/m^2 (time-averaged vector)
//   plane-wave intensity |P|^2 / (2 rho c), W/m^2 - an APPROXIMATION valid
//                       for a single progressive wave far from sources; it
//                       is wrong in standing-wave and reactive near fields.
#pragma once

#include "acoustics/Propagation.h"

namespace resonant::acoustics {

struct FieldSample {
    Complex pressure = Complex(0.0, 0.0);
    bool nearSource = false;  // within the near-source limit of at least one source
};

class FieldEvaluator {
public:
    // Uses the sources whose frequency equals `frequencyHz` (within one part
    // in 10^9). Sources at other frequencies cannot be superposed in one
    // frequency-domain solution; they are left out and counted.
    FieldEvaluator(const std::vector<PointSource>& sources, double frequencyHz, Medium medium,
                   std::shared_ptr<const PropagationModel> model);

    double frequencyHz() const { return frequencyHz_; }
    const Medium& medium() const { return medium_; }
    const PropagationModel& model() const { return *model_; }
    const std::vector<PointSource>& sources() const { return sources_; }
    size_t excludedSourceCount() const { return excluded_; }
    // True when every source is omnidirectional, so the velocity and
    // intensity formulas are exact for this model.
    bool velocityIsExact() const { return velocityExact_; }

    FieldSample sample(const Vec3& point) const;
    Complex pressure(const Vec3& point) const { return sample(point).pressure; }
    Eigen::Vector3cd particleVelocity(const Vec3& point) const;
    Vec3 activeIntensity(const Vec3& point) const;

private:
    std::vector<PointSource> sources_;
    double frequencyHz_;
    Medium medium_;
    std::shared_ptr<const PropagationModel> model_;
    size_t excluded_ = 0;
    bool velocityExact_ = true;
};

double rmsPressure(const Complex& pressure);
double soundPressureLevel(const Complex& pressure, double referencePressure);
double planeWaveIntensity(const Complex& pressure, const Medium& medium);
// Re{P exp(j * phase)}: the pressure at the instant when omega t = phase.
double instantaneousPressure(const Complex& pressure, double phase);

}  // namespace resonant::acoustics
