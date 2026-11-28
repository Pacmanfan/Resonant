// Architectures in the coupled engine and the laboratory (RES-FAM-013, -018):
// a stack as a physical system run by the engine against the series formula,
// graded layers as sublayers with laws, what the engine refuses, a contact
// resistance calibrated from measurements and validated on another set, and
// the architecture as a laboratory link with its revision.
#include "MatterFixture.h"

#include "coupling/CoupledRun.h"
#include "coupling/LaboratoryModels.h"
#include "coupling/LaboratoryStudies.h"
#include "coupling/MatterAnalyses.h"
#include "coupling/MatterSystems.h"
#include "laboratory/Laboratory.h"
#include "laboratory/ParameterEstimation.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

using namespace matter_test;
using namespace resonant::matter;
using resonant::coupling::stackToPhysicalSystem;

namespace {

const json kWallFaces{{"-z", {{"kind", "temperature"}, {"temperature", 900.0}}}, {"+z", {{"kind", "temperature"}, {"temperature", 1400.0}}}};

json thermalDomain(const json& faces)
{
    return json::array({{{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"}, {"settings", {{"faces", faces}}}}});
}

}  // namespace

TEST(MatterCoupling, TheBarrierStackRunsInTheCoupledEngine)
{
    auto p = example("c-thermal-barrier.resonant");
    const Architecture a = architecture(*p, "arch-tbc");
    auto sys = stackToPhysicalSystem(a, "st-wall", {{"domains", thermalDomain(kWallFaces)}, {"cellsPerLayer", 12}}, p->materials);
    ASSERT_TRUE(sys.ok()) << sys.message();
    const auto& s = sys.value().system;
    ASSERT_EQ(s.regions.size(), 3u);
    EXPECT_EQ(s.regions[0].objectId, "ex-fam-tbc");
    EXPECT_NEAR(s.lateralArea, 0.05 * 0.05, 1e-15);
    ASSERT_TRUE(s.domains[0].settings.contains("interfaces"));
    EXPECT_DOUBLE_EQ(s.domains[0].settings["interfaces"][0]["resistance"].get<double>(), 2e-5);
    auto run = resonant::coupling::runPhysicalSystem(s, p->materials);
    ASSERT_TRUE(run.ok()) << run.message();
    ASSERT_TRUE(run.value().ok()) << run.value().message;
    // The flux from the temperature gradient in the wall against the series formula.
    const auto& t = run.value().fields.at("heat.temperature");
    const double dz = 3e-3 / 12.0;
    const double q = 11.4 * (t.value(4) - t.value(3)) / dz;
    const double expected = 500.0 / (3e-3 / 11.4 + 1e-4 / 4.3 + 3e-4 / 1.0 + 2e-5);
    EXPECT_NEAR(q / expected, 1.0, 1e-6);
    // The jump at the bond coat - top coat interface (the cells next to it).
    const double jump = t.value(24) - t.value(23);
    EXPECT_NEAR(jump, expected * (1e-4 / 12.0 / (2.0 * 4.3) + 3e-4 / 12.0 / (2.0 * 1.0) + 2e-5), 1e-6 * jump);
}

TEST(MatterCoupling, GradedLayersBecomeSublayersWithLaws)
{
    auto p = example("d-graded-metal-ceramic.resonant");
    const Architecture a = architecture(*p, "arch-fgm");
    const json faces{{"-z", {{"kind", "temperature"}, {"temperature", 400.0}}}, {"+z", {{"kind", "temperature"}, {"temperature", 300.0}}}};
    auto sys = stackToPhysicalSystem(a, "st-fgm", {{"domains", thermalDomain(faces)}, {"sublayers", 40}, {"cellsPerLayer", 40}}, p->materials);
    ASSERT_TRUE(sys.ok()) << sys.message();
    const auto& s = sys.value().system;
    EXPECT_EQ(s.regions.size(), 40u);
    bool homogenized = false;
    for (const auto& law : s.laws)
        if (law.property == "thermal.thermalConductivity" && law.provenance == "homogenized")
            homogenized = true;
    EXPECT_TRUE(homogenized) << "the effective values carry their method";
    auto run = resonant::coupling::runPhysicalSystem(s, p->materials);
    ASSERT_TRUE(run.ok() && run.value().ok()) << (run.ok() ? run.value().message : run.message());
    // The engine's heat flux against the series integral of the graded conductivity.
    const auto series = resonant::coupling::analyzeStack(a, "st-fgm", "thermal", json::object(), p->materials);
    ASSERT_TRUE(series.ok());
    const double qExpected = 100.0 / series.value()["metrics"]["resistance"].get<double>();
    const auto& t = run.value().fields.at("heat.temperature");
    // Through the first sublayer (its conductivity is the law's).
    double k0 = 0.0;
    for (const auto& law : s.laws)
        if (law.region == s.regions[0].id && law.property == "thermal.thermalConductivity")
            k0 = law.value;
    ASSERT_GT(k0, 0.0);
    const double q = 2.0 * k0 * (400.0 - t.value(0)) / (2e-3 / 40.0);
    EXPECT_NEAR(q / qExpected, 1.0, 2e-3) << "40 sublayers: the discretization of the graded conductivity";
}

TEST(MatterCoupling, WhatTheEngineCannotRepresentIsRefused)
{
    auto p = example("c-thermal-barrier.resonant");
    Architecture a = architecture(*p, "arch-tbc");
    a.interfaceById("i-bond-top")->models = {{"cohesive-zone", {}}};
    auto refused = stackToPhysicalSystem(a, "st-wall", json::object(), p->materials);
    ASSERT_FALSE(refused.ok());
    EXPECT_NE(refused.message().find("no thermal model"), std::string::npos) << refused.message();
    a = architecture(*p, "arch-tbc");
    a.interfaceById("i-wall-bond")->models.push_back({"electrical-contact-resistance", {}});
    const json em = json::array({{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"}, {"settings", json::object()}},
                                 {{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"}, {"settings", {{"faces", kWallFaces}}}}});
    auto emRefused = stackToPhysicalSystem(a, "st-wall", {{"domains", em}}, p->materials);
    ASSERT_FALSE(emRefused.ok());
    a = architecture(*p, "arch-tbc");
    auto mech = stackToPhysicalSystem(a, "st-wall", {{"domains", json::array({{{"id", "m"}, {"kind", "mechanical"}, {"solver", "x"}}})}}, p->materials);
    EXPECT_FALSE(mech.ok());
    // The default domain is stated in the notes.
    auto defaults = stackToPhysicalSystem(a, "st-wall", json::object(), p->materials);
    ASSERT_TRUE(defaults.ok()) << defaults.message();
    EXPECT_NE(defaults.value().notes.dump().find("h = 10"), std::string::npos);
}

TEST(MatterCoupling, InterfaceResistanceCalibratedFromMeasurements)
{
    // Synthetic "measurements" of the temperature drop across the coated wall at four heat
    // fluxes, made with a contact resistance of 5e-5 m^2 K/W (the record says 2e-5) and 0.2 K noise.
    auto p = example("c-thermal-barrier.resonant");
    const json cfg{{"architecture", "arch-tbc"}, {"stack", "st-wall"}, {"analysis", "thermal"}, {"metric", "temperatureDrop"}, {"axisOption", "heatFlux"}};
    auto params = resonant::coupling::laboratoryModelParameters("matter.stack-metric", cfg, *p);
    ASSERT_TRUE(params.ok()) << params.message();
    const std::string key = "interface.i-bond-top.thermal-contact-resistance.resistance";
    bool listed = false;
    for (const auto& q : params.value())
        listed = listed || q.id == key;
    ASSERT_TRUE(listed);
    const std::vector<double> fluxes{1e5, 2e5, 3e5, 4e5};
    auto truth = resonant::coupling::predictLaboratoryModel("matter.stack-metric", cfg, {{key, 5e-5}}, fluxes, *p, nullptr);
    ASSERT_TRUE(truth.ok()) << truth.message();
    std::mt19937 rng(11);
    std::normal_distribution<double> noise(0.0, 0.2);
    auto observe = [&](unsigned seed) {
        rng.seed(seed);
        resonant::laboratory::Observation o;
        o.dataset = "synthetic-" + std::to_string(seed);
        o.axis = fluxes;
        for (const auto& v : truth.value().values) {
            o.values.push_back(v.real() + noise(rng));
            o.standard.push_back(0.2);
        }
        return o;
    };
    const auto calibration = observe(11), validation = observe(12);
    resonant::laboratory::EstimatedParameter rc{key, "contact resistance", "m^2 K/W", "", 2e-5, 0.0, 1e-3, std::nullopt, std::nullopt};
    auto predictor = [&](const std::vector<double>& x, size_t) -> resonant::Result<std::vector<double>> {
        auto pr = resonant::coupling::predictLaboratoryModel("matter.stack-metric", cfg, {{key, x[0]}}, fluxes, *p, nullptr);
        if (!pr)
            return resonant::Result<std::vector<double>>::failure(pr.message());
        std::vector<double> out;
        for (const auto& v : pr.value().values)
            out.push_back(v.real());
        return out;
    };
    auto fit = resonant::laboratory::estimateParameters({rc}, {calibration}, predictor);
    ASSERT_TRUE(fit.ok()) << fit.message();
    const auto& est = fit.value().parameters[0];
    EXPECT_EQ(est.identifiability, "identified");
    EXPECT_NEAR(est.estimate, 5e-5, 4.0 * est.standardError);
    EXPECT_GT(est.lower, 2e-5) << "the record's value lies outside the interval: the measurement says otherwise";
    // Validation on the other set: the calibrated prediction lies within the noise.
    auto predicted = predictor({est.estimate}, 0);
    ASSERT_TRUE(predicted.ok());
    for (size_t i = 0; i < fluxes.size(); ++i)
        EXPECT_NEAR(predicted.value()[i], validation.values[i], 3.0 * 0.2 + 3.0 * est.standardError * fluxes[i]);
}

TEST(MatterCoupling, TheArchitectureIsALaboratoryLinkWithItsRevision)
{
    auto p = example("c-thermal-barrier.resonant");
    const auto& kinds = resonant::laboratory::modelLinkKinds();
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), "matter-architecture"), kinds.end());
    resonant::laboratory::ModelLink link;
    link.kind = "matter-architecture";
    link.id = "arch-tbc";
    const std::string before = resonant::coupling::currentRevision(*p, link);
    EXPECT_FALSE(before.empty());
    Architecture a = architecture(*p, "arch-tbc");
    a.stack("st-wall")->layers[2].thickness = 4e-4;
    put(*p, a);
    EXPECT_NE(resonant::coupling::currentRevision(*p, link), before);
    link.id = "arch-none";
    EXPECT_TRUE(resonant::coupling::currentRevision(*p, link).empty());
}
