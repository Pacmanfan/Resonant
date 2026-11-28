#include "mechanics/LumpedResonators.h"

#include <cmath>
#include <limits>

namespace resonant::mechanics {

namespace {
constexpr double kPi = 3.14159265358979323846;

// The clamped-free mode shape phi(x) = cosh - cos - sigma (sinh - sin) of lambda x; returns phi(xi L).
double shape(double lambdaL, double xi)
{
    const double sigma = (std::cosh(lambdaL) + std::cos(lambdaL)) / (std::sinh(lambdaL) + std::sin(lambdaL));
    const double u = lambdaL * xi;
    return std::cosh(u) - std::cos(u) - sigma * (std::sinh(u) - std::sin(u));
}
}  // namespace

Status CantileverSpec::validate() const
{
    if (!(length > 0.0) || !(width > 0.0) || !(thickness > 0.0))
        return Status::failure("the cantilever needs a positive length, width and thickness");
    if (!(youngsModulus > 0.0) || !(density > 0.0))
        return Status::failure("the cantilever's material needs a Young's modulus and a density");
    if (lossFactor < 0.0 || lossFactor >= 1.0)
        return Status::failure("the loss factor must lie in [0, 1)");
    return Status::success();
}

Result<CantileverMode> cantileverMode(const CantileverSpec& s, int mode)
{
    using R = Result<CantileverMode>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    static const double roots[] = {1.87510406871196, 4.69409113297417, 7.85475743823761, 10.9955407348755, 14.1371683910465};
    if (mode < 1 || mode > 5)
        return R::failure("modes 1 to 5");
    const double lambdaL = roots[mode - 1];
    const double area = s.width * s.thickness, inertia = s.width * std::pow(s.thickness, 3) / 12.0;
    const double massPerLength = s.density * area;
    CantileverMode m;
    m.mode = mode;
    const double w = lambdaL * lambdaL * std::sqrt(s.youngsModulus * inertia / (massPerLength * std::pow(s.length, 4)));
    m.frequencyHz = w / (2 * kPi);
    // Modal mass for phi(L) = 1: integral of rho A phi^2 by Simpson's rule (exactly 0.25 m for every mode).
    const int n = 2000;
    double integral = 0.0;
    const double tip = shape(lambdaL, 1.0);
    for (int i = 0; i <= n; ++i) {
        const double v = shape(lambdaL, static_cast<double>(i) / n) / tip;
        integral += (i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0)) * v * v;
    }
    integral *= 1.0 / (3.0 * n);
    m.tipMass = massPerLength * s.length * integral;
    m.tipStiffness = w * w * m.tipMass;
    m.quality = s.lossFactor > 0.0 ? 1.0 / s.lossFactor : std::numeric_limits<double>::infinity();
    m.kineticEnergyPerTip2 = 0.5 * m.tipMass * w * w;
    return m;
}

Result<oscillators::Network> coupledCantilevers(const CantileverSpec& a, const CantileverSpec& b, double kc)
{
    using R = Result<oscillators::Network>;
    if (!(kc >= 0.0) || !std::isfinite(kc))
        return R::failure("the coupling stiffness must be non-negative");
    auto ma = cantileverMode(a), mb = cantileverMode(b);
    if (!ma || !mb)
        return R::failure(ma ? mb.message() : ma.message());
    oscillators::Network n;
    n.names = {"cantilever 1", "cantilever 2"};
    n.masses = {ma.value().tipMass, mb.value().tipMass};
    n.springs = {{"cantilever 1 bending", 0, -1, ma.value().tipStiffness}, {"cantilever 2 bending", 1, -1, mb.value().tipStiffness},
                 {"tip coupling spring", 0, 1, kc}};
    const auto damper = [](const CantileverSpec& s, const CantileverMode& m, int i) {
        return oscillators::Damper{"structural (cantilever " + std::to_string(i + 1) + ")", i, -1,
                                   s.lossFactor * m.tipStiffness / (2 * kPi * m.frequencyHz)};
    };
    if (a.lossFactor > 0.0)
        n.dampers.push_back(damper(a, ma.value(), 0));
    if (b.lossFactor > 0.0)
        n.dampers.push_back(damper(b, mb.value(), 1));
    n.unitsNote = "tip displacement (m), tip-referred modal mass (kg) and stiffness (N/m)";
    return n;
}

}  // namespace resonant::mechanics
