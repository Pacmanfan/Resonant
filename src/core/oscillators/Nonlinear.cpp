#include "oscillators/Nonlinear.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace resonant::oscillators {

namespace {
constexpr double kPi = 3.14159265358979323846;

// One RK4 step of y' = f(t, y) for a 2-state system.
template <class F>
std::array<double, 2> rk4(const F& f, double t, const std::array<double, 2>& y, double h)
{
    const auto add = [](const std::array<double, 2>& a, const std::array<double, 2>& b, double s) {
        return std::array<double, 2>{a[0] + s * b[0], a[1] + s * b[1]};
    };
    const auto k1 = f(t, y);
    const auto k2 = f(t + 0.5 * h, add(y, k1, 0.5 * h));
    const auto k3 = f(t + 0.5 * h, add(y, k2, 0.5 * h));
    const auto k4 = f(t + h, add(y, k3, h));
    return {y[0] + h / 6.0 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0]), y[1] + h / 6.0 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1])};
}
}  // namespace

Status DuffingOscillator::validate() const
{
    if (!(mass > 0.0) || !(stiffness > 0.0) || damping < 0.0 || !std::isfinite(mass + stiffness + damping + cubic))
        return Status::failure("a Duffing oscillator needs a positive mass and stiffness and non-negative damping");
    return Status::success();
}

double duffingBackbone(const DuffingOscillator& d, double a)
{
    return std::sqrt(std::max(0.0, (d.stiffness + 0.75 * d.cubic * a * a) / d.mass));
}

std::vector<DuffingBranch> duffingResponse(const DuffingOscillator& d, double force, double w)
{
    // With u = A^2: (p + q u)^2 u + r u - F^2 = 0, p = k - m w^2, q = 3/4 k3, r = (c w)^2:
    // q^2 u^3 + 2 p q u^2 + (p^2 + r) u - F^2 = 0.
    const double p = d.stiffness - d.mass * w * w, q = 0.75 * d.cubic, r = std::pow(d.damping * w, 2), F2 = force * force;
    std::vector<double> roots;
    if (std::abs(q) < 1e-300) {
        roots.push_back(F2 / (p * p + r));
    } else {
        // Cubic a u^3 + b u^2 + c u + e = 0 by the trigonometric / Cardano method.
        const double a = q * q, b = 2 * p * q, c = p * p + r, e = -F2;
        const double B = b / a, C = c / a, E = e / a;
        const double Q = (3 * C - B * B) / 9, Rr = (9 * B * C - 27 * E - 2 * B * B * B) / 54, D = Q * Q * Q + Rr * Rr;
        if (D > 0) {
            const double S = std::cbrt(Rr + std::sqrt(D)), T = std::cbrt(Rr - std::sqrt(D));
            roots.push_back(-B / 3 + S + T);
        } else {
            const double th = std::acos(std::clamp(Rr / std::sqrt(-Q * Q * Q), -1.0, 1.0));
            for (int k = 0; k < 3; ++k)
                roots.push_back(2 * std::sqrt(-Q) * std::cos((th + 2 * kPi * k) / 3) - B / 3);
        }
    }
    std::vector<double> amplitudes;
    for (double u : roots)
        if (u > 0.0)
            amplitudes.push_back(std::sqrt(u));
    std::sort(amplitudes.begin(), amplitudes.end());
    std::vector<DuffingBranch> out;
    for (size_t i = 0; i < amplitudes.size(); ++i)
        out.push_back({amplitudes[i], !(amplitudes.size() == 3 && i == 1)});
    return out;
}

Result<double> duffingMeasuredFrequency(const DuffingOscillator& d, double amplitude, int periods)
{
    if (Status s = d.validate(); !s)
        return Result<double>::failure(s.message());
    const double w = duffingBackbone(d, amplitude);
    const double h = 2 * kPi / w / 400.0;
    const auto f = [&](double, const std::array<double, 2>& y) {
        return std::array<double, 2>{y[1], (-d.damping * y[1] - d.stiffness * y[0] - d.cubic * y[0] * y[0] * y[0]) / d.mass};
    };
    std::array<double, 2> y{amplitude, 0.0};
    double t = 0.0, first = -1.0, last = -1.0;
    int crossings = 0;
    for (long s = 0; s < 400L * (periods + 2) && crossings < 2 * periods + 1; ++s) {
        const auto next = rk4(f, t, y, h);
        if (y[0] > 0.0 && next[0] <= 0.0) {  // downward crossing, linear interpolation
            const double tc = t + h * y[0] / (y[0] - next[0]);
            if (first < 0.0)
                first = tc;
            last = tc;
            ++crossings;
        }
        y = next;
        t += h;
    }
    if (crossings < 2)
        return Result<double>::failure("the oscillation did not complete two periods");
    return 2 * kPi * (crossings - 1) / (last - first);
}

Result<double> duffingForcedAmplitude(const DuffingOscillator& d, double force, double w, double a0, int settle)
{
    if (Status s = d.validate(); !s)
        return Result<double>::failure(s.message());
    const double h = 2 * kPi / w / 200.0;
    const auto f = [&](double t, const std::array<double, 2>& y) {
        return std::array<double, 2>{y[1], (force * std::cos(w * t) - d.damping * y[1] - d.stiffness * y[0] - d.cubic * std::pow(y[0], 3)) / d.mass};
    };
    std::array<double, 2> y{a0, 0.0};
    double t = 0.0, peak = 0.0;
    const long total = 200L * settle, measure = 200L * 10;
    for (long s = 0; s < total; ++s) {
        y = rk4(f, t, y, h);
        t += h;
        if (s >= total - measure)
            peak = std::max(peak, std::abs(y[0]));
    }
    return peak;
}

Status MathieuOscillator::validate() const
{
    if (!(naturalOmega > 0.0) || dampingRatio < 0.0 || modulation < 0.0 || modulation >= 1.0)
        return Status::failure("a Mathieu oscillator needs w0 > 0, zeta >= 0 and 0 <= h < 1");
    return Status::success();
}

double mathieuMultiplier(const MathieuOscillator& m, double W)
{
    const double T = 2 * kPi / W;
    const int n = 2000;
    const double h = T / n;
    const auto f = [&](double t, const std::array<double, 2>& y) {
        return std::array<double, 2>{y[1], -2 * m.dampingRatio * m.naturalOmega * y[1] -
                                               m.naturalOmega * m.naturalOmega * (1.0 + m.modulation * std::cos(W * t)) * y[0]};
    };
    std::array<double, 2> a{1.0, 0.0}, b{0.0, 1.0};
    double t = 0.0;
    for (int s = 0; s < n; ++s) {
        a = rk4(f, t, a, h);
        b = rk4(f, t, b, h);
        t += h;
    }
    // Monodromy [[a0, b0], [a1, b1]]: eigenvalues of a 2x2.
    const double tr = a[0] + b[1], det = a[0] * b[1] - b[0] * a[1];
    const std::complex<double> disc = std::sqrt(std::complex<double>(tr * tr / 4 - det));
    return std::max(std::abs(tr / 2 + disc), std::abs(tr / 2 - disc));
}

std::vector<double> mathieuTongue(const MathieuOscillator& m)
{
    const double w0 = m.naturalOmega;
    // On the stability side |lambda| = sqrt(det) = 1 up to the integrator's rounding: a margin
    // keeps those points stable (the growth near an edge rises like the square root of the distance).
    const auto unstable = [&](double W) { return mathieuMultiplier(m, W) > 1.0 + 1e-8; };
    if (!unstable(2 * w0))
        return {};
    const auto edge = [&](double inside, double outside) {
        for (int i = 0; i < 60; ++i) {
            const double mid = 0.5 * (inside + outside);
            (unstable(mid) ? inside : outside) = mid;
        }
        return 0.5 * (inside + outside);
    };
    return {edge(2 * w0, 2 * w0 * (1 - 0.6 * m.modulation - 1e-3)), edge(2 * w0, 2 * w0 * (1 + 0.6 * m.modulation + 1e-3))};
}

}  // namespace resonant::oscillators
