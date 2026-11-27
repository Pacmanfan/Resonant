#include "electromagnetics/Antennas.h"

#include <cmath>
#include <functional>
#include <limits>

namespace resonant::em {
namespace {

constexpr double kEuler = 0.5772156649015329;

// Si and Ci together (Numerical Recipes "cisi").
void cisi(double x, double& ci, double& si)
{
    const int maxIterations = 200;
    const double eps = 1e-16, fpmin = 1e-300, tmin = 2.0;
    const double t = std::abs(x);
    if (t == 0.0) {
        si = 0.0;
        ci = -std::numeric_limits<double>::infinity();
        return;
    }
    if (t > tmin) {
        // Continued fraction for E1(i t) (modified Lentz).
        std::complex<double> b(1.0, t), c(1.0 / fpmin, 0.0), d = 1.0 / b, h = d;
        for (int i = 2; i <= maxIterations; ++i) {
            const double a = -(i - 1.0) * (i - 1.0);
            b += 2.0;
            d = 1.0 / (a * d + b);
            c = b + a / c;
            const std::complex<double> del = c * d;
            h *= del;
            if (std::abs(del.real() - 1.0) + std::abs(del.imag()) < eps)
                break;
        }
        h *= std::complex<double>(std::cos(t), -std::sin(t));
        ci = -h.real();
        si = kPi / 2.0 + h.imag();
    } else {
        double sum = 0.0, sums = 0.0, sumc = 0.0, sign = 1.0, fact = 1.0;
        bool odd = true;
        for (int k = 1; k <= maxIterations; ++k) {
            fact *= t / k;
            const double term = fact / k;
            sum += sign * term;
            const double err = term / std::abs(sum);
            if (odd) {
                sign = -sign;
                sums = sum;
                sum = sumc;
            } else {
                sumc = sum;
                sum = sums;
            }
            if (err < eps)
                break;
            odd = !odd;
        }
        si = sums;
        ci = sumc + std::log(t) + kEuler;
    }
    if (x < 0.0)
        si = -si;
}

}  // namespace

double sineIntegral(double x)
{
    double ci, si;
    cisi(x, ci, si);
    return si;
}

double cosineIntegral(double x)
{
    double ci, si;
    cisi(x, ci, si);
    return ci;
}

FieldPoint hertzianDipoleField(const Vec3& p, Complex il, double f)
{
    FieldPoint out;
    const double r = p.norm();
    if (r <= 0.0) {
        out.singular = true;
        return out;
    }
    const double k = wavenumber(f), omega = 2.0 * kPi * f;
    const double cost = p.z() / r, sint = std::sqrt(std::max(0.0, 1.0 - cost * cost));
    const Complex j(0.0, 1.0), e = std::exp(-j * k * r);
    // Balanis eq. 4-8a..c and 4-10a..c.
    const Complex er = kEta0 * il * cost / (2.0 * kPi * r * r) * (1.0 + 1.0 / (j * k * r)) * e;
    const Complex et = j * kEta0 * k * il * sint / (4.0 * kPi * r) * (1.0 + 1.0 / (j * k * r) - 1.0 / (k * k * r * r)) * e;
    const Complex hp = j * k * il * sint / (4.0 * kPi * r) * (1.0 + 1.0 / (j * k * r)) * e;
    (void)omega;
    const double rho = std::hypot(p.x(), p.y());
    const Vec3 rh = p / r;
    const Vec3 th = rho > 0.0 ? Vec3(cost * p.x() / rho, cost * p.y() / rho, -sint) : Vec3(1, 0, 0);
    const Vec3 ph = rho > 0.0 ? Vec3(-p.y() / rho, p.x() / rho, 0.0) : Vec3(0, 1, 0);
    out.e = er * rh.cast<Complex>() + et * th.cast<Complex>();
    out.h = hp * ph.cast<Complex>();
    return out;
}

FieldPoint smallLoopField(const Vec3& p, Complex m, double f)
{
    // Duality with the Hertzian dipole: I l -> j omega mu m (Balanis 5-18).
    FieldPoint out;
    const double r = p.norm();
    if (r <= 0.0) {
        out.singular = true;
        return out;
    }
    const double k = wavenumber(f);
    const double cost = p.z() / r, sint = std::sqrt(std::max(0.0, 1.0 - cost * cost));
    const Complex j(0.0, 1.0), e = std::exp(-j * k * r);
    const Complex hr = j * k * m * cost / (2.0 * kPi * r * r) * (1.0 + 1.0 / (j * k * r)) * e;
    const Complex ht = -k * k * m * sint / (4.0 * kPi * r) * (1.0 + 1.0 / (j * k * r) - 1.0 / (k * k * r * r)) * e;
    const Complex ep = kEta0 * k * k * m * sint / (4.0 * kPi * r) * (1.0 + 1.0 / (j * k * r)) * e;
    const double rho = std::hypot(p.x(), p.y());
    const Vec3 rh = p / r;
    const Vec3 th = rho > 0.0 ? Vec3(cost * p.x() / rho, cost * p.y() / rho, -sint) : Vec3(1, 0, 0);
    const Vec3 ph = rho > 0.0 ? Vec3(-p.y() / rho, p.x() / rho, 0.0) : Vec3(0, 1, 0);
    out.h = hr * rh.cast<Complex>() + ht * th.cast<Complex>();
    out.e = ep * ph.cast<Complex>();
    return out;
}

FieldPoint thinDipoleField(const Vec3& p, double length, Complex feedCurrent, double f)
{
    // Exact fields of I(z) = Im sin(k(h - |z|)) (Jordan & Balmain; Balanis 8-51):
    //   Ez   = -j eta Im / (4 pi) [e^{-jkR1}/R1 + e^{-jkR2}/R2 - 2 cos(kh) e^{-jkr}/r]
    //   Erho =  j eta Im / (4 pi rho) [(z-h) e^{-jkR1}/R1 + (z+h) e^{-jkR2}/R2 - 2 z cos(kh) e^{-jkr}/r]
    //   Hphi =  j Im / (4 pi rho) [e^{-jkR1} + e^{-jkR2} - 2 cos(kh) e^{-jkr}]
    FieldPoint out;
    const double k = wavenumber(f), h = 0.5 * length;
    const double sinkh = std::sin(k * h);
    const Complex im = std::abs(sinkh) > 1e-9 ? feedCurrent / sinkh : Complex(0.0, 0.0);
    const double rho = std::hypot(p.x(), p.y()), z = p.z();
    const double r = p.norm(), r1 = std::hypot(rho, z - h), r2 = std::hypot(rho, z + h);
    if (rho < 1e-12 && std::abs(z) <= h) {
        out.singular = true;
        return out;
    }
    const Complex j(0.0, 1.0);
    const Complex e1 = std::exp(-j * k * r1), e2 = std::exp(-j * k * r2), e0 = std::exp(-j * k * r);
    const double c = std::cos(k * h);
    const Complex ez = -j * kEta0 * im / (4.0 * kPi) * (e1 / r1 + e2 / r2 - 2.0 * c * e0 / r);
    Complex erho(0.0, 0.0), hphi(0.0, 0.0);
    if (rho > 1e-12) {
        erho = j * kEta0 * im / (4.0 * kPi * rho) * ((z - h) * e1 / r1 + (z + h) * e2 / r2 - 2.0 * z * c * e0 / r);
        hphi = j * im / (4.0 * kPi * rho) * (e1 + e2 - 2.0 * c * e0);
    }
    const Vec3 rhoHat = rho > 1e-12 ? Vec3(p.x() / rho, p.y() / rho, 0.0) : Vec3(1, 0, 0);
    const Vec3 phiHat = rho > 1e-12 ? Vec3(-p.y() / rho, p.x() / rho, 0.0) : Vec3(0, 1, 0);
    out.e = erho * rhoHat.cast<Complex>() + ez * Vec3::UnitZ().cast<Complex>();
    out.h = hphi * phiHat.cast<Complex>();
    return out;
}

double thinDipolePattern(double theta, double length, double f)
{
    const double kh = wavenumber(f) * 0.5 * length;
    const double s = std::sin(theta);
    if (std::abs(s) < 1e-12)
        return 0.0;
    return (std::cos(kh * std::cos(theta)) - std::cos(kh)) / s;
}

AntennaImpedance thinDipoleImpedance(double length, double radius, double f)
{
    const double k = wavenumber(f), kl = k * length;
    const double ci1 = cosineIntegral(kl), ci2 = cosineIntegral(2.0 * kl), si1 = sineIntegral(kl), si2 = sineIntegral(2.0 * kl);
    const double eta = kEta0;
    const double rr = eta / (2.0 * kPi)
                      * (kEuler + std::log(kl) - ci1 + 0.5 * std::sin(kl) * (si2 - 2.0 * si1)
                         + 0.5 * std::cos(kl) * (kEuler + std::log(kl / 2.0) + ci2 - 2.0 * ci1));
    const double xm = eta / (4.0 * kPi)
                      * (2.0 * si1 + std::cos(kl) * (2.0 * si1 - si2)
                         - std::sin(kl) * (2.0 * ci1 - ci2 - cosineIntegral(2.0 * k * radius * radius / length)));
    AntennaImpedance z;
    z.terminal = Complex(rr, xm);
    const double s = std::sin(kl / 2.0);
    z.valid = std::abs(s) > 0.2;
    z.input = std::abs(s) > 1e-9 ? z.terminal / (s * s) : Complex(std::numeric_limits<double>::infinity(), 0.0);
    return z;
}

AntennaImpedance monopoleImpedance(double height, double radius, double f)
{
    AntennaImpedance z = thinDipoleImpedance(2.0 * height, radius, f);
    z.terminal *= 0.5;
    z.input *= 0.5;
    return z;
}

AntennaImpedance smallLoopImpedance(double loopRadius, double wireRadius, double f)
{
    const double c = 2.0 * kPi * loopRadius, lambda = wavelength(f);
    const double r = 20.0 * kPi * kPi * std::pow(c / lambda, 4);
    const double x = 2.0 * kPi * f * kMu0 * loopRadius * (std::log(8.0 * loopRadius / wireRadius) - 2.0);
    AntennaImpedance z;
    z.terminal = z.input = Complex(r, x);
    z.valid = c < 0.3 * lambda;
    return z;
}

double resonantDipoleLength(double radius, double f)
{
    // Bisection of the input reactance between 0.40 and 0.50 wavelengths.
    const double lambda = wavelength(f);
    double lo = 0.40 * lambda, hi = 0.52 * lambda;
    for (int i = 0; i < 80; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (thinDipoleImpedance(mid, radius, f).input.imag() > 0.0)
            hi = mid;
        else
            lo = mid;
    }
    return 0.5 * (lo + hi);
}

double directivityOfAxisymmetric(const std::function<double(double)>& u, bool upperHalfOnly)
{
    const int n = 20000;
    const double top = upperHalfOnly ? kPi / 2.0 : kPi;
    double integral = 0.0, peak = 0.0;
    for (int i = 0; i < n; ++i) {
        const double theta = (i + 0.5) * top / n;
        const double value = u(theta);
        integral += value * std::sin(theta) * top / n;
        peak = std::max(peak, value);
    }
    return integral > 0.0 ? 2.0 * peak / integral : 0.0;  // 4 pi U_max / (2 pi int U sin)
}

MatchQuantities match(const Complex& z, double z0)
{
    MatchQuantities m;
    m.reflection = (z - z0) / (z + z0);
    const double g = std::abs(m.reflection);
    m.returnLossDb = g > 0.0 ? -20.0 * std::log10(g) : std::numeric_limits<double>::infinity();
    m.vswr = g < 1.0 ? (1.0 + g) / (1.0 - g) : std::numeric_limits<double>::infinity();
    m.mismatchEfficiency = 1.0 - g * g;
    return m;
}

}  // namespace resonant::em
