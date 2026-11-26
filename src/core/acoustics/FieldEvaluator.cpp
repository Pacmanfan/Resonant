#include "acoustics/FieldEvaluator.h"

#include "foundation/Units.h"

#include <cmath>

namespace resonant::acoustics {

FieldEvaluator::FieldEvaluator(const std::vector<PointSource>& sources, double frequencyHz, Medium medium,
                               std::shared_ptr<const PropagationModel> model)
    : frequencyHz_(frequencyHz), medium_(std::move(medium)), model_(std::move(model))
{
    for (const PointSource& source : sources) {
        if (std::abs(source.frequencyHz - frequencyHz) <= 1e-9 * frequencyHz) {
            sources_.push_back(source);
            if (source.directivity.model != DirectivityModel::Omnidirectional)
                velocityExact_ = false;
        } else {
            ++excluded_;
        }
    }
}

FieldSample FieldEvaluator::sample(const Vec3& point) const
{
    FieldSample result;
    for (const PointSource& source : sources_) {
        const Transfer transfer = model_->transfer(source, point, medium_);
        result.pressure += source.amplitude * transfer.value;
        result.nearSource = result.nearSource || transfer.nearSource;
    }
    return result;
}

Eigen::Vector3cd FieldEvaluator::particleVelocity(const Vec3& point) const
{
    Eigen::Vector3cd gradient = Eigen::Vector3cd::Zero();
    for (const PointSource& source : sources_)
        gradient += source.amplitude * model_->transfer(source, point, medium_, true).gradient;
    // Linearized momentum equation, j omega rho V = -grad P.
    const Complex factor = -1.0 / (Complex(0.0, 2.0 * units::pi * frequencyHz_) * medium_.density);
    return factor * gradient;
}

Vec3 FieldEvaluator::activeIntensity(const Vec3& point) const
{
    const Complex p = pressure(point);
    const Eigen::Vector3cd v = particleVelocity(point);
    return 0.5 * (p * v.conjugate()).real();
}

double rmsPressure(const Complex& pressure)
{
    return std::abs(pressure) / std::sqrt(2.0);
}

double soundPressureLevel(const Complex& pressure, double referencePressure)
{
    return 20.0 * std::log10(std::max(rmsPressure(pressure), 1e-300) / referencePressure);
}

double planeWaveIntensity(const Complex& pressure, const Medium& medium)
{
    return std::norm(pressure) / (2.0 * medium.impedance());
}

double instantaneousPressure(const Complex& pressure, double phase)
{
    return (pressure * std::polar(1.0, phase)).real();
}

}  // namespace resonant::acoustics
