#include "electromagnetics/SplitRing.h"

#include "electromagnetics/Conventions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::em {

Status SplitRingSpec::validate() const
{
    if (!(radius > 0.0) || !(width > 0.0) || !(gap > 0.0) || !(metalThickness > 0.0) || !(latticeSpacing > 0.0))
        return Status::failure("the split ring needs a positive radius, width, gap, metal thickness and lattice spacing");
    if (!(radius + 0.5 * width < 0.5 * latticeSpacing))
        return Status::failure("the ring must fit inside its lattice cell");
    if (!(gap < 2.0 * radius))
        return Status::failure("the gap must be smaller than the ring");
    if (!(substratePermittivity >= 1.0) || !(conductivity > 0.0))
        return Status::failure("the substrate permittivity must be at least 1 and the conductivity positive");
    if (!(substrateLossTangent >= 0.0) || substrateLossTangent >= 1.0)
        return Status::failure("the substrate loss tangent must lie in [0, 1)");
    return Status::success();
}

Result<SplitRingModel> splitRingModel(const SplitRingSpec& s)
{
    using R = Result<SplitRingModel>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    SplitRingModel m;
    const double rw = 0.25 * (s.width + s.metalThickness);
    m.inductance = kMu0 * s.radius * (std::log(8.0 * s.radius / rw) - 2.0);
    if (!(m.inductance > 0.0))
        return R::failure("the strip is too wide for the loop inductance formula (8 r / r_w must exceed e^2)");
    const double epsEff = 0.5 * (1.0 + s.substratePermittivity);
    m.capacitance = kEpsilon0 * epsEff * (s.width * s.metalThickness / s.gap + (s.width + s.metalThickness));
    m.resonanceHz = 1.0 / (2 * kPi * std::sqrt(m.inductance * m.capacitance));
    const double w0 = 2 * kPi * m.resonanceHz;
    const double skin = std::sqrt(2.0 / (w0 * kMu0 * s.conductivity));
    m.resistance = 2 * kPi * s.radius / (s.conductivity * skin * 2.0 * (s.width + s.metalThickness));
    m.qConductor = w0 * m.inductance / m.resistance;
    const double dielectricShare = s.substratePermittivity / (1.0 + s.substratePermittivity);
    m.qDielectric = s.substrateLossTangent > 0.0 ? 1.0 / (s.substrateLossTangent * dielectricShare) : std::numeric_limits<double>::infinity();
    m.quality = 1.0 / (1.0 / m.qConductor + 1.0 / m.qDielectric);
    m.dampingRate = w0 / m.quality;
    m.fillingFactor = kPi * s.radius * s.radius / (s.latticeSpacing * s.latticeSpacing);
    m.magneticPlasmaHz = m.resonanceHz / std::sqrt(1.0 - m.fillingFactor);
    m.cellOverWavelength = s.latticeSpacing * m.resonanceHz / kSpeedOfLight;
    return m;
}

Complex splitRingPermeability(const SplitRingModel& m, double f)
{
    const double w = 2 * kPi * f, w0 = 2 * kPi * m.resonanceHz;
    return 1.0 - m.fillingFactor * w * w / (w * w - w0 * w0 - Complex(0.0, w * m.dampingRate));
}

double splitRingKramersKronigResidual(const SplitRingModel& m, const std::vector<double>& frequencies)
{
    const double w0 = 2 * kPi * m.resonanceHz, gamma = m.dampingRate, F = m.fillingFactor;
    // g(w) = w chi''(w) = w * F w^3 Gamma / |w^2 - w0^2 - j w Gamma|^2, which tends to F Gamma.
    const auto g = [&](double w) {
        const double re = w * w - w0 * w0, im = w * gamma;
        return F * w * w * w * w * gamma / (re * re + im * im);
    };
    const double top = 40.0 * w0;
    const int n = 400000;
    const double h = top / n;
    double worst = 0.0;
    for (double f : frequencies) {
        const double w = 2 * kPi * f;
        const double gw = g(w);
        // PV int_0^inf dw'/(w'^2 - w^2) = 0, so subtract g(w): the integrand is regular.
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double x = (i + 0.5) * h;
            const double d = x * x - w * w;
            if (std::abs(x - w) < 1e-9 * w)
                continue;
            sum += (g(x) - gw) / d;
        }
        sum *= h;
        // The tail beyond `top`, where g is F Gamma to O(w0^2 / w^2).
        sum += (F * gamma - gw) * std::log((top + w) / (top - w)) / (2.0 * w);
        const double reKK = (1.0 - F) + 2.0 / kPi * sum;
        worst = std::max(worst, std::abs(reKK - splitRingPermeability(m, f).real()));
    }
    return worst;
}

}  // namespace resonant::em
