// Layered media at oblique incidence and the uniaxial homogenized slab (RES-META-003,
// RES-META-005): normal incidence against the layered-media solver, a slab against the
// Airy formula and Brewster's angle, energy conservation, the uniaxial slab's limits,
// and the homogenization error shrinking with the period.
#include "electromagnetics/Conventions.h"
#include "electromagnetics/LayeredMedia.h"
#include "electromagnetics/ObliqueLayers.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using em::Polarization;

namespace {
constexpr double kPi = 3.14159265358979323846;
const Complex j(0.0, 1.0);

std::vector<em::IsotropicLayer> periodic(double d1, Complex e1, double d2, Complex e2, int periods)
{
    std::vector<em::IsotropicLayer> s;
    for (int n = 0; n < periods; ++n) {
        s.push_back({0.5 * d1, e1, Complex(1.0)});
        s.push_back({d2, e2, Complex(1.0)});
        s.push_back({0.5 * d1, e1, Complex(1.0)});
    }
    return s;
}
}  // namespace

TEST(ObliqueLayers, NormalIncidenceMatchesTheLayeredMediaSolver)
{
    em::LayeredProblem p;
    p.frequencyHz = 3e9;
    em::MediumLayer a;
    a.thickness = 0.01;
    a.permittivityReal = 4.0;
    a.lossTangent = 0.02;
    em::MediumLayer b;
    b.thickness = 0.007;
    b.permittivityReal = 9.5;
    p.layers = {a, b};
    auto ref = em::LayeredSolution::solve(p);
    ASSERT_TRUE(ref) << ref.message();
    const std::vector<em::IsotropicLayer> stack{{0.01, Complex(4.0, -0.08), Complex(1.0)}, {0.007, Complex(9.5, 0.0), Complex(1.0)}};
    for (Polarization pol : {Polarization::TE, Polarization::TM}) {
        const auto r = em::obliqueStack(stack, 3e9, 0.0, pol);
        EXPECT_NEAR(r.reflectance, ref.value().reflectance(), 1e-12);
        EXPECT_NEAR(r.transmittance, ref.value().transmittance(), 1e-12);
    }
}

TEST(ObliqueLayers, ASlabFollowsTheAiryFormulaAndBrewstersAngle)
{
    const double eps = 6.0, d = 0.012, f = 5e9, k0 = 2 * kPi * f / em::kSpeedOfLight;
    for (double deg : {0.0, 20.0, 50.0, 75.0}) {
        const double th = deg * kPi / 180.0, c = std::cos(th), s2 = std::pow(std::sin(th), 2);
        const double kz = std::sqrt(eps - s2);
        const Complex phase = std::exp(-2.0 * j * k0 * d * kz);
        for (Polarization pol : {Polarization::TE, Polarization::TM}) {
            const double r12 = pol == Polarization::TE ? (c - kz) / (c + kz) : (eps * c - kz) / (eps * c + kz);
            const Complex r = r12 * (1.0 - phase) / (1.0 - r12 * r12 * phase);
            const auto got = em::obliqueStack({{d, Complex(eps), Complex(1.0)}}, f, th, pol);
            EXPECT_NEAR(got.reflectance, std::norm(r), 1e-12) << deg;
            EXPECT_NEAR(got.reflectance + got.transmittance, 1.0, 1e-12) << deg;
        }
    }
    // TM light at Brewster's angle passes without reflection; TE does not.
    const double brewster = std::atan(std::sqrt(eps));
    EXPECT_LT(em::obliqueStack({{d, Complex(eps), Complex(1.0)}}, f, brewster, Polarization::TM).reflectance, 1e-24);
    EXPECT_GT(em::obliqueStack({{d, Complex(eps), Complex(1.0)}}, f, brewster, Polarization::TE).reflectance, 0.1);
}

TEST(ObliqueLayers, LossesAbsorbAndLosslessStacksConserveEnergy)
{
    const auto lossless = periodic(0.004, Complex(10.0), 0.006, Complex(2.0), 7);
    const auto lossy = periodic(0.004, Complex(10.0, -0.5), 0.006, Complex(2.0), 7);
    for (double f : {1e9, 4e9, 9e9})
        for (Polarization pol : {Polarization::TE, Polarization::TM}) {
            EXPECT_NEAR(em::obliqueStack(lossless, f, 0.6, pol).absorptance, 0.0, 1e-12);
            EXPECT_GT(em::obliqueStack(lossy, f, 0.6, pol).absorptance, 1e-3);
        }
}

TEST(ObliqueLayers, TheUniaxialSlabReducesToTheIsotropicOne)
{
    const Complex eps(5.0, -0.1);
    for (Polarization pol : {Polarization::TE, Polarization::TM}) {
        const auto a = em::uniaxialSlab(0.02, eps, eps, 2e9, 0.7, pol);
        const auto b = em::obliqueStack({{0.02, eps, Complex(1.0)}}, 2e9, 0.7, pol);
        EXPECT_NEAR(std::abs(a.r - b.r), 0.0, 1e-12);
        EXPECT_NEAR(std::abs(a.t - b.t), 0.0, 1e-12);
    }
    // TE fields lie along the layers: eps_perp does not enter; TM feels it.
    const auto te1 = em::uniaxialSlab(0.02, Complex(5.0), Complex(2.0), 2e9, 0.7, Polarization::TE);
    const auto te2 = em::uniaxialSlab(0.02, Complex(5.0), Complex(3.0), 2e9, 0.7, Polarization::TE);
    EXPECT_NEAR(std::abs(te1.t - te2.t), 0.0, 1e-15);
    const auto tm1 = em::uniaxialSlab(0.02, Complex(5.0), Complex(2.0), 2e9, 0.7, Polarization::TM);
    const auto tm2 = em::uniaxialSlab(0.02, Complex(5.0), Complex(3.0), 2e9, 0.7, Polarization::TM);
    EXPECT_GT(std::abs(tm1.t - tm2.t), 1e-3);
}

TEST(ObliqueLayers, HomogenizationConvergesAsThePeriodShrinks)
{
    // The same 60 mm slab of a 10 / 2 cell, with ever finer periods: the explicit stack
    // approaches the uniaxial slab, the error falling as (a / lambda)^2.
    const double e1 = 10.0, e2 = 2.0, thickness = 0.06, f = 1.5e9, angle = 50.0 * kPi / 180.0;
    double previous = 0.0;
    for (int periods : {6, 12, 24}) {
        const double a = thickness / periods, d1 = 0.4 * a, d2 = 0.6 * a;
        const Complex par = (d1 * e1 + d2 * e2) / a, perp = a / (d1 / e1 + d2 / e2);
        const auto stack = periodic(d1, Complex(e1), d2, Complex(e2), periods);
        double err = 0.0;
        for (Polarization pol : {Polarization::TE, Polarization::TM})
            err = std::max(err, std::abs(em::obliqueStack(stack, f, angle, pol).t - em::uniaxialSlab(thickness, par, perp, f, angle, pol).t));
        if (previous > 0.0) {
            EXPECT_NEAR(previous / err, 4.0, 0.4) << periods;
        }
        previous = err;
    }
    EXPECT_LT(previous, 2e-3);
}

TEST(ObliqueLayers, ObliqueBlochWavesFollowTheUniaxialDispersion)
{
    const double e1 = 10.0, e2 = 2.0, d1 = 0.002, d2 = 0.003, a = d1 + d2;
    const double par = (d1 * e1 + d2 * e2) / a, perp = a / (d1 / e1 + d2 / e2);
    const double f = 0.01 * em::kSpeedOfLight / a, angle = 0.8, k0 = 2 * kPi * f / em::kSpeedOfLight, s2 = std::pow(std::sin(angle), 2);
    const std::vector<em::IsotropicLayer> cell{{0.5 * d1, Complex(e1), Complex(1.0)}, {d2, Complex(e2), Complex(1.0)}, {0.5 * d1, Complex(e1), Complex(1.0)}};
    EXPECT_NEAR(em::obliqueBloch(cell, f, angle, Polarization::TE).real(), k0 * std::sqrt(par - s2), 1e-3 * k0);
    EXPECT_NEAR(em::obliqueBloch(cell, f, angle, Polarization::TM).real(), k0 * std::sqrt(par * (1.0 - s2 / perp)), 1e-3 * k0);
}
