// Thermal contact resistance on internal planes (RES-FAM-009, -013): the
// conduction solver against the series formula, and the coupled engine's
// thermal participant with interfaces between regions.
#include "coupling/CoupledRun.h"
#include "materials/Material.h"
#include "multiphysics/PhysicalSystem.h"
#include "thermal/HeatConduction.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {

thermal::ConductionProblem twoLayers(double r)
{
    // 4 mm of steel (k = 50) on 2 mm of alumina (k = 30), 10 cells each; 400 K and 300 K faces.
    thermal::ConductionProblem p;
    p.grid.edges[0] = {-0.05, 0.05};
    p.grid.edges[1] = {-0.05, 0.05};
    for (int i = 0; i <= 20; ++i)
        p.grid.edges[2].push_back(i < 10 ? i * 0.4e-3 : 4e-3 + (i - 10) * 0.2e-3);
    for (int i = 0; i < 20; ++i)
        p.conductivity.push_back(i < 10 ? 50.0 : 30.0);
    p.faces[4] = {thermal::FaceKind::Temperature, 400.0, 0.0, 293.15};
    p.faces[5] = {thermal::FaceKind::Temperature, 300.0, 0.0, 293.15};
    if (r > 0.0) {
        p.contactResistance[2].assign(19, 0.0);
        p.contactResistance[2][9] = r;
    }
    return p;
}

}  // namespace

TEST(ThermalContact, SeriesResistanceAndTheTemperatureJump)
{
    const double r = 1e-4;  // m^2 K/W, of the order of a bonded joint
    auto bonded = thermal::solveSteady(twoLayers(0.0));
    auto contact = thermal::solveSteady(twoLayers(r));
    ASSERT_TRUE(bonded.ok() && contact.ok()) << contact.message();
    const double area = 0.01;
    const double perfect = 100.0 / (4e-3 / 50.0 + 2e-3 / 30.0) * area;
    const double withContact = 100.0 / (4e-3 / 50.0 + 2e-3 / 30.0 + r) * area;
    EXPECT_NEAR(bonded.value().faceHeatOut[5] / perfect, 1.0, 1e-10);
    EXPECT_NEAR(contact.value().faceHeatOut[5] / withContact, 1.0, 1e-10);
    // Across the interface the cell temperatures differ by q (w1 / 2 k1 + w2 / 2 k2 + R).
    const double q = withContact / area;
    const auto& t = contact.value().temperature;
    EXPECT_NEAR(t[9] - t[10], q * (0.2e-3 / 50.0 + 0.1e-3 / 30.0 + r), 1e-9);
    EXPECT_NEAR(contact.value().balanceResidual, 0.0, 1e-9);
    auto wrong = twoLayers(r);
    wrong.contactResistance[2].pop_back();
    EXPECT_FALSE(thermal::solveSteady(wrong).ok());
    auto negative = twoLayers(r);
    negative.contactResistance[2][3] = -1.0;
    EXPECT_FALSE(thermal::solveSteady(negative).ok());
}

TEST(ThermalContact, TheCoupledEngineReadsInterfacesBetweenRegions)
{
    MaterialLibrary lib;
    auto add = [&](const char* id, double k) {
        Material m(Id(id), id);
        PropertyValue v;
        v.provenance = Provenance::Illustrative;
        for (const auto& [key, value] : std::vector<std::pair<const char*, double>>{{"density", 3000.0}, {"thermalConductivity", k}, {"specificHeat", 500.0}, {"thermalExpansion", 1e-5}}) {
            v.value = value;
            ASSERT_TRUE(m.setProperty(PhysicsDomain::Thermal, key, v).ok());
        }
        ASSERT_TRUE(lib.add(m).ok());
    };
    add("mat-a", 50.0);
    add("mat-b", 30.0);
    const nlohmann::json doc{{"format", "resonant.physical-system"},
                             {"version", 1},
                             {"id", "sys"},
                             {"name", "Two layers"},
                             {"geometry", {{"kind", "layered-slab"}, {"lateralArea", 0.01}}},
                             {"environment", {{"ambientTemperature", 300.0}}},
                             {"regions",
                              {{{"id", "a"}, {"name", "A"}, {"materialId", "mat-a"}, {"thickness", 4e-3}, {"cells", 10}},
                               {{"id", "b"}, {"name", "B"}, {"materialId", "mat-b"}, {"thickness", 2e-3}, {"cells", 10}}}},
                             {"domains",
                              {{{"id", "heat"},
                                {"kind", "thermal"},
                                {"solver", "thermal.conduction.fv"},
                                {"settings",
                                 {{"faces", {{"-z", {{"kind", "temperature"}, {"temperature", 400.0}}}, {"+z", {{"kind", "temperature"}, {"temperature", 300.0}}}}},
                                  {"interfaces", {{{"regions", {"a", "b"}}, {"resistance", 1e-4}}}}}}}}},
                             {"couplings", nlohmann::json::array()},
                             {"materialLaws", nlohmann::json::array()},
                             {"scheme", {{"kind", "one-way"}}}};
    auto system = multiphysics::PhysicalSystem::fromJson(doc);
    ASSERT_TRUE(system.ok()) << system.message();
    auto result = coupling::runPhysicalSystem(system.value(), lib);
    ASSERT_TRUE(result.ok()) << result.message();
    ASSERT_TRUE(result.value().ok()) << result.value().message;
    const auto& t = result.value().fields.at("heat.temperature");
    const double q = 100.0 / (4e-3 / 50.0 + 2e-3 / 30.0 + 1e-4);
    EXPECT_NEAR(t.value(9) - t.value(10), q * (0.2e-3 / 50.0 + 0.1e-3 / 30.0 + 1e-4), 1e-6);
    // An interface between regions that do not touch is refused.
    nlohmann::json far = doc;
    far["domains"][0]["settings"]["interfaces"][0]["regions"] = {"a", "c"};
    auto bad = multiphysics::PhysicalSystem::fromJson(far);
    ASSERT_TRUE(bad.ok());
    auto refused = coupling::runPhysicalSystem(bad.value(), lib);
    EXPECT_TRUE(!refused.ok() || !refused.value().ok());
}
