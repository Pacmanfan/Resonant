#include "manipulators/Horn.h"

#include "geometry/Geometry.h"

#include <algorithm>
#include <cmath>

namespace resonant::manipulators {

namespace {

constexpr double kPi = 3.14159265358979323846;

// x0 of the hyperbolic horn: cosh(L/x0) + T sinh(L/x0) = r_m / r_t.
double hyperbolicLength(const HornSpec& s)
{
    const double ratio = s.mouthRadius / s.throatRadius;
    const auto f = [&](double u) { return std::cosh(u) + s.flare * std::sinh(u) - ratio; };  // u = L / x0
    double lo = 0.0, hi = 1.0;
    while (f(hi) < 0.0 && hi < 700.0)
        hi *= 2.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (f(mid) < 0.0 ? lo : hi) = mid;
    }
    return s.length / (0.5 * (lo + hi));
}

}  // namespace

std::string_view toString(HornProfile p)
{
    switch (p) {
    case HornProfile::Conical: return "conical";
    case HornProfile::Exponential: return "exponential";
    case HornProfile::Hyperbolic: return "hyperbolic";
    case HornProfile::Uniform: return "uniform";
    }
    return "conical";
}

Status HornSpec::validate() const
{
    if (!(throatRadius > 0.0) || !(mouthRadius > 0.0) || !(length > 0.0) || !std::isfinite(throatRadius + mouthRadius + length))
        return Status::failure("the horn's throat radius, mouth radius and length must be positive");
    if (profile != HornProfile::Uniform && !(mouthRadius > throatRadius))
        return Status::failure("the mouth must be larger than the throat (use the uniform profile for a straight duct)");
    if (profile == HornProfile::Hyperbolic && !(flare > 0.0 && flare <= 1.0))
        return Status::failure("the hyperbolic flare parameter T must lie in (0, 1]");
    if (!(density > 0.0) || !(soundSpeed > 0.0))
        return Status::failure("the medium needs a positive density and sound speed");
    if (steps < 10 || steps > 200000)
        return Status::failure("between 10 and 200 000 integration steps");
    return Status::success();
}

HornSpec hornSpecOf(const Geometry& g)
{
    HornSpec s;
    s.profile = static_cast<HornProfile>(std::clamp<long>(std::lround(g.parameter("profile")), 0, 3));
    s.throatRadius = g.parameter("throatRadius");
    s.mouthRadius = g.parameter("mouthRadius");
    s.length = g.parameter("length");
    s.flare = g.parameter("flare");
    return s;
}

double hornRadius(const HornSpec& s, double x)
{
    const double t = std::clamp(x / s.length, 0.0, 1.0);
    switch (s.profile) {
    case HornProfile::Conical: return s.throatRadius + (s.mouthRadius - s.throatRadius) * t;
    case HornProfile::Exponential: return s.throatRadius * std::pow(s.mouthRadius / s.throatRadius, t);
    case HornProfile::Hyperbolic: {
        const double x0 = hyperbolicLength(s), u = t * s.length / x0;
        return s.throatRadius * (std::cosh(u) + s.flare * std::sinh(u));
    }
    case HornProfile::Uniform: return s.throatRadius;
    }
    return s.throatRadius;
}

double hornFlareConstant(const HornSpec& s)
{
    if (s.profile == HornProfile::Exponential)
        return 2.0 * std::log(s.mouthRadius / s.throatRadius) / s.length;
    if (s.profile == HornProfile::Hyperbolic)
        return 2.0 / hyperbolicLength(s);
    return 0.0;
}

double hornCutoffHz(const HornSpec& s)
{
    return hornFlareConstant(s) * s.soundSpeed / (4.0 * kPi);
}

double struveH1(double x)
{
    // H1(x) = (2x/pi) int_0^{pi/2} cos^2(phi) sin(x sin(phi)) dphi, by composite Simpson.
    if (x == 0.0)
        return 0.0;
    const int n = 2 * (200 + static_cast<int>(16.0 * std::abs(x)));
    const double h = 0.5 * kPi / n;
    double sum = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double phi = i * h;
        const double c = std::cos(phi);
        const double v = c * c * std::sin(x * std::sin(phi));
        sum += (i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0)) * v;
    }
    return 2.0 * x / kPi * sum * h / 3.0;
}

Complex bafflePistonImpedance(double radius, double f, double rho, double c)
{
    const double k = 2.0 * kPi * f / c, ka = k * radius, area = kPi * radius * radius;
    if (ka < 1e-6)
        return (rho * c / area) * Complex(0.5 * ka * ka, 8.0 * ka / (3.0 * kPi));
    const double x = 2.0 * ka;
    return (rho * c / area) * Complex(1.0 - std::cyl_bessel_j(1.0, x) / ka, struveH1(x) / ka);
}

double pistonDirectivity(double ka, double sinTheta)
{
    const double x = ka * sinTheta;
    if (std::abs(x) < 1e-8)
        return 1.0;
    return 2.0 * std::cyl_bessel_j(1.0, x) / x;
}

double pistonDirectivityFactor(double ka)
{
    if (ka < 1e-4)
        return 2.0;
    return ka * ka / (1.0 - std::cyl_bessel_j(1.0, 2.0 * ka) / ka);
}

Result<HornResponse> analyzeHorn(const HornSpec& s, double f, double throatVelocity, int sampleCount)
{
    using R = Result<HornResponse>;
    if (Status v = s.validate(); !v.ok())
        return R::failure(v.message());
    if (!(f > 0.0) || !std::isfinite(f))
        return R::failure("the frequency must be positive");
    if (!(throatVelocity > 0.0))
        return R::failure("the throat velocity must be positive");
    sampleCount = std::clamp(sampleCount, 2, 2001);
    const double omega = 2.0 * kPi * f, rho = s.density, c = s.soundSpeed;
    const Complex j(0.0, 1.0);
    const auto area = [&](double x) {
        const double r = hornRadius(s, x);
        return kPi * r * r;
    };
    HornResponse out;
    out.frequencyHz = f;
    out.mouthImpedance = s.mouthImpedance ? *s.mouthImpedance : bafflePistonImpedance(s.mouthRadius, f, rho, c);
    // Integrate from the mouth (x = L) to the throat with U_m = 1, p_m = Z_m U_m; keep the states at the samples.
    const auto derivative = [&](double x, Complex p, Complex u, Complex& dp, Complex& du) {
        const double S = area(x);
        dp = -j * omega * rho * u / S;
        du = -j * omega * S * p / (rho * c * c);
    };
    const int n = s.steps;
    const double h = -s.length / n;
    std::vector<std::pair<Complex, Complex>> state(static_cast<size_t>(n) + 1);
    Complex p = out.mouthImpedance, u(1.0, 0.0);
    state[static_cast<size_t>(n)] = {p, u};
    for (int i = n; i > 0; --i) {
        const double x = s.length * i / n;
        Complex k1p, k1u, k2p, k2u, k3p, k3u, k4p, k4u;
        derivative(x, p, u, k1p, k1u);
        derivative(x + 0.5 * h, p + 0.5 * h * k1p, u + 0.5 * h * k1u, k2p, k2u);
        derivative(x + 0.5 * h, p + 0.5 * h * k2p, u + 0.5 * h * k2u, k3p, k3u);
        derivative(x + h, p + h * k3p, u + h * k3u, k4p, k4u);
        p += h / 6.0 * (k1p + 2.0 * k2p + 2.0 * k3p + k4p);
        u += h / 6.0 * (k1u + 2.0 * k2u + 2.0 * k3u + k4u);
        state[static_cast<size_t>(i - 1)] = {p, u};
    }
    if (!std::isfinite(std::abs(p)) || !std::isfinite(std::abs(u)) || std::abs(u) == 0.0)
        return R::failure("the Webster integration did not give a finite throat state");
    const double St = area(0.0), zc = rho * c / St;
    out.throatImpedance = p / u;
    out.normalizedThroatImpedance = out.throatImpedance / zc;
    out.throatReflection = (out.throatImpedance - zc) / (out.throatImpedance + zc);
    out.powerTransmission = 1.0 - std::norm(out.throatReflection);
    // Scale to the drive.
    out.throatVelocity = throatVelocity;
    const Complex scale = throatVelocity * St / u;
    out.radiatedPower = 0.5 * out.throatImpedance.real() * std::norm(throatVelocity * St);
    const Complex pm = state.back().first * scale, um = state.back().second * scale;
    out.mouthPower = 0.5 * (pm * std::conj(um)).real();
    // Far field of the mouth as a baffled piston of the mouth's volume velocity: |p| = w rho |U| / (2 pi r).
    out.onAxisPressureAt1m = omega * rho * std::abs(um) / (2.0 * kPi * 1.0);
    const double ka = omega / c * s.mouthRadius;
    out.directivityFactor = pistonDirectivityFactor(ka);
    out.directivityIndexDb = 10.0 * std::log10(out.directivityFactor);
    if (ka > 2.215)
        out.halfAngleMinus6dB = std::asin(std::min(1.0, 2.2151 / ka));
    for (int k = 0; k < sampleCount; ++k) {
        const double x = s.length * k / (sampleCount - 1);
        const size_t i = static_cast<size_t>(std::lround(x / s.length * n));
        HornSample sample;
        sample.x = s.length * static_cast<double>(i) / n;
        sample.radius = hornRadius(s, sample.x);
        const Complex P = state[i].first * scale, U = state[i].second * scale;
        const double S = area(sample.x);
        sample.pressure = P;
        sample.velocity = U / S;
        sample.intensity = 0.5 * (P * std::conj(U / S)).real();
        sample.power = 0.5 * (P * std::conj(U)).real();
        out.samples.push_back(sample);
    }
    return R(std::move(out));
}

}  // namespace resonant::manipulators
