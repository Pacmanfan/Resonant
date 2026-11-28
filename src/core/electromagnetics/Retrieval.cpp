#include "electromagnetics/Retrieval.h"

#include "electromagnetics/Conventions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::em {

Result<Retrieval> retrieveSlab(const std::vector<double>& f, const std::vector<Complex>& r, const std::vector<Complex>& t, double d, double cell)
{
    using R = Result<Retrieval>;
    if (f.empty() || f.size() != r.size() || f.size() != t.size())
        return R::failure("as many reflection and transmission values as frequencies");
    if (!(d > 0.0) || !(cell > 0.0))
        return R::failure("the slab thickness and cell size must be positive");
    for (size_t i = 1; i < f.size(); ++i)
        if (!(f[i] > f[i - 1]))
            return R::failure("frequencies must ascend");
    Retrieval out;
    double unwrapped = 0.0, previous = 0.0;
    int flagged = 0, antiresonant = 0;
    double worstSign = 0.0;
    for (size_t i = 0; i < f.size(); ++i) {
        RetrievedPoint p;
        p.frequencyHz = f[i];
        const double k0 = 2 * kPi * f[i] / kSpeedOfLight;
        const Complex num = (1.0 + r[i]) * (1.0 + r[i]) - t[i] * t[i], den = (1.0 - r[i]) * (1.0 - r[i]) - t[i] * t[i];
        p.illConditioned = std::abs(den) < 1e-9 || std::abs(num) < 1e-9;
        Complex z = std::sqrt(num / den);
        if (z.real() < 0.0)
            z = -z;
        p.impedance = z;
        const Complex P = t[i] / (1.0 - r[i] * (z - 1.0) / (z + 1.0));
        // -arg P, unwrapped along frequency.
        const double phase = -std::arg(P);
        if (i == 0) {
            unwrapped = phase;
        } else {
            double step = phase - previous;
            while (step > kPi)
                step -= 2 * kPi;
            while (step < -kPi)
                step += 2 * kPi;
            unwrapped += step;
        }
        previous = phase;
        p.index = Complex(unwrapped / (k0 * d), std::log(std::abs(P)) / (k0 * d));
        p.permittivity = p.index / z;
        p.permeability = p.index * z;
        const double tol = 1e-12 * std::abs(p.index);
        p.passive = p.index.imag() <= tol && z.real() >= 0.0;
        p.signViolation = std::max({0.0, p.permittivity.imag(), p.permeability.imag()});
        p.antiresonance = p.passive && p.signViolation > 1e-12 * (std::abs(p.permittivity) + std::abs(p.permeability));
        p.cellOverWavelength = cell * std::abs(p.index.real()) * f[i] / kSpeedOfLight;
        p.nonlocal = p.cellOverWavelength > 0.1;
        if (!p.passive || p.illConditioned || p.nonlocal)
            ++flagged;
        if (p.antiresonance) {
            ++antiresonant;
            worstSign = std::max(worstSign, p.signViolation);
        }
        out.points.push_back(p);
    }
    if (std::abs(out.points.front().index.real()) * 2 * kPi * f.front() / kSpeedOfLight * d > kPi)
        out.warnings.push_back("the lowest frequency's phase through the slab exceeds pi: the branch of the index is uncertain");
    if (flagged > 0)
        out.warnings.push_back(std::to_string(flagged) + " point(s) are non-passive, ill-conditioned or non-local; their parameters are not "
                                                         "local material parameters");
    if (antiresonant > 0) {
        char text[64];
        std::snprintf(text, sizeof text, "%.2g", worstSign);
        out.warnings.push_back(std::to_string(antiresonant) + " passive point(s) have Im eps or Im mu > 0 (at most " + text +
                               "): the antiresonance of a periodic cell, a sign of spatial dispersion");
    }
    return out;
}

}  // namespace resonant::em
