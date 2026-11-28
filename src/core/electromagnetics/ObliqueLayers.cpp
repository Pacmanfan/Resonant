#include "electromagnetics/ObliqueLayers.h"

#include "electromagnetics/Conventions.h"

#include <array>
#include <cmath>

namespace resonant::em {

namespace {

const Complex j(0.0, 1.0);
using M2 = std::array<Complex, 4>;

M2 mul(const M2& a, const M2& b)
{
    return {a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3], a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3]};
}

// The decaying root: Im <= 0 for e^{-j kz z} (and Re >= 0 when propagating).
Complex root(Complex x)
{
    Complex r = std::sqrt(x);
    if (r.imag() > 0.0 || (r.imag() == 0.0 && r.real() < 0.0))
        r = -r;
    return r;
}

// A layer with normalized longitudinal wavenumber kz/k0 and transverse impedance z (eta0 units).
M2 layerMatrix(Complex kzOverK0, Complex z, double k0, double d)
{
    const Complex delta = k0 * d * kzOverK0;
    return {std::cos(delta), j * z * std::sin(delta), j * std::sin(delta) / z, std::cos(delta)};
}

void isotropic(const IsotropicLayer& l, double s2, Polarization p, Complex& kz, Complex& z)
{
    kz = root(l.permittivity * l.permeability - s2);
    z = p == Polarization::TE ? l.permeability / kz : kz / l.permittivity;
}

ObliqueResponse respond(const M2& m, double angle, Polarization p)
{
    const double c = std::cos(angle);
    const Complex z0 = p == Polarization::TE ? 1.0 / c : Complex(c, 0.0);
    // [E_in, H_in] = M [E_out, E_out / z0]; incident E+ = (E_in + z0 H_in) / 2.
    const Complex ein = m[0] + m[1] / z0, hin = m[2] + m[3] / z0;
    ObliqueResponse r;
    r.t = 2.0 / (ein + z0 * hin);
    r.r = (ein - z0 * hin) / (ein + z0 * hin);
    r.reflectance = std::norm(r.r);
    r.transmittance = std::norm(r.t);
    r.absorptance = 1.0 - r.reflectance - r.transmittance;
    return r;
}

}  // namespace

ObliqueResponse obliqueStack(const std::vector<IsotropicLayer>& layers, double f, double angle, Polarization p)
{
    const double k0 = 2 * kPi * f / kSpeedOfLight, s2 = std::pow(std::sin(angle), 2);
    M2 m{Complex(1.0), Complex(0.0), Complex(0.0), Complex(1.0)};
    for (const IsotropicLayer& l : layers) {
        Complex kz, z;
        isotropic(l, s2, p, kz, z);
        m = mul(m, layerMatrix(kz, z, k0, l.thickness));
    }
    return respond(m, angle, p);
}

ObliqueResponse uniaxialSlab(double d, Complex epsPar, Complex epsPerp, double f, double angle, Polarization p)
{
    const double k0 = 2 * kPi * f / kSpeedOfLight, s2 = std::pow(std::sin(angle), 2);
    Complex kz, z;
    if (p == Polarization::TE) {
        kz = root(epsPar - s2);
        z = 1.0 / kz;
    } else {
        // kx^2 / eps_zz + kz^2 / eps_xx = k0^2; E_x / H_y = kz / (w eps0 eps_xx).
        kz = root(epsPar * (1.0 - s2 / epsPerp));
        z = kz / epsPar;
    }
    return respond(layerMatrix(kz, z, k0, d), angle, p);
}

Complex obliqueBloch(const std::vector<IsotropicLayer>& cell, double f, double angle, Polarization p)
{
    const double k0 = 2 * kPi * f / kSpeedOfLight, s2 = std::pow(std::sin(angle), 2);
    M2 m{Complex(1.0), Complex(0.0), Complex(0.0), Complex(1.0)};
    double a = 0.0;
    for (const IsotropicLayer& l : cell) {
        Complex kz, z;
        isotropic(l, s2, p, kz, z);
        m = mul(m, layerMatrix(kz, z, k0, l.thickness));
        a += l.thickness;
    }
    // The principal phase per cell in [0, pi]; the attenuation per cell as |Im|.
    const Complex Ka = std::acos(0.5 * (m[0] + m[3]));
    return Complex(Ka.real(), std::abs(Ka.imag())) / a;
}

}  // namespace resonant::em
