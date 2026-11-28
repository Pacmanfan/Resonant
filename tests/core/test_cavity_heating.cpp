// A resonant cavity in the Coupled Energy-Matter Engine (RES-RSN-011): a
// cavity driven at its cold resonance heats from its own losses, its fill's
// permittivity falls and its walls expand, so the resonance moves away from
// the drive and less power is absorbed. The engine's fixed point against an
// independent scalar fixed point, one-way against feedback, the ledger, the
// revisions and the refusals.
#include "coupling/CoupledRun.h"
#include "electromagnetics/CavityResonator.h"
#include "materials/Material.h"
#include "multiphysics/PhysicalSystem.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::multiphysics;
using nlohmann::json;

namespace {

constexpr double T0 = 293.15, slope = -5e-4, alpha = 1.65e-5, G = 0.05, Pinc = 1.0;

PropertyValue value(double v)
{
    PropertyValue p;
    p.value = v;
    p.provenance = Provenance::Measured;
    p.source = "test";
    return p;
}

MaterialLibrary materials()
{
    MaterialLibrary lib;
    Material fill(Id("mat-fill"), "Fill");
    EXPECT_TRUE(fill.setProperty(PhysicsDomain::Electromagnetic, "relativePermittivity", value(2.1)).ok());
    EXPECT_TRUE(fill.setProperty(PhysicsDomain::Electromagnetic, "lossTangent", value(1e-3)).ok());
    EXPECT_TRUE(fill.setProperty(PhysicsDomain::Electromagnetic, "relativePermeability", value(1.0)).ok());
    Material wall(Id("mat-wall"), "Copper");
    EXPECT_TRUE(wall.setProperty(PhysicsDomain::Electromagnetic, "relativePermittivity", value(1.0)).ok());
    EXPECT_TRUE(wall.setProperty(PhysicsDomain::Electromagnetic, "relativePermeability", value(1.0)).ok());
    EXPECT_TRUE(wall.setProperty(PhysicsDomain::Electromagnetic, "conductivity", value(5.8e7)).ok());
    EXPECT_TRUE(wall.setProperty(PhysicsDomain::Thermal, "thermalExpansion", value(alpha)).ok());
    EXPECT_TRUE(lib.add(fill).ok());
    EXPECT_TRUE(lib.add(wall).ok());
    return lib;
}

em::CavityResonatorSpec specAt(double T)
{
    em::CavityResonatorSpec s;
    const double scale = 1.0 + alpha * (T - T0);
    s.a = 0.02286 * scale;
    s.b = 0.01016 * scale;
    s.d = 0.030 * scale;
    s.fillPermittivity = 2.1 + slope * (T - T0);
    s.fillLossTangent = 1e-3;
    s.wallConductivity = 5.8e7;
    return s;
}

double coldResonance()
{
    return em::analyzeRectangularCavity(specAt(T0), 1).value()[0].frequencyHz;
}

json cavitySystem(const std::string& scheme, bool feedback = true)
{
    json couplings = {{{"id", "heat"}, {"model", "em.absorption-to-heat"}, {"from", "em"}, {"to", "thermal"}}};
    if (feedback) {
        couplings.push_back({{"id", "permittivity"}, {"model", "thermal.temperature-to-em-properties"}, {"from", "thermal"}, {"to", "em"}});
        couplings.push_back({{"id", "expansion"}, {"model", "thermal.uniform-expansion"}, {"from", "thermal"}, {"to", "em"}});
    }
    return {{"format", "resonant.physical-system"},
            {"version", 1},
            {"id", "sys-cavity"},
            {"name", "Heated cavity"},
            {"geometry", {{"kind", "rectangular-cavity"}}},
            {"environment", {{"ambientTemperature", T0}}},
            {"regions", {{{"id", "cavity"}, {"materialId", "mat-fill"}, {"dimensions", {{"a", 0.02286}, {"b", 0.01016}, {"d", 0.030}}}},
                         {{"id", "walls"}, {"materialId", "mat-wall"}}}},
            {"domains", {{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.cavity.mode"},
                          {"settings", {{"coupling", 1.0}, {"incidentPowerW", Pinc}, {"driveFrequencyHz", coldResonance()}}}},
                         {{"id", "thermal"}, {"kind", "thermal"}, {"solver", "thermal.lumped"}, {"settings", {{"conductanceWPerK", G}}}}}},
            {"couplings", couplings},
            {"materialLaws", {{{"region", "cavity"}, {"property", "electromagnetic.relativePermittivity"}, {"law", "linear"}, {"value", 2.1},
                               {"reference", T0}, {"slope", slope}, {"validity", {250.0, 400.0}}, {"provenance", "illustrative"}, {"source", "test"}}}},
            {"scheme", {{"kind", scheme}, {"maxIterations", 300}, {"temperatureTolerance", 1e-8}, {"geometryTolerance", 1e-12}, {"relaxation", 0.5}}}};
}

// The delivered power at temperature T for the drive at the cold resonance, the port fixed at the cold state.
double delivered(double T)
{
    const auto cold = em::analyzeRectangularCavity(specAt(T0), 1).value()[0];
    em::CavityResonatorSpec s = specAt(T);
    s.externalQ = cold.unloadedQ;  // critical coupling at the reference state
    const auto mode = em::analyzeRectangularCavity(s, 1).value()[0];
    return Pinc * (1.0 - std::norm(em::portReflection(mode, coldResonance())));
}

double fixedPoint()
{
    double lo = T0, hi = T0 + Pinc / G;  // g(T) = T - T0 - P(T)/G changes sign in [T0, T0 + Pinc/G]
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (mid - T0 - delivered(mid) / G < 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

double monitored(const CoupledResult& r, const std::string& participant, const std::string& key)
{
    return r.history.back().monitored.at(participant).at(key).get<double>();
}

}  // namespace

TEST(CavityHeating, FeedbackConvergesToTheLumpedFixedPoint)
{
    const MaterialLibrary lib = materials();
    auto sys = PhysicalSystem::fromJson(cavitySystem("iterative"));
    ASSERT_TRUE(sys) << sys.message();
    auto run = coupling::runPhysicalSystem(sys.value(), lib);
    ASSERT_TRUE(run) << run.message();
    const CoupledResult& r = run.value();
    ASSERT_EQ(r.status, RunStatus::Converged) << r.message;
    const double expected = fixedPoint();
    EXPECT_NEAR(monitored(r, "thermal", "temperatureK"), expected, 1e-5);
    EXPECT_NEAR(monitored(r, "em", "deliveredW"), delivered(expected), 1e-6);
    // The resonance moved up (the permittivity fell more than the walls grew) and away from the drive.
    const double shift = monitored(r, "em", "resonanceHz") - coldResonance();
    EXPECT_NEAR(shift, em::analyzeRectangularCavity(specAt(expected), 1).value()[0].frequencyHz - coldResonance(), 1.0);
    EXPECT_GT(shift, 0.0);
    EXPECT_LT(monitored(r, "em", "deliveredW"), 0.99 * Pinc);
    // The ledger: incident = reflected + heat to the ambient (steady), counted once.
    EXPECT_TRUE(r.audit.balanced) << r.audit.relativeResidual;
    EXPECT_LT(std::abs(r.audit.relativeResidual), 1e-6);
    EXPECT_GT(r.geometry["revision"].get<int>(), 0);
    EXPECT_NEAR(r.geometry["regions"]["cavity"]["a"]["current"].get<double>(), 0.02286 * (1 + alpha * (expected - T0)), 1e-12);
}

TEST(CavityHeating, OneWayOverestimatesTheHeatingThatFeedbackLimits)
{
    const MaterialLibrary lib = materials();
    auto oneWay = coupling::runPhysicalSystem(PhysicalSystem::fromJson(cavitySystem("one-way", false)).value(), lib);
    ASSERT_TRUE(oneWay) << oneWay.message();
    ASSERT_TRUE(oneWay.value().ok());
    // Without feedback the cavity absorbs everything at its cold resonance (critical coupling).
    EXPECT_NEAR(monitored(oneWay.value(), "em", "deliveredW"), Pinc, 1e-9);
    EXPECT_NEAR(monitored(oneWay.value(), "thermal", "temperatureK"), T0 + Pinc / G, 1e-9);
    auto feedback = coupling::runPhysicalSystem(PhysicalSystem::fromJson(cavitySystem("iterative")).value(), lib);
    ASSERT_TRUE(feedback);
    EXPECT_LT(monitored(feedback.value(), "thermal", "temperatureK"), monitored(oneWay.value(), "thermal", "temperatureK") - 1.0);
}

TEST(CavityHeating, InvalidCavitySystemsAreRefused)
{
    const MaterialLibrary lib = materials();
    json noWalls = cavitySystem("iterative");
    noWalls["regions"].erase(1);
    EXPECT_FALSE(coupling::runPhysicalSystem(PhysicalSystem::fromJson(noWalls).value(), lib));
    json noDims = cavitySystem("iterative");
    noDims["regions"][0].erase("dimensions");
    EXPECT_FALSE(coupling::runPhysicalSystem(PhysicalSystem::fromJson(noDims).value(), lib));
    json noConductance = cavitySystem("iterative");
    noConductance["domains"][1]["settings"] = json::object();
    auto failed = coupling::runPhysicalSystem(PhysicalSystem::fromJson(noConductance).value(), lib);
    ASSERT_TRUE(failed);
    EXPECT_EQ(failed.value().status, RunStatus::Failed);
    // The document keeps the dimensions through serialization.
    const auto sys = PhysicalSystem::fromJson(cavitySystem("iterative")).value();
    EXPECT_EQ(PhysicalSystem::fromJson(sys.toJson()).value().regions[0].dimensions.at("d"), 0.030);
}
