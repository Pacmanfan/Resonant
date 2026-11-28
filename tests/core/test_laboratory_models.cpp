// The laboratory's physical models (RES-LAB-011): each prediction is the existing
// solver's - the lumped resonator, layered electromagnetics, the J2 material point, the
// coupled evolution, a functional analysis, a material property, a design revision.
#include "LaboratoryFixture.h"

#include "coupling/LaboratoryModels.h"
#include "coupling/StateEvolution.h"
#include "electromagnetics/LayeredMedia.h"
#include "mechanics/Plasticity.h"
#include "multiphysics/PhysicalSystem.h"
#include "optimization/Persistence.h"

#include <cmath>

using namespace laboratory_test;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST(LaboratoryModels, ResonatorLayeredAndJ2PredictionsAreTheSolvers)
{
    Project empty;
    auto r = coupling::predictLaboratoryModel("resonator.lumped", {{"response", "receptance"}}, {{"mass", 0.5}, {"stiffness", 2e5}, {"damping", 6.0}},
                                              {50.0, 100.0}, empty, nullptr);
    ASSERT_TRUE(r) << r.message();
    const double w = 2 * kPi * 100.0;
    EXPECT_NEAR(std::abs(r.value().values[1]), 1.0 / std::hypot(2e5 - 0.5 * w * w, 6.0 * w), 1e-12);
    EXPECT_EQ(r.value().quantity.quantity, experiments::Quantity::Displacement);
    auto mob = coupling::predictLaboratoryModel("resonator.lumped", {{"response", "mobility"}}, {{"mass", 0.5}, {"stiffness", 2e5}, {"damping", 6.0}}, {100.0},
                                                empty, nullptr);
    EXPECT_NEAR(std::abs(mob.value().values[0]), w * std::abs(r.value().values[1]), 1e-9);
    EXPECT_FALSE(coupling::predictLaboratoryModel("resonator.lumped", {{"response", "magic"}}, {}, {100.0}, empty, nullptr));
    // Layered: the transfer-matrix solution itself.
    const json cfg{{"layers", {{{"name", "sheet"}, {"thickness", 0.005}, {"permittivity", 4.2}, {"lossTangent", 0.02}}}}};
    auto s = coupling::predictLaboratoryModel("em.layered-reflection", cfg, {{"sheet.permittivity", 3.0}}, {5e9}, empty, nullptr);
    ASSERT_TRUE(s);
    em::LayeredProblem p;
    p.frequencyHz = 5e9;
    em::MediumLayer l;
    l.name = "sheet";
    l.thickness = 0.005;
    l.permittivityReal = 3.0;
    l.lossTangent = 0.02;
    p.layers = {l};
    EXPECT_NEAR(std::abs(s.value().values[0] - em::LayeredSolution::solve(p).value().reflection()), 0.0, 1e-14);
    // J2: the uniaxial driver along the path.
    const std::vector<double> path{0.0005, 0.001, 0.002, 0.004};
    auto j = coupling::predictLaboratoryModel("material-state.j2-uniaxial", json::object(), {{"yieldStress", 250e6}, {"isotropicHardening", 2e9}}, path, empty,
                                              nullptr);
    ASSERT_TRUE(j);
    EXPECT_NEAR(j.value().values[0].real(), 200e9 * 0.0005, 1e-3);
    const double et = 200e9 * 2e9 / (200e9 + 2e9);
    EXPECT_NEAR(j.value().values[3].real(), 250e6 + et * (0.004 - 250e6 / 200e9), 1e3);
    auto params = coupling::laboratoryModelParameters("em.layered-reflection", cfg, empty);
    ASSERT_TRUE(params);
    EXPECT_EQ(params.value().size(), 3u);
    EXPECT_FALSE(coupling::laboratoryModelParameters("no.such.model", json::object(), empty));
}

TEST(LaboratoryModels, CoupledEvolutionFunctionalMetricPropertyAndRevision)
{
    Fixture f("c-thermal-slab.resonant");
    const json cfg{{"system", "sys-lab-slab"}, {"position", 0.02}, {"duration", 300.0}, {"steps", 10}, {"substeps", 4}};
    auto p = coupling::predictLaboratoryModel("thermal.evolution-probe", cfg, {}, {0.0, 150.0, 300.0}, f.host.project(), nullptr);
    ASSERT_TRUE(p) << p.message();
    EXPECT_NEAR(p.value().values[0].real(), 293.15, 1e-9);
    // The evolution itself, at the probe.
    auto sys = multiphysics::PhysicalSystem::fromJson(f.host.project().extensions["multiphysics"]["systems"][0]);
    coupling::EvolutionSettings st;
    st.duration = 300.0;
    st.steps = 10;
    st.substeps = 4;
    auto doc = coupling::evolveSystem(sys.value(), f.host.project().materials, st, nullptr);
    const auto z = doc.value()["positions"].get<std::vector<double>>();
    const auto T = doc.value()["history"].back()["temperature"].get<std::vector<double>>();
    const size_t mid = z.size() / 2;
    const double atMid = T[mid - 1] + (T[mid] - T[mid - 1]) * (0.02 - z[mid - 1]) / (z[mid] - z[mid - 1]);
    EXPECT_NEAR(p.value().values[2].real(), atMid, 1e-9);
    // A higher conductivity flattens the profile: a lower mid-plane temperature.
    auto k = coupling::predictLaboratoryModel("thermal.evolution-probe", cfg, {{"mat-ex-lab-polymer.thermal.thermalConductivity", 2.0}}, {300.0}, f.host.project(),
                                              nullptr);
    ASSERT_TRUE(k);
    EXPECT_LT(k.value().values[0].real(), p.value().values[2].real());
    // A material property, with its provenance.
    auto prop = coupling::predictLaboratoryModel("material.property", {{"material", "mat-ex-lab-polymer"}, {"domain", "thermal"}, {"key", "thermalConductivity"}},
                                                 {}, {}, f.host.project(), nullptr);
    ASSERT_TRUE(prop);
    EXPECT_DOUBLE_EQ(prop.value().values[0].real(), 0.40);
    EXPECT_EQ(prop.value().details["provenance"], "literature");
    // A functional structure's metric.
    Fixture h("a-helmholtz-resonator.resonant", "resonance");
    auto f0 = coupling::predictLaboratoryModel("functional.metric", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.f0"}}, {}, {}, h.host.project(), nullptr);
    ASSERT_TRUE(f0) << f0.message();
    EXPECT_GT(f0.value().values[0].real(), 10.0);
    EXPECT_EQ(f0.value().unit, "Hz");
    auto smaller = coupling::predictLaboratoryModel("functional.metric", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.f0"}},
                                                    {{"neckRadius", 0.0123}}, {}, h.host.project(), nullptr);
    ASSERT_TRUE(smaller) << smaller.message();
    EXPECT_NE(smaller.value().values[0].real(), f0.value().values[0].real());
    // A design revision's evaluated quantity.
    optimization::OptimizationWorkspace w;
    optimization::RunRecord run;
    run.runId = "run-1";
    optimization::Candidate c;
    c.id = "cand-7";
    c.evaluation.quantities["resonance.frequency"] = 412.0;
    c.evaluation.model.modelId = "mechanics.cantilever";
    run.candidates.push_back(c);
    w.runs.push_back(run);
    optimization::DesignRevision rev;
    rev.id = "rev-1";
    rev.candidateId = "cand-7";
    rev.runId = "run-1";
    w.revisions.push_back(rev);
    optimization::storeWorkspace(h.host.project(), w);
    auto metric = coupling::predictLaboratoryModel("design-revision.metric", {{"revision", "rev-1"}, {"quantity", "resonance.frequency"}}, {}, {},
                                                   h.host.project(), nullptr);
    ASSERT_TRUE(metric) << metric.message();
    EXPECT_DOUBLE_EQ(metric.value().values[0].real(), 412.0);
    EXPECT_FALSE(coupling::predictLaboratoryModel("design-revision.metric", {{"revision", "rev-9"}}, {}, {}, h.host.project(), nullptr));
}
