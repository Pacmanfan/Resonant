#include "electromagnetics/PeriodicLayers.h"

#include "electromagnetics/Conventions.h"

#include <algorithm>
#include <cmath>

namespace resonant::em {

Status PeriodicCell::validate() const
{
    if (!(thickness1 > 0.0) || !(thickness2 > 0.0) || !std::isfinite(thickness1 + thickness2))
        return Status::failure("both layers of the cell need a positive thickness");
    if (!(permittivity1 >= 1.0) || !(permittivity2 >= 1.0) || !std::isfinite(permittivity1 + permittivity2))
        return Status::failure("the layers' relative permittivities must be at least 1");
    return Status::success();
}

namespace {

double dispersionOf(const PeriodicCell& c, double f)
{
    const double k0 = 2.0 * kPi * f / kSpeedOfLight;
    const double n1 = std::sqrt(c.permittivity1), n2 = std::sqrt(c.permittivity2);
    const double p1 = k0 * n1 * c.thickness1, p2 = k0 * n2 * c.thickness2;
    return std::cos(p1) * std::cos(p2) - 0.5 * (n1 / n2 + n2 / n1) * std::sin(p1) * std::sin(p2);
}

}  // namespace

BlochPoint blochPoint(const PeriodicCell& cell, double f)
{
    BlochPoint p;
    p.frequencyHz = f;
    p.dispersion = dispersionOf(cell, f);
    const double a = cell.period();
    if (std::abs(p.dispersion) <= 1.0) {
        p.wavenumber = Complex(std::acos(p.dispersion) / a, 0.0);
    } else if (p.dispersion > 1.0) {
        p.inGap = true;
        p.wavenumber = Complex(0.0, std::acosh(p.dispersion) / a);
    } else {
        p.inGap = true;
        p.wavenumber = Complex(kPi / a, std::acosh(-p.dispersion) / a);
    }
    return p;
}

std::vector<BandGap> bandGaps(const PeriodicCell& cell, double fmax, int samples)
{
    std::vector<BandGap> gaps;
    if (!cell.validate().ok() || !(fmax > 0.0) || samples < 10)
        return gaps;
    const auto outside = [&](double f) { return std::abs(dispersionOf(cell, f)) > 1.0; };
    const auto edge = [&](double inside, double out) {
        // inside: in a band, out: in the gap.
        for (int i = 0; i < 200 && std::abs(out - inside) > 1e-10 * std::abs(out); ++i) {
            const double mid = 0.5 * (inside + out);
            (outside(mid) ? out : inside) = mid;
        }
        return 0.5 * (inside + out);
    };
    const double df = fmax / samples;
    bool was = false;
    double start = 0.0;
    for (int i = 1; i <= samples; ++i) {
        const double f = i * df;
        const bool now = outside(f);
        if (now && !was)
            start = edge(f - df, f);
        if (!now && was) {
            BandGap g;
            g.lowerHz = start;
            g.upperHz = edge(f, f - df);
            gaps.push_back(g);
        }
        was = now;
    }
    for (BandGap& g : gaps) {
        g.centerHz = 0.5 * (g.lowerHz + g.upperHz);
        g.relativeWidth = (g.upperHz - g.lowerHz) / g.centerHz;
        g.atZoneEdge = dispersionOf(cell, g.centerHz) < -1.0;
        for (int i = 0; i <= 200; ++i) {
            const double f = g.lowerHz + (g.upperHz - g.lowerHz) * i / 200.0;
            g.peakAttenuation = std::max(g.peakAttenuation, blochPoint(cell, f).wavenumber.imag() * cell.period());
        }
    }
    return gaps;
}

EffectiveMedium effectiveMedium(const PeriodicCell& c)
{
    EffectiveMedium e;
    const double a = c.period();
    e.permittivityParallel = (c.permittivity1 * c.thickness1 + c.permittivity2 * c.thickness2) / a;
    e.permittivityPerpendicular = a / (c.thickness1 / c.permittivity1 + c.thickness2 / c.permittivity2);
    // At normal incidence E lies in the plane of the layers: the long-wavelength index is sqrt(eps_parallel).
    e.index = std::sqrt(e.permittivityParallel);
    e.validBelowHz = 0.1 * kSpeedOfLight / (e.index * a);
    e.model = "quasi-static homogenization of a layered medium (Rytov's zeroth order), a / lambda < 0.1";
    return e;
}

Result<double> blochEffectiveIndex(const PeriodicCell& cell, double f)
{
    if (Status v = cell.validate(); !v.ok())
        return Result<double>::failure(v.message());
    if (blochPoint(cell, f).inGap || !bandGaps(cell, f, 2000).empty())
        return Result<double>::failure("the frequency lies in or above the first band gap");
    const BlochPoint p = blochPoint(cell, f);
    return Result<double>(p.wavenumber.real() * kSpeedOfLight / (2.0 * kPi * f));
}

LayeredProblem periodicStack(const PeriodicCell& cell, int periods, double f)
{
    LayeredProblem problem;
    problem.frequencyHz = f;
    problem.incidence.name = "vacuum";
    problem.exit.name = "vacuum";
    for (int i = 0; i < periods; ++i) {
        MediumLayer one, two;
        one.name = "layer 1";
        one.thickness = cell.thickness1;
        one.permittivityReal = cell.permittivity1;
        two.name = "layer 2";
        two.thickness = cell.thickness2;
        two.permittivityReal = cell.permittivity2;
        problem.layers.push_back(one);
        problem.layers.push_back(two);
    }
    return problem;
}

}  // namespace resonant::em
