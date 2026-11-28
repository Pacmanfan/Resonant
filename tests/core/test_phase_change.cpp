// Phase change by the enthalpy method (RES-DMS-007, -012): the melt front against the
// one-phase Stefan solution, the energy balance (sensible and latent), the conductivity
// of each phase, and the refused unstable step.
#include "thermal/PhaseChange.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::thermal;

namespace {
PhaseMaterial paraffin()
{
    PhaseMaterial m;
    m.density = 800.0;
    m.solidSpecificHeat = 2000.0;
    m.liquidSpecificHeat = 2200.0;
    m.solidConductivity = 0.25;
    m.liquidConductivity = 0.15;
    m.meltingTemperature = 310.0;
    m.latentHeat = 2.0e5;
    return m;
}
}  // namespace

TEST(PhaseChange, TheMeltFrontFollowsTheStefanSolution)
{
    PhaseChangeProblem p;
    p.length = 0.05;
    p.cells = 400;
    p.material = paraffin();
    p.initialTemperature = 310.0;  // solid at the melting point
    p.left = {SlabFace::Kind::Temperature, 330.0};
    p.right = {SlabFace::Kind::Adiabatic, 0.0};
    PhaseChangeSettings s;
    s.duration = 3600.0;
    s.snapshots = 6;
    auto r = solvePhaseChange(p, s);
    ASSERT_TRUE(r) << r.message();
    const double st = 2200.0 * 20.0 / 2.0e5, lambda = stefanLambda(st), alpha = 0.15 / (800.0 * 2200.0);
    EXPECT_NEAR(lambda * std::exp(lambda * lambda) * std::erf(lambda), st / std::sqrt(3.14159265358979323846), 1e-12);
    for (size_t i = 2; i < r.value().times.size(); ++i) {
        const double s_exact = 2.0 * lambda * std::sqrt(alpha * r.value().times[i]);
        EXPECT_NEAR(r.value().front[i], s_exact, 0.02 * s_exact) << r.value().times[i];
    }
    // Energy in through the hot wall = sensible + latent, to rounding.
    EXPECT_NEAR(r.value().balanceResidual, 0.0, 1e-9 * r.value().heatIn);
    EXPECT_NEAR(r.value().latentChange, 800.0 * 2.0e5 * r.value().front.back(), 1e-9 * r.value().latentChange);
    EXPECT_GT(r.value().latentChange, r.value().sensibleChange);  // St = 0.22: most of the heat melts
}

TEST(PhaseChange, SolidificationReleasesTheLatentHeat)
{
    PhaseChangeProblem p;
    p.length = 0.02;
    p.cells = 100;
    p.material = paraffin();
    p.initialTemperature = 320.0;  // liquid
    p.left = {SlabFace::Kind::Temperature, 290.0};
    p.right = {SlabFace::Kind::Adiabatic, 0.0};
    PhaseChangeSettings s;
    s.duration = 1800.0;
    auto r = solvePhaseChange(p, s);
    ASSERT_TRUE(r) << r.message();
    EXPECT_LT(r.value().heatIn, 0.0);        // heat leaves
    EXPECT_LT(r.value().latentChange, 0.0);  // the solid released its latent heat
    EXPECT_NEAR(r.value().balanceResidual, 0.0, 1e-9 * std::abs(r.value().heatIn));
    EXPECT_LT(r.value().front.back(), p.length);  // part of it froze
}

TEST(PhaseChange, AnUnstableStepIsRefused)
{
    PhaseChangeProblem p;
    p.length = 0.01;
    p.cells = 100;
    p.material = paraffin();
    p.initialTemperature = 300.0;
    PhaseChangeSettings s;
    s.duration = 10.0;
    s.timeStep = 10.0;
    auto r = solvePhaseChange(p, s);
    ASSERT_FALSE(r);
    EXPECT_NE(r.message().find("stability limit"), std::string::npos);
}
