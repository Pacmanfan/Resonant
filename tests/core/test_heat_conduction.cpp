// Heat conduction against analytical solutions (RES-CEME-010).
#include "thermal/HeatConduction.h"
#include "thermal/ThermalMaterial.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace resonant;
using namespace resonant::thermal;

namespace {

ConductionProblem slab(size_t cells, double length, double k)
{
    ConductionProblem p;
    p.grid = CellGrid::uniform(Vec3(0, 0, 0), Vec3(0.1, 0.1, length), 1, 1, cells);
    p.conductivity.assign(cells, k);
    p.volumetricHeatCapacity.assign(cells, 1.5e6);
    return p;
}

double centre(const ConductionProblem& p, size_t c)
{
    return 0.5 * (p.grid.edges[2][c] + p.grid.edges[2][c + 1]);
}

}  // namespace

TEST(HeatConduction, UniformSourceBetweenFixedTemperaturesIsParabolic)
{
    const double length = 0.02, k = 0.8, q = 2e5, t0 = 300.0;
    ConductionProblem p = slab(200, length, k);
    p.source.assign(200, q);
    p.faces[4] = {FaceKind::Temperature, t0};
    p.faces[5] = {FaceKind::Temperature, t0};
    auto s = solveSteady(p);
    ASSERT_TRUE(s) << s.message();
    const double rise = q * length * length / (8 * k);  // the analytical maximum rise
    for (size_t c = 0; c < 200; ++c) {
        const double z = centre(p, c);
        EXPECT_NEAR(s.value().temperature[c], t0 + q * z * (length - z) / (2 * k), 2e-3 * rise);
    }
    // The discrete balance is exact: all the heat leaves through the two faces, half each.
    const double power = q * length * 0.01;
    EXPECT_NEAR(s.value().sourcePower, power, 1e-12 * power);
    EXPECT_NEAR(s.value().heatOut(), power, 1e-9 * power);
    EXPECT_NEAR(s.value().faceHeatOut[4], power / 2, 1e-9 * power);
    EXPECT_NEAR(s.value().balanceResidual, 0.0, 1e-9 * power);
}

TEST(HeatConduction, ConvectionOnBothSidesGivesTheSeriesResistanceFlux)
{
    // No source: a linear profile, which finite volumes reproduce exactly.
    const double length = 0.05, k = 2.0, h1 = 25.0, h2 = 8.0, hot = 400.0, cold = 290.0;
    ConductionProblem p = slab(30, length, k);
    p.faces[4] = {FaceKind::Convective, 0.0, h1, hot};
    p.faces[5] = {FaceKind::Convective, 0.0, h2, cold};
    auto s = solveSteady(p);
    ASSERT_TRUE(s) << s.message();
    const double area = 0.01;
    const double flux = (hot - cold) / (1 / h1 + length / k + 1 / h2);  // W/m^2
    EXPECT_NEAR(s.value().faceHeatOut[5], flux * area, 1e-9 * flux * area);
    EXPECT_NEAR(s.value().faceHeatOut[4], -flux * area, 1e-9 * flux * area);
    const double surfaceHot = hot - flux / h1;
    for (size_t c = 0; c < 30; ++c)
        EXPECT_NEAR(s.value().temperature[c], surfaceHot - flux * centre(p, c) / k, 1e-9 * (hot - cold));
}

TEST(HeatConduction, ThreeDimensionalBoxWithTwoFixedFacesIsLinear)
{
    ConductionProblem p;
    p.grid = CellGrid::uniform(Vec3(-0.1, 0, 0), Vec3(0.2, 0.05, 0.03), 8, 5, 4);
    p.conductivity.assign(p.grid.cellCount(), 15.0);
    p.faces[0] = {FaceKind::Temperature, 350.0};
    p.faces[1] = {FaceKind::Temperature, 300.0};
    auto s = solveSteady(p);
    ASSERT_TRUE(s) << s.message();
    for (size_t k = 0; k < 4; ++k)
        for (size_t j = 0; j < 5; ++j)
            for (size_t i = 0; i < 8; ++i) {
                const double x = 0.5 * (p.grid.edges[0][i] + p.grid.edges[0][i + 1]);
                EXPECT_NEAR(s.value().temperature[p.grid.index(i, j, k)], 350.0 - 50.0 * (x + 0.1) / 0.2, 1e-9);
            }
    EXPECT_NEAR(s.value().faceHeatOut[1], 15.0 * 50.0 / 0.2 * 0.05 * 0.03, 1e-9);
}

TEST(HeatConduction, InsulatedTransientStoresAllTheHeat)
{
    ConductionProblem p = slab(20, 0.01, 1.0);
    p.source.assign(20, 1e6);
    p.referenceTemperature = 293.15;
    TransientSettings t;
    t.duration = 3.0;
    t.steps = 30;
    auto s = solveTransient(p, t);
    ASSERT_TRUE(s) << s.message();
    const double rise = 1e6 * 3.0 / 1.5e6;  // q t / (rho c_p)
    for (double temperature : s.value().temperature)
        EXPECT_NEAR(temperature, 293.15 + rise, 1e-9);
    const double energy = 1e6 * 0.01 * 0.01 * 3.0;
    EXPECT_NEAR(s.value().storedEnergyChange, energy, 1e-9 * energy);
    EXPECT_NEAR(s.value().balanceResidual, 0.0, 1e-9 * energy);
    EXPECT_EQ(s.value().snapshots.size(), s.value().times.size());
}

TEST(HeatConduction, SuddenSurfaceTemperatureFollowsTheFourierSeries)
{
    // A slab at 300 K whose faces are held at 400 K from t = 0.
    const double length = 0.02, k = 1.0, c = 1.5e6, alpha = k / c;
    ConductionProblem p = slab(100, length, k);
    p.faces[4] = {FaceKind::Temperature, 400.0};
    p.faces[5] = {FaceKind::Temperature, 400.0};
    p.initialTemperature.assign(100, 300.0);
    TransientSettings t;
    t.duration = 30.0;
    t.steps = 3000;
    auto s = solveTransient(p, t);
    ASSERT_TRUE(s) << s.message();
    const auto exact = [&](double z) {
        double sum = 0.0;
        for (int n = 1; n < 200; n += 2)
            sum += 4.0 / (n * std::numbers::pi) * std::sin(n * std::numbers::pi * z / length) *
                   std::exp(-alpha * std::pow(n * std::numbers::pi / length, 2) * t.duration);
        return 400.0 - 100.0 * sum;
    };
    for (size_t cell : {10u, 25u, 50u, 75u})
        EXPECT_NEAR(s.value().temperature[cell], exact(centre(p, cell)), 0.5);  // of a 100 K change
    EXPECT_NEAR(s.value().balanceResidual, 0.0, 1e-6 * std::abs(s.value().storedEnergyChange));
}

TEST(HeatConduction, RefusesUndeterminedOrInvalidProblems)
{
    ConductionProblem p = slab(5, 0.01, 1.0);
    EXPECT_FALSE(solveSteady(p));  // every face adiabatic
    p.faces[4] = {FaceKind::Temperature, 300.0};
    p.conductivity[2] = -1.0;
    EXPECT_FALSE(solveSteady(p));
    p.conductivity[2] = 1.0;
    p.source = {1.0, 2.0};
    EXPECT_FALSE(solveSteady(p));
    p.source.clear();
    p.volumetricHeatCapacity.clear();
    EXPECT_FALSE(solveTransient(p, {}));
}

TEST(HeatConduction, ReadsTheThermalPropertiesOfAMaterial)
{
    Material m(Id::generate(), "Test");
    auto set = [&m](const char* key, double v) {
        PropertyValue pv;
        pv.value = v;
        pv.provenance = Provenance::Measured;
        ASSERT_TRUE(m.setProperty(PhysicsDomain::Thermal, key, pv).ok());
    };
    EXPECT_FALSE(thermalMaterial(m));
    set("density", 2000.0);
    set("thermalConductivity", 1.2);
    EXPECT_FALSE(thermalMaterial(m));  // no specific heat
    set("specificHeat", 900.0);
    auto t = thermalMaterial(m);
    ASSERT_TRUE(t) << t.message();
    EXPECT_DOUBLE_EQ(t.value().volumetricHeatCapacity(), 1.8e6);
    EXPECT_FALSE(t.value().expansionStated);
    set("thermalExpansion", 1e-5);
    EXPECT_DOUBLE_EQ(thermalMaterial(m).value().expansion, 1e-5);
}
