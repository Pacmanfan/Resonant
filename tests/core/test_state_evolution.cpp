// Time-stepped evolution through the coupled engine (RES-DMS-002, -009, -010, -012):
// each step continues from the previous temperature field (uniform heating against its
// closed form), the constrained thermal stress follows the temperature at every step,
// temperature-dependent loss accelerates heating and the result converges with the time
// step, and the energy of the steps sums to the run's account.
#include "coupling/StateEvolution.h"
#include "multiphysics/PhysicalSystem.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using nlohmann::json;

namespace {

PropertyValue value(double v)
{
    PropertyValue p;
    p.value = v;
    p.provenance = Provenance::Measured;
    p.source = "test";
    return p;
}

MaterialLibrary library()
{
    Material m(Id("mat-slab"), "slab");
    for (auto [d, k, v] : std::vector<std::tuple<PhysicsDomain, const char*, double>>{
             {PhysicsDomain::Electromagnetic, "relativePermittivity", 4.0}, {PhysicsDomain::Electromagnetic, "lossTangent", 0.05},
             {PhysicsDomain::Electromagnetic, "conductivity", 0.0},        {PhysicsDomain::Electromagnetic, "relativePermeability", 1.0},
             {PhysicsDomain::Thermal, "thermalConductivity", 0.5},         {PhysicsDomain::Thermal, "density", 1500.0},
             {PhysicsDomain::Thermal, "specificHeat", 1000.0},             {PhysicsDomain::Thermal, "thermalExpansion", 5e-5},
             {PhysicsDomain::Mechanical, "density", 1500.0},               {PhysicsDomain::Mechanical, "youngsModulus", 3e9},
             {PhysicsDomain::Mechanical, "poissonsRatio", 0.35}})
        EXPECT_TRUE(m.setProperty(d, k, value(v)).ok());
    MaterialLibrary lib;
    EXPECT_TRUE(lib.add(m).ok());
    return lib;
}

json slab(const json& domains, const json& couplings, const json& laws = json::array(), const std::string& scheme = "one-way")
{
    return {{"format", "resonant.physical-system"}, {"version", 1}, {"id", "sys-test"}, {"name", "test slab"},
            {"geometry", {{"kind", "layered-slab"}, {"lateralArea", 0.01}}}, {"environment", {{"ambientTemperature", 293.15}}},
            {"regions", {{{"id", "slab"}, {"name", "Slab"}, {"materialId", "mat-slab"}, {"thickness", 0.01}, {"cells", 20}}}},
            {"domains", domains}, {"couplings", couplings}, {"materialLaws", laws},
            {"scheme", {{"kind", scheme}, {"maxIterations", 50}, {"temperatureTolerance", 1e-7}}}};
}

json heat(const json& faces, double source = 0.0)
{
    json settings{{"faces", faces}};
    if (source != 0.0)
        settings["volumetricSource"] = source;
    return {{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"}, {"settings", settings}};
}

json run(const json& system, double duration, int steps, int substeps = 4)
{
    auto sys = multiphysics::PhysicalSystem::fromJson(system);
    EXPECT_TRUE(sys) << sys.message();
    coupling::EvolutionSettings s;
    s.duration = duration;
    s.steps = steps;
    s.substeps = substeps;
    auto r = coupling::evolveSystem(sys.value(), library(), s, nullptr);
    EXPECT_TRUE(r) << r.message();
    return r ? r.value() : json::object();
}

}  // namespace

TEST(StateEvolution, EachStepContinuesFromThePreviousTemperature)
{
    // Uniform heating, adiabatic faces: T(t) = T0 + q t / (rho c), whatever the steps.
    const double q = 1e5, rhoC = 1500.0 * 1000.0;
    const json r = run(slab(json::array({heat(json::object(), q)}), json::array()), 600.0, 10, 1);
    ASSERT_EQ(r["history"].size(), 10u);
    EXPECT_TRUE(r["completed"].get<bool>());
    for (const json& s : r["history"]) {
        const double expected = 293.15 + q * s["time"].get<double>() / rhoC;
        for (double T : s["temperature"])
            EXPECT_NEAR(T, expected, 1e-6) << s["step"];
        EXPECT_TRUE(s["energy"]["balanced"].get<bool>());
    }
    // The run's energy: the source's energy is stored.
    const json& e = r["energy"];
    EXPECT_NEAR(e["externalInput"].get<double>(), q * 0.01 * 0.01 * 600.0, 1e-6 * q * 0.06);
    EXPECT_NEAR(e["stored"].get<double>(), e["externalInput"].get<double>(), 1e-6 * e["externalInput"].get<double>());
    EXPECT_TRUE(r["reliable"].get<bool>()) << r["reliability"].dump();
}

TEST(StateEvolution, ThermoMechanicalStressFollowsTheTemperature)
{
    const double q = 2e5, rhoC = 1.5e6, E = 3e9, nu = 0.35, alpha = 5e-5;
    const json solid{{"id", "solid"}, {"kind", "mechanical"}, {"solver", "mechanics.fe.solid-column"}, {"settings", {{"lateral", "constrained"}}}};
    // A negligible wave holds the slab's geometry: the deformation becomes its current thickness.
    const json probe{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
                     {"settings", {{"frequencyHz", 1e9}, {"incidentIntensity", 1e-9}}}};
    const json r = run(slab(json::array({heat(json::object(), q), solid, probe}),
                            json::array({{{"id", "expand"}, {"model", "thermal.expansion"}, {"from", "heat"}, {"to", "solid"}},
                                         {{"id", "deform"}, {"model", "mechanics.deformation-to-geometry"}, {"from", "solid"}, {"to", "em"}}}),
                            json::array(), "iterative"),
                       300.0, 6, 1);
    ASSERT_EQ(r["history"].size(), 6u) << r.dump().substr(0, 400);
    double previous = 0.0;
    for (const json& s : r["history"]) {
        const double dT = q * s["time"].get<double>() / rhoC;
        ASSERT_TRUE(s.contains("stressXX"));
        for (double sxx : s["stressXX"])
            EXPECT_NEAR(sxx, -E * alpha * dT / (1 - nu), 1e-6 * E * alpha * dT);
        // The slab thickens as it heats (constrained laterally: (1 + nu) / (1 - nu) alpha dT d).
        const double t = s["regions"]["slab"]["thickness"].get<double>();
        EXPECT_GT(t, previous);
        EXPECT_NEAR(t, 0.01 * (1 + alpha * dT * (1 + nu) / (1 - nu)), 1e-9 * 0.01);
        previous = t;
    }
}

TEST(StateEvolution, TemperatureDependentLossAcceleratesHeatingAndConvergesWithTheStep)
{
    const json em{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
                  {"settings", {{"frequencyHz", 2.45e9}, {"incidentIntensity", 5e4}}}};
    const json faces{{"-z", {{"kind", "convective"}, {"h", 10.0}}}, {"+z", {{"kind", "convective"}, {"h", 10.0}}}};
    const json couplings = json::array({{{"id", "absorb"}, {"model", "em.absorption-to-heat"}, {"from", "em"}, {"to", "heat"}},
                                        {{"id", "loss"}, {"model", "thermal.temperature-to-em-properties"}, {"from", "heat"}, {"to", "em"}}});
    const json laws = json::array({{{"region", "slab"}, {"property", "electromagnetic.lossTangent"}, {"law", "linear"}, {"value", 0.05},
                                    {"reference", 293.15}, {"slope", 5e-4}, {"validity", {250.0, 600.0}}, {"provenance", "illustrative"},
                                    {"source", "test law"}}});
    const json constant = run(slab(json::array({em, heat(faces)}), json::array({couplings[0]})), 120.0, 12);
    const json rising = run(slab(json::array({em, heat(faces)}), couplings, laws, "iterative"), 120.0, 12);
    ASSERT_TRUE(rising["completed"].get<bool>()) << rising["reliability"].dump();
    const auto meanT = [](const json& r) { return r["history"].back()["regions"]["slab"]["meanTemperatureK"].get<double>(); };
    EXPECT_GT(meanT(constant), 293.15 + 1.0);
    EXPECT_GT(meanT(rising) - 293.15, 1.05 * (meanT(constant) - 293.15));  // the loss grows with the temperature
    // The loss tangent recorded per step follows its law at the step's temperature.
    for (const json& s : rising["history"]) {
        const double T = s["regions"]["slab"]["meanTemperatureK"].get<double>();
        EXPECT_NEAR(s["regions"]["slab"]["properties"]["electromagnetic.lossTangent"]["value"].get<double>(), 0.05 + 5e-4 * (T - 293.15), 1e-12);
    }
    // Halving the step: the change halves too (first order between steps).
    const json half = run(slab(json::array({em, heat(faces)}), couplings, laws, "iterative"), 120.0, 24);
    const json quarter = run(slab(json::array({em, heat(faces)}), couplings, laws, "iterative"), 120.0, 48);
    const double d1 = std::abs(meanT(half) - meanT(rising)), d2 = std::abs(meanT(quarter) - meanT(half));
    EXPECT_LT(d2, 0.7 * d1);
    // The energy of the steps: input = out + stored (+ dissipated), per step and in total.
    const json& e = rising["energy"];
    EXPECT_NEAR(e["residual"].get<double>(), 0.0, 1e-6 * e["externalInput"].get<double>());
    EXPECT_FALSE(rising["committable"].empty());
}

TEST(StateEvolution, ASystemWithoutAThermalDomainIsRefused)
{
    auto sys = multiphysics::PhysicalSystem::fromJson(slab(json::array({{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
                                                                          {"settings", {{"frequencyHz", 2.45e9}, {"incidentIntensity", 1e3}}}}}),
                                                           json::array()));
    ASSERT_TRUE(sys);
    coupling::EvolutionSettings s;
    s.duration = 10.0;
    EXPECT_FALSE(coupling::evolveSystem(sys.value(), library(), s, nullptr));
}
