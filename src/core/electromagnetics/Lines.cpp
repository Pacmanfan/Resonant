#include "electromagnetics/Lines.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::em {

double ellipticK(double k)
{
    double a = 1.0, b = std::sqrt(std::max(0.0, 1.0 - k * k));
    for (int i = 0; i < 60 && std::abs(a - b) > 1e-16 * a; ++i) {
        const double next = 0.5 * (a + b);
        b = std::sqrt(a * b);
        a = next;
    }
    return kPi / (2.0 * a);
}

LineParameters coaxialLine(double a, double b, double er)
{
    LineParameters p;
    p.effectivePermittivity = er;
    p.characteristicImpedance = kEta0 / (2.0 * kPi * std::sqrt(er)) * std::log(b / a);
    p.phaseVelocity = kSpeedOfLight / std::sqrt(er);
    p.model = "coaxial TEM line, exact for perfect conductors";
    return p;
}

LineParameters microstripLine(double w, double h, double er)
{
    const double u = w / h;
    const double a = 1.0 + std::log((std::pow(u, 4) + std::pow(u / 52.0, 2)) / (std::pow(u, 4) + 0.432)) / 49.0
                     + std::log(1.0 + std::pow(u / 18.1, 3)) / 18.7;
    const double b = 0.564 * std::pow((er - 0.9) / (er + 3.0), 0.053);
    LineParameters p;
    p.effectivePermittivity = 0.5 * (er + 1.0) + 0.5 * (er - 1.0) * std::pow(1.0 + 10.0 / u, -a * b);
    const double f = 6.0 + (2.0 * kPi - 6.0) * std::exp(-std::pow(30.666 / u, 0.7528));
    const double z01 = kEta0 / (2.0 * kPi) * std::log(f / u + std::sqrt(1.0 + 4.0 / (u * u)));
    p.characteristicImpedance = z01 / std::sqrt(p.effectivePermittivity);
    p.phaseVelocity = kSpeedOfLight / std::sqrt(p.effectivePermittivity);
    p.model = "microstrip, quasi-static Hammerstad-Jensen formulas (zero strip thickness, no dispersion; about 0.2 % for "
              "0.01 <= w/h <= 100)";
    return p;
}

LineParameters striplineLine(double w, double spacing, double er)
{
    const double k = 1.0 / std::cosh(kPi * w / (2.0 * spacing));
    const double kp = std::tanh(kPi * w / (2.0 * spacing));
    LineParameters p;
    p.effectivePermittivity = er;
    p.characteristicImpedance = kEta0 / (4.0 * std::sqrt(er)) * ellipticK(k) / ellipticK(kp);
    p.phaseVelocity = kSpeedOfLight / std::sqrt(er);
    p.model = "centred stripline of zero thickness, exact conformal mapping (Cohn)";
    return p;
}

LineParameters coplanarLine(double s, double g, double er)
{
    const double k = s / (s + 2.0 * g), kp = std::sqrt(1.0 - k * k);
    LineParameters p;
    p.effectivePermittivity = 0.5 * (er + 1.0);
    p.characteristicImpedance = kEta0 / (4.0 * std::sqrt(p.effectivePermittivity)) * ellipticK(kp) / ellipticK(k);
    p.phaseVelocity = kSpeedOfLight / std::sqrt(p.effectivePermittivity);
    p.model = "coplanar waveguide on an infinitely thick substrate, zero metal thickness, exact conformal mapping";
    return p;
}

Complex inputImpedance(Complex load, double z0, double beta, double length, double alpha)
{
    const Complex gl(alpha * length, beta * length);
    const Complex t = std::tanh(gl);
    return z0 * (load + z0 * t) / (z0 + load * t);
}

double quarterWaveTransformer(double z0, double rl)
{
    return std::sqrt(z0 * rl);
}

std::vector<LMatch> lNetworkMatches(Complex z, double r0)
{
    // Pozar section 5.1: two topologies depending on whether R_L > R_0.
    std::vector<LMatch> out;
    const double r = z.real(), x = z.imag();
    if (r <= 0.0)
        return out;
    if (r > r0) {
        // Shunt susceptance across the load, then series reactance.
        const double root = std::sqrt(r / r0) * std::sqrt(r * r + x * x - r0 * r);
        for (const double sign : {1.0, -1.0}) {
            const double bsh = (x + sign * root) / (r * r + x * x);
            const double xs = 1.0 / bsh + x * r0 / r - r0 / (bsh * r);
            out.push_back({xs, bsh, true});
        }
    } else {
        // Series reactance at the load, then shunt across the source.
        const double root = std::sqrt(r * (r0 - r));
        for (const double sign : {1.0, -1.0}) {
            const double xs = sign * root - x;
            const double bsh = sign * std::sqrt((r0 - r) / r) / r0;
            out.push_back({xs, bsh, false});
        }
    }
    return out;
}

double coaxialConductorLoss(double a, double b, double er, double sigma, double f)
{
    const double rs = std::sqrt(2.0 * kPi * f * kMu0 / (2.0 * sigma));
    const double eta = kEta0 / std::sqrt(er);
    return rs / (2.0 * eta * std::log(b / a)) * (1.0 / a + 1.0 / b);
}

double dielectricLoss(double er, double tanDelta, double f)
{
    return 0.5 * wavenumber(f) * std::sqrt(er) * tanDelta;
}

std::string GuideMode::label() const
{
    char text[32];
    if (kind == ModeKind::TEM)
        return "TEM";
    std::snprintf(text, sizeof text, "%s%d%d", kind == ModeKind::TE ? "TE" : "TM", m, n);
    return text;
}

std::vector<GuideMode> rectangularGuideModes(double a, double b, int count, double er, double mr)
{
    std::vector<GuideMode> modes;
    const double c = kSpeedOfLight / std::sqrt(er * mr);
    for (int m = 0; m <= 12; ++m)
        for (int n = 0; n <= 12; ++n) {
            if (m == 0 && n == 0)
                continue;
            const double fc = 0.5 * c * std::hypot(m / a, n / b);
            modes.push_back({ModeKind::TE, m, n, fc});
            if (m >= 1 && n >= 1)
                modes.push_back({ModeKind::TM, m, n, fc});
        }
    std::stable_sort(modes.begin(), modes.end(), [](const GuideMode& x, const GuideMode& y) { return x.cutoffHz < y.cutoffHz; });
    if (static_cast<int>(modes.size()) > count)
        modes.resize(static_cast<size_t>(count));
    return modes;
}

double besselZero(int n, int m, bool derivative)
{
    // Bracket sign changes on a fine grid, then bisect.
    const auto f = [n, derivative](double x) {
        if (!derivative)
            return std::cyl_bessel_j(static_cast<double>(n), x);
        return n == 0 ? -std::cyl_bessel_j(1.0, x) : 0.5 * (std::cyl_bessel_j(n - 1.0, x) - std::cyl_bessel_j(n + 1.0, x));
    };
    int found = 0;
    double previous = f(1e-6);
    const double step = 0.01;
    for (double x = 1e-6 + step; x < 200.0; x += step) {
        const double value = f(x);
        if ((previous < 0.0) != (value < 0.0) && !(derivative && n == 0 && x < 0.1)) {
            double lo = x - step, hi = x;
            for (int i = 0; i < 100; ++i) {
                const double mid = 0.5 * (lo + hi);
                if ((f(lo) < 0.0) != (f(mid) < 0.0))
                    hi = mid;
                else
                    lo = mid;
            }
            if (++found == m)
                return 0.5 * (lo + hi);
        }
        previous = value;
    }
    return 0.0;
}

std::vector<GuideMode> circularGuideModes(double radius, int count, double er)
{
    std::vector<GuideMode> modes;
    const double c = kSpeedOfLight / std::sqrt(er);
    for (int n = 0; n <= 6; ++n)
        for (int m = 1; m <= 4; ++m) {
            modes.push_back({ModeKind::TE, n, m, c * besselZero(n, m, true) / (2.0 * kPi * radius)});
            modes.push_back({ModeKind::TM, n, m, c * besselZero(n, m, false) / (2.0 * kPi * radius)});
        }
    std::stable_sort(modes.begin(), modes.end(), [](const GuideMode& x, const GuideMode& y) { return x.cutoffHz < y.cutoffHz; });
    if (static_cast<int>(modes.size()) > count)
        modes.resize(static_cast<size_t>(count));
    return modes;
}

std::vector<GuideMode> parallelPlateModes(double d, int count, double er)
{
    std::vector<GuideMode> modes{{ModeKind::TEM, 0, 0, 0.0}};
    const double c = kSpeedOfLight / std::sqrt(er);
    for (int n = 1; static_cast<int>(modes.size()) < count; ++n) {
        modes.push_back({ModeKind::TE, n, 0, n * c / (2.0 * d)});
        if (static_cast<int>(modes.size()) < count)
            modes.push_back({ModeKind::TM, n, 0, n * c / (2.0 * d)});
    }
    return modes;
}

ModePropagation propagate(const GuideMode& mode, double f, double er, double mr)
{
    ModePropagation p;
    const double k = wavenumber(f) * std::sqrt(er * mr), eta = kEta0 * std::sqrt(mr / er);
    const double ratio = mode.cutoffHz / f;
    if (mode.kind == ModeKind::TEM) {
        p.propagating = true;
        p.beta = k;
        p.waveImpedance = eta;
    } else if (ratio < 1.0) {
        const double root = std::sqrt(1.0 - ratio * ratio);
        p.propagating = true;
        p.beta = k * root;
        p.waveImpedance = mode.kind == ModeKind::TE ? eta / root : eta * root;
    } else {
        p.attenuation = k * std::sqrt(ratio * ratio - 1.0);
        return p;
    }
    p.guideWavelength = 2.0 * kPi / p.beta;
    p.phaseVelocity = 2.0 * kPi * f / p.beta;
    const double v = kSpeedOfLight / std::sqrt(er * mr);
    p.groupVelocity = v * v / p.phaseVelocity;
    return p;
}

std::vector<CavityMode> rectangularCavityModes(double a, double b, double d, int count, double er)
{
    std::vector<CavityMode> modes;
    const double c = kSpeedOfLight / std::sqrt(er);
    for (int m = 0; m <= 6; ++m)
        for (int n = 0; n <= 6; ++n)
            for (int p = 0; p <= 6; ++p) {
                const double f = 0.5 * c * std::sqrt((m / a) * (m / a) + (n / b) * (n / b) + (p / d) * (p / d));
                // TE_mnp (z along d): m or n nonzero, p >= 1. TM_mnp: m, n >= 1, p >= 0.
                if ((m || n) && p >= 1)
                    modes.push_back({ModeKind::TE, m, n, p, f});
                if (m >= 1 && n >= 1)
                    modes.push_back({ModeKind::TM, m, n, p, f});
            }
    std::stable_sort(modes.begin(), modes.end(), [](const CavityMode& x, const CavityMode& y) { return x.frequencyHz < y.frequencyHz; });
    if (static_cast<int>(modes.size()) > count)
        modes.resize(static_cast<size_t>(count));
    return modes;
}

TE10Field te10Field(double a, double x, double z, double f, Complex e0)
{
    const double omega = 2.0 * kPi * f, k = wavenumber(f);
    const double beta = std::sqrt(std::max(0.0, k * k - (kPi / a) * (kPi / a)));
    const Complex phase = std::polar(1.0, -beta * z), j(0.0, 1.0);
    TE10Field out;
    out.ey = e0 * std::sin(kPi * x / a) * phase;
    out.hx = -(beta / (omega * kMu0)) * out.ey;
    out.hz = j * kPi / (omega * kMu0 * a) * e0 * std::cos(kPi * x / a) * phase;
    return out;
}

double te10Power(double a, double b, double f, double e0)
{
    const double omega = 2.0 * kPi * f, k = wavenumber(f);
    const double beta = std::sqrt(std::max(0.0, k * k - (kPi / a) * (kPi / a)));
    return e0 * e0 * a * b * beta / (4.0 * omega * kMu0);
}

}  // namespace resonant::em
