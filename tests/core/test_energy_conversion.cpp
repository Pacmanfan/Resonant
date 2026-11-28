// Energy-conversion mechanisms (REQ-CHAT-20261128-005, RES-CONV-001, -003..005, ADR-000233):
// stages and chains through the accounting framework; photovoltaic, thermoelectric and
// magnetostrictive models against closed forms and independent identities.

#include "conversion/ConversionStage.h"
#include "conversion/Photovoltaic.h"
#include "mechanics/Magnetostrictive.h"
#include "thermal/Thermoelectric.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace resonant;
using namespace resonant::conversion;

namespace {

constexpr double kQ = 1.602176634e-19, kK = 1.380649e-23;

PhotovoltaicCell siliconLike()
{
    PhotovoltaicCell c;
    c.name = "silicon-like test cell";
    c.area = 0.01;
    c.bandGap = 1.12;
    c.absorptionAboveGap = 1e5;
    c.thickness = 180e-6;
    c.reflectance = 0.05;
    c.collection = 0.95;
    c.saturationCurrent = 1e-10;
    c.idealityFactor = 1.0;
    c.seriesResistance = 0.005;
    c.shuntResistance = 50.0;
    c.provenance = "illustrative equivalent-circuit values";
    return c;
}

thermal::ThermoelectricCouple bismuthTellurideLike(bool temperatureDependent)
{
    thermal::ThermoelectricCouple c;
    const auto leg = [&](const char* name, double sign) {
        thermal::ThermoelectricLeg l;
        l.name = name;
        l.length = 2e-3;
        l.area = 4e-6;
        if (temperatureDependent) {
            l.seebeck = {{sign * 1.4e-4, sign * 2e-7}};      // alpha = +-(2e-4 + 2e-7 (T - 300))
            l.resistivity = {{0.4e-5, 2e-8}};                // rho = 1e-5 + 2e-8 (T - 300)
            l.conductivity = {{0.9, 0.002}};                 // kappa = 1.5 + 0.002 (T - 300)
        } else {
            l.seebeck = thermal::TemperatureFunction::constant(sign * 2e-4);
            l.resistivity = thermal::TemperatureFunction::constant(1e-5);
            l.conductivity = thermal::TemperatureFunction::constant(1.5);
        }
        l.provenance = "illustrative bismuth-telluride-order values";
        return l;
    };
    c.p = leg("p", +1);
    c.n = leg("n", -1);
    return c;
}

}  // namespace

// --- stages and chains --------------------------------------------------------------------

TEST(EnergyConversion, AChainIsCheckedStageByStageAndAtEveryJoin)
{
    ConversionChain chain;
    chain.id = "test";
    ConversionStage a;
    a.id = "a";
    a.model = "test";
    a.inputForm = EnergyForm::Acoustic;
    a.outputForm = EnergyForm::Mechanical;
    a.input = 10.0;
    a.output = 6.0;
    a.otherOutputs = {{"reflected", EnergyForm::Acoustic, 4.0}};
    ConversionStage b;
    b.id = "b";
    b.model = "test";
    b.inputForm = EnergyForm::Mechanical;
    b.outputForm = EnergyForm::Electrical;
    b.input = 6.0;
    b.output = 2.0;
    b.dissipated = 4.0;
    chain.stages = {a, b};
    ChainReport r = evaluateChain(chain);
    EXPECT_EQ(r.status, accounting::CheckStatus::Verified) << r.toJson().dump(2);
    EXPECT_DOUBLE_EQ(r.overallEfficiency, 0.2);
    ASSERT_EQ(r.links.size(), 1u);
    EXPECT_TRUE(r.links[0].joined);
    // A stage that loses energy without saying where is exceeded, never balanced quietly.
    chain.stages[1].dissipated = 3.0;
    EXPECT_EQ(evaluateChain(chain).status, accounting::CheckStatus::Exceeded);
    // Stages that do not join are reported at the join.
    chain.stages[1].dissipated = 4.0;
    chain.stages[1].input = 5.0;
    chain.stages[1].dissipated = 3.0;
    r = evaluateChain(chain);
    EXPECT_FALSE(r.links[0].joined);
    EXPECT_EQ(r.status, accounting::CheckStatus::Exceeded);
    // A stage whose model leaves a contribution out is insufficient data, whatever its residual.
    chain.stages[1].input = 6.0;
    chain.stages[1].dissipated = 4.0;
    chain.stages[0].missing = {"the scattered wave"};
    EXPECT_EQ(evaluateChain(chain).status, accounting::CheckStatus::InsufficientData);
}

// --- photovoltaic -------------------------------------------------------------------------------

TEST(EnergyConversion, LambertWSolvesItsDefiningEquation)
{
    EXPECT_NEAR(lambertW0(1.0), 0.5671432904097838, 1e-15);
    EXPECT_NEAR(lambertW0(std::exp(1.0)), 1.0, 1e-14);
    EXPECT_NEAR(lambertW0(-0.2), -0.25917110181907377, 1e-14);
    const double w = lambertW0OfExp(400.0);  // W(e^400) without overflow
    EXPECT_NEAR(w + std::log(w), 400.0, 1e-12);
}

TEST(EnergyConversion, TheDiodeEquationAgreesWithItsClosedForms)
{
    const Spectrum sun = blackBodySpectrum(5778.0, 1000.0);
    EXPECT_NEAR(sun.total(), 1000.0, 1e-9);
    PhotovoltaicCell c = siliconLike();
    const PhotovoltaicState st = cellState(c, sun, 298.15);
    // Newton and the Lambert W closed form, independently.
    for (int i = 0; i <= 20; ++i) {
        const double v = 0.7 * i / 20.0;
        EXPECT_NEAR(cellCurrent(c, st, v), cellCurrentLambertW(c, st, v), 1e-9 * st.photocurrent) << "V = " << v;
    }
    // Without a shunt the open-circuit voltage is a ln(I_ph / I_0 + 1), whatever R_s.
    c.shuntResistance = std::numeric_limits<double>::infinity();
    auto open = solvePhotovoltaic(c, sun, 298.15);
    ASSERT_TRUE(open.ok()) << open.message();
    const double a = kK * 298.15 / kQ;
    EXPECT_NEAR(open.value().openCircuitVoltage, a * std::log1p(open.value().state.photocurrent / open.value().state.saturationCurrent), 1e-9);
    // Without a series resistance the short-circuit current is the photocurrent.
    c.seriesResistance = 0.0;
    auto ideal = solvePhotovoltaic(c, sun, 298.15);
    ASSERT_TRUE(ideal.ok()) << ideal.message();
    EXPECT_NEAR(ideal.value().shortCircuitCurrent, ideal.value().state.photocurrent, 1e-12 * ideal.value().state.photocurrent);
    // The ideal fill factor: Green's empirical relation (within 0.5 %).
    const double voc = ideal.value().openCircuitVoltage / a;
    const double green = (voc - std::log(voc + 0.72)) / (voc + 1);
    EXPECT_NEAR(ideal.value().fillFactor, green, 0.005 * green);
}

TEST(EnergyConversion, TheIncidentLightIsAccountedForTermByTerm)
{
    const Spectrum sun = blackBodySpectrum(5778.0, 1000.0);
    auto r = solvePhotovoltaic(siliconLike(), sun, 298.15);
    ASSERT_TRUE(r.ok()) << r.message();
    const PhotovoltaicResult& p = r.value();
    // The circuit's own identity: I_ph V_j = diode + shunt + series + output.
    EXPECT_NEAR(p.circuitResidual, 0.0, 1e-9 * p.maximumPower.power);
    // Optics, absorption and circuit together close the stage.
    EXPECT_NEAR(p.stage.residual(), 0.0, 1e-9 * p.incident);
    EXPECT_NEAR(p.incident, 1000.0 * 0.01, 1e-9);
    EXPECT_GT(p.thermalization, 0.0);
    EXPECT_GT(p.transmitted, 0.0);  // the light below the gap
    EXPECT_GT(p.efficiency, 0.05);
    EXPECT_LT(p.efficiency, 0.337);  // below the single-junction detailed-balance limit
    ConversionChain chain;
    chain.stages = {p.stage};
    EXPECT_EQ(evaluateChain(chain).status, accounting::CheckStatus::Verified);
    EXPECT_FALSE(p.stage.validity.empty());
}

TEST(EnergyConversion, AWarmerCellLosesOpenCircuitVoltage)
{
    const Spectrum sun = blackBodySpectrum(5778.0, 1000.0);
    auto cool = solvePhotovoltaic(siliconLike(), sun, 298.15);
    auto warm = solvePhotovoltaic(siliconLike(), sun, 308.15);
    ASSERT_TRUE(cool.ok() && warm.ok());
    const double slope = (warm.value().openCircuitVoltage - cool.value().openCircuitVoltage) / 10.0;
    EXPECT_LT(slope, -1.5e-3);  // silicon's open-circuit voltage falls by about 2 mV/K
    EXPECT_GT(slope, -3.0e-3);
    EXPECT_LT(warm.value().state.bandGap, cool.value().state.bandGap);
    // No light above the gap: refused, not a zero result.
    PhotovoltaicCell wide = siliconLike();
    wide.bandGap = 6.0;
    EXPECT_FALSE(solvePhotovoltaic(wide, sun, 298.15).ok());
}

// --- thermoelectric ------------------------------------------------------------------------------

TEST(EnergyConversion, AThermoelectricGeneratorMatchesTheConstantPropertyClosedForm)
{
    const auto couple = bismuthTellurideLike(false);
    const double th = 400, tc = 300, alpha = 4e-4, r = 0.01, k = 6e-3;
    const double zt = alpha * alpha / (r * k) * 0.5 * (th + tc);
    for (const double load : {0.005, 0.01, 0.01 * std::sqrt(1 + zt), 0.05}) {
        thermal::ThermoelectricOperation op;
        op.hotTemperature = th;
        op.coldTemperature = tc;
        op.loadResistance = load;
        auto res = thermal::solveThermoelectric(couple, thermal::ThermoelectricMode::Generator, op);
        ASSERT_TRUE(res.ok()) << res.message();
        const auto& g = res.value();
        const double i = alpha * (th - tc) / (r + load);
        EXPECT_NEAR(g.current, i, 1e-9 * i);
        EXPECT_NEAR(g.heatHot, alpha * th * i + k * (th - tc) - 0.5 * i * i * r, 1e-9 * g.heatHot);
        EXPECT_NEAR(g.heatCold, alpha * tc * i + k * (th - tc) + 0.5 * i * i * r, 1e-9 * g.heatCold);
        EXPECT_NEAR(g.electricalPower, i * i * load, 1e-9 * i * i * load);
        EXPECT_NEAR(g.energyResidual, 0.0, 1e-9 * g.heatHot);
        EXPECT_GE(g.entropyGeneration, 0.0);
        if (std::abs(load - 0.01 * std::sqrt(1 + zt)) < 1e-12)
            EXPECT_NEAR(g.efficiency, thermal::maximumGeneratorEfficiency(zt, th, tc), 1e-9);
    }
}

TEST(EnergyConversion, AThermoelectricCoolerReachesHalfZTSquaredWithoutLoad)
{
    const auto couple = bismuthTellurideLike(false);
    const double alpha = 4e-4, r = 0.01, k = 6e-3, tc = 300;
    const double z = alpha * alpha / (r * k);
    thermal::ThermoelectricOperation op;
    op.coldTemperature = tc;
    op.hotTemperature = tc + 0.5 * z * tc * tc;  // the largest temperature difference
    op.current = alpha * tc / r;                 // at the optimum current
    auto res = thermal::solveThermoelectric(couple, thermal::ThermoelectricMode::Cooler, op);
    ASSERT_TRUE(res.ok()) << res.message();
    EXPECT_NEAR(res.value().heatCold, 0.0, 1e-9 * alpha * tc * *op.current);  // nothing left to pump
    // At a smaller difference it pumps heat, with its coefficient of performance.
    op.hotTemperature = tc + 20;
    op.current = 3.0;
    auto cool = thermal::solveThermoelectric(couple, thermal::ThermoelectricMode::Cooler, op);
    ASSERT_TRUE(cool.ok()) << cool.message();
    const double i = 3.0, qc = alpha * tc * i - 0.5 * i * i * r - k * 20;
    EXPECT_NEAR(cool.value().heatCold, qc, 1e-9 * std::abs(qc));
    EXPECT_NEAR(cool.value().cop, qc / (i * i * r + alpha * 20 * i), 1e-9);
    EXPECT_GE(cool.value().entropyGeneration, 0.0);
    ConversionChain chain;
    chain.stages = {cool.value().stage};
    EXPECT_EQ(evaluateChain(chain).status, accounting::CheckStatus::Verified);
}

TEST(EnergyConversion, TemperatureDependentLegsConserveEnergyAndConverge)
{
    const auto couple = bismuthTellurideLike(true);
    double previous = 0.0;
    for (const int nodes : {51, 101, 201, 401}) {
        thermal::ThermoelectricOperation op;
        op.hotTemperature = 450;
        op.coldTemperature = 300;
        op.loadResistance = 0.02;
        op.nodes = nodes;
        auto res = thermal::solveThermoelectric(couple, thermal::ThermoelectricMode::Generator, op);
        ASSERT_TRUE(res.ok()) << res.message();
        const auto& g = res.value();
        const double relative = std::abs(g.energyResidual) / g.heatHot;
        EXPECT_LT(relative, 1e-3) << nodes;
        if (previous > 0.0)
            EXPECT_LT(relative, 0.3 * previous) << "second order: the residual falls about fourfold per halving";
        previous = relative;
        EXPECT_GT(std::abs(g.thomson), 1e-4 * g.electricalPower);  // alpha varies: a Thomson heat
        EXPECT_GE(g.entropyGeneration, 0.0);
        // The open-circuit voltage is the integral of alpha_p - alpha_n.
        EXPECT_NEAR(g.openCircuitVoltage, 2 * (1.4e-4 * 150 + 1e-7 * (450 * 450 - 300 * 300)), 1e-12);
    }
}

// --- magnetostrictive --------------------------------------------------------------------------------

TEST(EnergyConversion, AMagnetostrictiveRodIsLinearAboutItsBias)
{
    mechanics::MagnetostrictiveRod rod;
    rod.material = mechanics::PiezomagneticConstants::terfenolD();
    rod.length = 0.05;
    rod.area = 1e-4;
    rod.turns = 500;
    rod.coilResistance = 2.0;
    rod.tipMass = 0.2;
    rod.dampingRatio = 0.02;
    auto m = mechanics::magnetostrictiveModel(rod);
    ASSERT_TRUE(m.ok()) << m.message();
    const double k2 = rod.material.couplingSquared();
    EXPECT_NEAR(m.value().couplingSquared, k2, 1e-15);
    // An ideal shorted coil holds the flux: the stiffness at constant B, k^H / (1 - k^2).
    EXPECT_NEAR(m.value().shortedIdealHz, m.value().openCoilHz / std::sqrt(1 - k2), 1e-9 * m.value().openCoilHz);
    // The actuator: the free elongation d33 N I, held by the blocked force k^H delta.
    const auto act = mechanics::magnetostrictiveActuator(rod, 0.5);
    EXPECT_NEAR(act.freeElongation, rod.material.d33 * rod.turns * 0.5, 1e-18);
    EXPECT_NEAR(act.blockedForce, m.value().stiffnessH * act.freeElongation, 1e-9 * act.blockedForce);
    EXPECT_TRUE(act.withinLinearRange);
    EXPECT_FALSE(mechanics::magnetostrictiveActuator(rod, 5.0).withinLinearRange);
    // The harvester's balance: base work = load + coil + damping, at every point.
    double best = 0.0, bestLoad = 0.0;
    for (const double load : {1.0, 10.0, 100.0, 1000.0, 10000.0}) {
        const auto p = mechanics::magnetostrictiveHarvester(rod, m.value(), m.value().openCoilHz, load, 1.0);
        EXPECT_NEAR(p.residual(), 0.0, 1e-12 * p.inputPower) << load;
        if (p.loadPower > best)
            best = p.loadPower, bestLoad = load;
    }
    EXPECT_GT(bestLoad, 1.0);
    EXPECT_LT(bestLoad, 10000.0);  // an interior optimum load
    // Inconsistent constants (k^2 >= 1) are refused.
    rod.material.d33 = 1e-7;
    EXPECT_FALSE(mechanics::magnetostrictiveModel(rod).ok());
}
