#include "electromagnetics/LayeredMedia.h"

#include <cmath>
#include <numbers>

namespace resonant::em {
namespace {

constexpr double kC0 = 299792458.0;
constexpr double kMu0 = 1.25663706212e-6;
constexpr double kEps0 = 1.0 / (kMu0 * kC0 * kC0);
const double kEta0 = std::sqrt(kMu0 / kEps0);

Complex waveNumber(const MediumLayer& m, double omega)
{
    // Principal square root of eps' - j eps'': negative imaginary part, a wave
    // e^{-jkz} that decays along +z.
    return omega / kC0 * std::sqrt(m.relativePermittivity(omega) * m.permeability);
}

Complex impedance(const MediumLayer& m, double omega)
{
    return kEta0 * std::sqrt(Complex(m.permeability, 0.0) / m.relativePermittivity(omega));
}

Status checkLayer(const MediumLayer& m, bool halfSpace)
{
    if (!halfSpace && !(m.thickness > 0.0))
        return Status::failure("the layer '" + m.name + "' needs a positive thickness");
    if (!(m.permittivityReal > 0.0) || !(m.permeability > 0.0))
        return Status::failure("the medium '" + m.name + "' needs a positive relative permittivity and permeability");
    if (m.lossTangent < 0.0 || m.conductivity < 0.0)
        return Status::failure("the medium '" + m.name + "' cannot have a negative loss tangent or conductivity (it would be active)");
    if (halfSpace && (m.lossTangent > 0.0 || m.conductivity > 0.0))
        return Status::failure("the half-space '" + m.name + "' must be lossless (the incident and transmitted powers are defined there)");
    return Status::success();
}

}  // namespace

Complex MediumLayer::relativePermittivity(double omega) const
{
    return {permittivityReal, -lossFactor(omega)};
}

double MediumLayer::lossFactor(double omega) const
{
    return permittivityReal * lossTangent + (omega > 0.0 ? conductivity / (omega * kEps0) : 0.0);
}

Status LayeredProblem::validate() const
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        return Status::failure("the frequency must be positive");
    if (!(incidentIntensity >= 0.0) || !std::isfinite(incidentIntensity))
        return Status::failure("the incident intensity must be finite and not negative");
    if (Status s = checkLayer(incidence, true); !s)
        return s;
    if (Status s = checkLayer(exit, true); !s)
        return s;
    for (const MediumLayer& l : layers)
        if (Status s = checkLayer(l, false); !s)
            return s;
    return Status::success();
}

double LayeredProblem::totalThickness() const
{
    double d = 0.0;
    for (const MediumLayer& l : layers)
        d += l.thickness;
    return d;
}

Result<LayeredSolution> LayeredSolution::solve(const LayeredProblem& problem)
{
    if (Status s = problem.validate(); !s)
        return Result<LayeredSolution>::failure(s.message());
    LayeredSolution out;
    out.problem_ = problem;
    const double omega = 2 * std::numbers::pi * problem.frequencyHz;
    out.omega_ = omega;
    out.etaIn_ = impedance(problem.incidence, omega);
    out.etaOut_ = impedance(problem.exit, omega);
    out.kIn_ = waveNumber(problem.incidence, omega);
    out.kOut_ = waveNumber(problem.exit, omega);
    const size_t n = problem.layers.size();
    out.k_.resize(n);
    out.eta_.resize(n);
    out.starts_.resize(n);
    out.endE_.resize(n);
    out.endH_.resize(n);
    double z = 0.0;
    for (size_t i = 0; i < n; ++i) {
        out.starts_[i] = z;
        z += problem.layers[i].thickness;
        out.k_[i] = waveNumber(problem.layers[i], omega);
        out.eta_[i] = impedance(problem.layers[i], omega);
    }
    // Backwards from the exit interface with a transmitted field of 1: (E, H) = (1, 1/eta_out).
    Complex e = 1.0, h = 1.0 / out.etaOut_;
    const Complex j(0.0, 1.0);
    for (size_t idx = n; idx-- > 0;) {
        out.endE_[idx] = e;
        out.endH_[idx] = h;
        const Complex kd = out.k_[idx] * problem.layers[idx].thickness;
        const Complex c = std::cos(kd), s = std::sin(kd);
        const Complex e0 = e * c + j * out.eta_[idx] * h * s;
        const Complex h0 = j * e * s / out.eta_[idx] + h * c;
        e = e0;
        h = h0;
    }
    if (!std::isfinite(std::abs(e)) || !std::isfinite(std::abs(h)))
        return Result<LayeredSolution>::failure("the layered solution overflowed (a layer is too thick and lossy for this method; split it)");
    // In the incidence half-space: E(0) = E_i (1 + r), eta_in H(0) = E_i (1 - r).
    const double ei = std::sqrt(2.0 * out.etaIn_.real() * problem.incidentIntensity);
    out.incidentAmplitude_ = ei;
    const Complex unit = 2.0 / (e + out.etaIn_ * h);  // t for E_i = 1
    out.t_ = unit;
    out.r_ = unit * e - 1.0;
    // Scale the stored fields to the actual incident amplitude.
    for (size_t i = 0; i < n; ++i) {
        out.endE_[i] *= unit * ei;
        out.endH_[i] *= unit * ei;
    }
    return out;
}

double LayeredSolution::reflectance() const
{
    return std::norm(r_);
}

double LayeredSolution::transmittance() const
{
    return std::norm(t_) * etaIn_.real() * (1.0 / etaOut_).real();
}

double LayeredSolution::absorptance() const
{
    return 1.0 - reflectance() - transmittance();
}

int LayeredSolution::layerAt(double z) const
{
    if (z < 0.0)
        return -1;
    for (size_t i = 0; i < starts_.size(); ++i)
        if (z <= starts_[i] + problem_.layers[i].thickness)
            return static_cast<int>(i);
    return static_cast<int>(starts_.size());
}

Complex LayeredSolution::electricField(double z) const
{
    const Complex j(0.0, 1.0);
    const int i = layerAt(z);
    const Complex ei = incidentAmplitude_;
    if (i < 0)
        return ei * (std::exp(-j * kIn_ * z) + r_ * std::exp(j * kIn_ * z));
    if (i >= static_cast<int>(starts_.size()))
        return ei * t_ * std::exp(-j * kOut_ * (z - problem_.totalThickness()));
    const size_t l = static_cast<size_t>(i);
    const Complex u = k_[l] * (starts_[l] + problem_.layers[l].thickness - z);
    return endE_[l] * std::cos(u) + j * eta_[l] * endH_[l] * std::sin(u);
}

Complex LayeredSolution::magneticField(double z) const
{
    const Complex j(0.0, 1.0);
    const int i = layerAt(z);
    const Complex ei = incidentAmplitude_;
    if (i < 0)
        return ei * (std::exp(-j * kIn_ * z) - r_ * std::exp(j * kIn_ * z)) / etaIn_;
    if (i >= static_cast<int>(starts_.size()))
        return ei * t_ * std::exp(-j * kOut_ * (z - problem_.totalThickness())) / etaOut_;
    const size_t l = static_cast<size_t>(i);
    const Complex u = k_[l] * (starts_[l] + problem_.layers[l].thickness - z);
    return j * endE_[l] * std::sin(u) / eta_[l] + endH_[l] * std::cos(u);
}

double LayeredSolution::poyntingFlux(double z) const
{
    return 0.5 * (electricField(z) * std::conj(magneticField(z))).real();
}

double LayeredSolution::absorbedPowerDensity(double z) const
{
    const int i = layerAt(z);
    if (i < 0 || i >= static_cast<int>(starts_.size()))
        return 0.0;
    const double eps0 = 1.0 / (kMu0 * kC0 * kC0);
    return 0.5 * omega_ * eps0 * problem_.layers[static_cast<size_t>(i)].lossFactor(omega_) * std::norm(electricField(z));
}

}  // namespace resonant::em
