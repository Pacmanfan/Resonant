// The laboratory across the architecture (RES-LAB-013, -014; benchmarks 24.5, 24.6):
// a thermal run against the coupled engine, calibrated and applied as a new material
// revision; a measured resonance against a functional structure; measured S-parameters
// retrieved into effective properties within (and outside) their homogenization regime;
// a measured property against a derived one; a design revision against its measured
// performance; an experimental energy balance with a missing pathway.
#include "LaboratoryFixture.h"

#include "coupling/LaboratoryModels.h"
#include "electromagnetics/LayeredMedia.h"
#include "optimization/Persistence.h"

#include <cmath>
#include <fstream>
#include <sstream>

using namespace laboratory_test;

namespace {

json importText(Fixture& f, const std::string& name, const std::string& text)
{
    auto r = f.s.laboratory.importDataset(f.local(), {{"content", text}, {"fileName", name}});
    EXPECT_TRUE(r) << (r ? "" : r.error().message);
    return r ? r.value().result : json::object();
}

}  // namespace

TEST(LaboratoryIntegration, ThermalRunThroughTheCoupledEngine)
{
    Fixture f("c-thermal-slab.resonant");
    const std::string run1 = Fixture::datasetOf(f.import("data/c-thermal-run1.csv", "st-thermal", "calibration"), "temperature");
    const std::string run2 = Fixture::datasetOf(f.import("data/c-thermal-run2.csv", "st-thermal", "validation"), "temperature");
    const json cfg{{"system", "sys-lab-slab"}, {"position", 0.02}, {"duration", 1800.0}, {"steps", 30}, {"substeps", 4}};
    // The handbook conductivity does not explain the run.
    auto before = f.s.laboratory.compare(f.local(), {{"id", "cmp-nominal"}, {"study", "st-thermal"}, {"kind", "transient"}, {"measured", run1},
                                                     {"predicted", {{"model", "thermal.evolution-probe"}, {"configuration", cfg}}}});
    ASSERT_TRUE(before) << before.error().message;
    EXPECT_NE(before.value().result["verdict"].get<std::string>().find("inconsistent"), std::string::npos);
    const json cal = f.calibrate({{"id", "cal-k"}, {"study", "st-thermal"}, {"model", "thermal.evolution-probe"}, {"configuration", cfg},
                                  {"parameters", {{{"id", "mat-ex-lab-polymer.thermal.thermalConductivity"}, {"lower", 0.05}, {"upper", 5.0}}}},
                                  {"datasets", {run1}}, {"observable", "value"}});
    ASSERT_EQ(cal["status"], "calibrated") << cal.dump().substr(0, 600);
    const json* k = parameter(cal, "mat-ex-lab-polymer.thermal.thermalConductivity");
    EXPECT_LE((*k)["lower"].get<double>(), 0.50);
    EXPECT_GE((*k)["upper"].get<double>(), 0.50);
    auto val = f.s.laboratory.validate(f.local(), "cal-k", {run2}, "val-k");
    ASSERT_TRUE(val) << val.error().message;
    EXPECT_EQ(val.value().result["status"], "validated within uncertainty") << val.value().result.dump();
    // Applying creates a new material revision (provenance fitted); the original is unchanged.
    auto applied = f.s.laboratory.applyCalibration(f.local(), "cal-k", json::object());
    ASSERT_TRUE(applied) << applied.error().message;
    const std::string created = applied.value().result["materials"][0];
    const Material* copy = f.host.project().materials.find(Id(created));
    ASSERT_NE(copy, nullptr);
    const PropertyValue* kv = copy->property(PhysicsDomain::Thermal, "thermalConductivity");
    EXPECT_EQ(kv->provenance, Provenance::Fitted);
    EXPECT_NEAR(kv->value, 0.50, 0.03);
    EXPECT_TRUE(kv->uncertainty.has_value());
    EXPECT_DOUBLE_EQ(f.host.project().materials.find(Id("mat-ex-lab-polymer"))->property(PhysicsDomain::Thermal, "thermalConductivity")->value, 0.40);
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.host.project().materials.find(Id(created)), nullptr);
}

TEST(LaboratoryIntegration, AMeasuredResonanceAgainstAFunctionalStructure)
{
    Fixture f("a-helmholtz-resonator.resonant", "resonance");
    auto f0 = coupling::predictLaboratoryModel("functional.metric", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.f0"}}, {}, {}, f.host.project(), nullptr);
    auto q = coupling::predictLaboratoryModel("functional.metric", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.Q"}}, {}, {}, f.host.project(), nullptr);
    ASSERT_TRUE(f0 && q);
    // A synthetic measured pressure response: 4 % lower in frequency, Q 30 % lower.
    const double fm = 0.96 * f0.value().values[0].real(), qm = 0.7 * q.value().values[0].real();
    std::ostringstream csv;
    csv << "# provenance: synthetic\n# instrument: synthetic microphone pair (example)\n# quantity: Pressure\nfrequency [Hz], real [Pa/V], imag [Pa/V]\n";
    for (int i = 0; i < 2000; ++i) {
        const double fr = 0.5 * fm + fm * i / 1999.0, r = fr / fm;
        const std::complex<double> h = 1.0 / std::complex<double>(1 - r * r, r / qm);
        csv << fr << ", " << h.real() << ", " << h.imag() << "\n";
    }
    const std::string id = Fixture::datasetOf(importText(f, "helmholtz.csv", csv.str()));
    const json metric{{"model", "functional.metric"}, {"configuration", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.f0"}}}, {"standard", 0.5}};
    const json qmetric{{"model", "functional.metric"}, {"configuration", {{"structure", "fs-helmholtz"}, {"metric", "helmholtz.Q"}}}};
    auto c = f.s.laboratory.compare(f.local(), {{"id", "cmp-res"}, {"kind", "resonance"}, {"measured", id},
                                                {"predicted", {{"frequency", metric}, {"quality", qmetric}}}});
    ASSERT_TRUE(c) << c.error().message;
    const json& r = c.value().result;
    EXPECT_EQ(r["extracted"]["classification"], "natural mode");
    EXPECT_NEAR(r["result"]["frequency"]["relative"].get<double>(), -0.04 / 0.96, 0.005);  // (measured - predicted) / measured
    EXPECT_EQ(r["verdict"], "inconsistent");
    // The explanation: findings from the record, hypotheses marked as such.
    auto e = f.s.laboratory.explain(f.local(), "cmp-res");
    ASSERT_TRUE(e);
    EXPECT_FALSE(e.value()["findings"].empty());
    bool loss = false;
    for (const json& h : e.value()["hypotheses"])
        loss = loss || h["statement"].get<std::string>().find("loss mechanism") != std::string::npos;
    EXPECT_TRUE(loss);
    EXPECT_NE(e.value()["disclaimer"].get<std::string>().find("no model or parameter was changed"), std::string::npos);
}

TEST(LaboratoryIntegration, RetrievedEffectivePropertiesAndTheirRegime)
{
    Fixture f("a-resonator-frf.resonant");
    std::ostringstream s11, s21;
    for (auto* s : {&s11, &s21})
        *s << "# provenance: synthetic\n# instrument: synthetic network analyser (example)\n# quantity: ScatteringParameter\nfrequency [Hz], real [1], imag [1]\n";
    for (int i = 0; i <= 100; ++i) {
        em::LayeredProblem p;
        p.frequencyHz = 1e9 + 4e9 * i / 100;
        em::MediumLayer l;
        l.name = "slab";
        l.thickness = 0.005;
        l.permittivityReal = 4.0;
        l.lossTangent = 0.01;
        p.layers = {l};
        auto sol = em::LayeredSolution::solve(p).value();
        s11 << p.frequencyHz << ", " << sol.reflection().real() << ", " << sol.reflection().imag() << "\n";
        s21 << p.frequencyHz << ", " << sol.transmission().real() << ", " << sol.transmission().imag() << "\n";
    }
    const std::string r = Fixture::datasetOf(importText(f, "slab-s11.csv", s11.str()));
    const std::string t = Fixture::datasetOf(importText(f, "slab-s21.csv", s21.str()));
    auto c = f.s.laboratory.compare(f.local(), {{"id", "cmp-ret"}, {"kind", "retrieval"}, {"measured", r}, {"transmission", t},
                                                {"options", {{"thickness", 0.005}, {"cellSize", 0.005}, {"atHz", 2e9}, {"retrievedStandard", 0.02}}},
                                                {"predicted", {{"permittivity", {{"value", 4.0}, {"standard", 0.01}}}}}});
    ASSERT_TRUE(c) << c.error().message;
    EXPECT_EQ(c.value().result["verdict"], "consistent") << c.value().result["result"]["permittivityAt"].dump();
    EXPECT_EQ(c.value().result["result"]["categories"]["measured"], "S-parameters (directly measured)");
    // A large cell: outside the homogenization regime, not assessed.
    auto big = f.s.laboratory.compare(f.local(), {{"id", "cmp-ret-big"}, {"kind", "retrieval"}, {"measured", r}, {"transmission", t},
                                                  {"options", {{"thickness", 0.005}, {"cellSize", 0.03}, {"atHz", 4.5e9}, {"retrievedStandard", 0.02}}},
                                                  {"predicted", {{"permittivity", {{"value", 4.0}}}}}});
    ASSERT_TRUE(big) << big.error().message;
    EXPECT_NE(big.value().result["verdict"].get<std::string>().find("outside the homogenization regime"), std::string::npos);
}

TEST(LaboratoryIntegration, PropertiesDesignRevisionsAndEnergyBalances)
{
    Fixture f("c-thermal-slab.resonant");
    // A measured conductivity against the library value (its provenance reported).
    const std::string k = Fixture::datasetOf(importText(f, "k.json", R"({"format": "resonant.dataset", "metadata": {"provenance": "measured", "instrument": "guarded hot plate"},
        "scalars": [{"name": "conductivity", "quantity": "Dimensionless", "unit": "1", "value": 0.49, "uncertainty": 0.01}]})"));
    auto c = f.s.laboratory.compare(f.local(), {{"id", "cmp-k"}, {"kind", "scalar"}, {"measured", k},
                                                {"predicted", {{"model", "material.property"}, {"unit", "1"},
                                                               {"configuration", {{"material", "mat-ex-lab-polymer"}, {"domain", "thermal"}, {"key", "thermalConductivity"}}}}}});
    ASSERT_TRUE(c) << c.error().message;
    EXPECT_EQ(c.value().result["verdict"], "inconsistent");
    EXPECT_EQ(c.value().result["predicted"]["value"]["details"]["provenance"], "literature");
    // A design revision's predicted performance against its measurement; the optimization records are untouched.
    optimization::OptimizationWorkspace w;
    optimization::RunRecord run;
    run.runId = "run-1";
    optimization::Candidate cand;
    cand.id = "cand-3";
    cand.evaluation.quantities["temperature.rise"] = 4.9;
    run.candidates.push_back(cand);
    w.runs.push_back(run);
    optimization::DesignRevision rev;
    rev.id = "rev-1";
    rev.candidateId = "cand-3";
    rev.runId = "run-1";
    w.revisions.push_back(rev);
    optimization::storeWorkspace(f.host.project(), w);
    const json before = f.host.project().extensions["optimization"];
    const std::string rise = Fixture::datasetOf(importText(f, "rise.json", R"({"format": "resonant.dataset", "metadata": {"provenance": "measured", "instrument": "thermocouple"},
        "scalars": [{"name": "rise", "quantity": "Dimensionless", "unit": "1", "value": 5.0, "uncertainty": 0.1}]})"));
    auto d = f.s.laboratory.compare(f.local(), {{"id", "cmp-rev"}, {"kind", "scalar"}, {"measured", rise},
                                                {"predicted", {{"model", "design-revision.metric"}, {"unit", "1"},
                                                               {"configuration", {{"revision", "rev-1"}, {"quantity", "temperature.rise"}}}}},
                                                {"link", {{"kind", "design-revision"}, {"id", "rev-1"}}}});
    ASSERT_TRUE(d) << d.error().message;
    EXPECT_EQ(d.value().result["verdict"], "consistent");
    EXPECT_EQ(f.host.project().extensions["optimization"], before);
    // The experimental energy balance of example f: incomplete while radiation is unmeasured.
    Fixture e("f-energy-balance.resonant");
    const json balance = e.host.project().extensions["laboratory"]["studies"][0]["conditions"]["balance"];
    auto b = e.s.laboratory.energyBalance(e.local(), balance);
    ASSERT_TRUE(b) << b.error().message;
    const json& checks = b.value()["report"]["checks"];
    EXPECT_EQ(checks[0]["model"], "energy.total");
    EXPECT_EQ(checks[0]["status"], "insufficient data");
    json measuredAll = balance;
    measuredAll["unmeasured"] = json::array();
    measuredAll["terms"].push_back({{"id", "radiation"}, {"role", "output"}, {"from", "block"}, {"to", "air"}, {"mechanism", "radiation"}, {"value", 180.0},
                                    {"standard", 30.0}, {"direct", false}, {"derivation", "emissivity 0.1, Stefan-Boltzmann"}});
    auto closed = e.s.laboratory.energyBalance(e.local(), measuredAll);
    EXPECT_EQ(closed.value()["report"]["checks"][0]["status"], "verified") << closed.value()["report"]["checks"][0].dump();
}
