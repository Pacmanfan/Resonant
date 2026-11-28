// The multiscale reference workflows (RES-MSC-013, RES-MSC-014): source representation ->
// calculation -> derived property -> higher-scale material -> engineering simulation, with
// the lineage navigable back from the engineering result's material.
#include "MultiscaleFixture.h"

#include <cmath>

using namespace multiscale_test;

namespace {
constexpr double kPi = 3.14159265358979323846;

json analysis(Fixture& f, const std::string& structure)
{
    const std::string id = f.resultOf(f.s.functional.analyze(f.local(), structure, ""));
    auto r = f.s.functional.result(f.local(), id);
    EXPECT_TRUE(r);
    return r ? r.value() : json::object();
}

double metric(const json& doc, const std::string& key)
{
    for (const json& m : doc["metrics"])
        if (m["key"] == key)
            return m["value"].get<double>();
    ADD_FAILURE() << "no metric " << key;
    return 0.0;
}
}  // namespace

TEST(MultiscaleExamples, ArgonCrystalToACantilever)
{
    Fixture f("a-crystalline-argon.resonant");
    // Before the derivation the material has no elastic data: the engineering analysis fails and says so.
    {
        auto job = f.s.functional.analyze(f.local(), "fs-argon-cantilever", "");
        ASSERT_TRUE(job);
        ASSERT_TRUE(f.waitForJobs());
        auto view = f.s.jobs.get(f.local(), job.value().id);
        EXPECT_EQ(view.value().state, "failed");
    }
    const std::string id = f.resultOf(f.s.multiscale.derive(f.local(), "rep-argon-fcc", "atomistic.elastic-constants"));
    ASSERT_TRUE(f.s.multiscale.assign(f.local(), id, {{"materialId", "mat-ex-solid-argon"}}));
    const Material* ar = f.material("mat-ex-solid-argon");
    const double E = ar->property(PhysicsDomain::Mechanical, "youngsModulus")->value, rho = ar->property(PhysicsDomain::Mechanical, "density")->value;
    const json d = analysis(f, "fs-argon-cantilever");
    // Euler-Bernoulli: f1 = (1.8751^2 / 2 pi) sqrt(E I / (rho A L^4)), I = w t^3 / 12.
    const double L = 0.1, w = 0.01, t = 0.002, I = w * t * t * t / 12.0, A = w * t;
    const double f1 = 1.875104068711961 * 1.875104068711961 / (2 * kPi) * std::sqrt(E * I / (rho * A * std::pow(L, 4)));
    EXPECT_NEAR(metric(d, "cantilever.f1"), f1, 1e-3 * f1);
    // From the engineering result's material back to the crystal.
    auto l = f.s.multiscale.lineage(f.local(), "mat-ex-solid-argon");
    ASSERT_TRUE(l);
    for (const json& p : l.value()["properties"]) {
        EXPECT_EQ(p["provenance"], "computed");
        EXPECT_EQ(p["representation"]["id"], "rep-argon-fcc");
        EXPECT_EQ(p["derivationRecord"]["modelCard"], "atomistic.lj.lattice-statics");
    }
}

TEST(MultiscaleExamples, PolycrystalAndPorousCompositeGiveTheirMaterials)
{
    {
        Fixture f("b-polycrystal.resonant");
        const json d = f.derived("rve-graphite-random", "rve.thermal-conductivity");
        EXPECT_GT(prop(d, "thermalConductivity", "mean")->at("value").get<double>(), 300.0);
        ASSERT_TRUE(f.s.multiscale.assign(f.local(), d["resultId"], {{"materialId", "mat-ex-polygraphite"}}));
        EXPECT_NE(f.material("mat-ex-polygraphite")->property(PhysicsDomain::Thermal, "specificHeat"), nullptr);
    }
    {
        Fixture f("c-porous-composite.resonant");
        const json d = f.derived("rve-porous-alumina", "rve.thermal-conductivity");
        const double k = prop(d, "thermalConductivity", "mean")->at("value").get<double>();
        const double ruleOfMixtures = 0.8 * 30.0 + 0.2 * 0.026;  // the simplified homogeneous approximation
        EXPECT_LT(k, ruleOfMixtures);
        ASSERT_TRUE(f.s.multiscale.assign(f.local(), d["resultId"], {{"materialId", "mat-ex-porous-alumina"}}));
    }
}

TEST(MultiscaleExamples, PorousDielectricBecomesALayerOfAMetamaterial)
{
    Fixture f("d-multiscale-metamaterial.resonant");
    const json d = f.derived("rve-porous-dielectric", "rve.static-permittivity", {{"bounds", true}});
    const double eps = prop(d, "relativePermittivity", "mean")->at("value").get<double>();
    EXPECT_GT(eps, 1.0);
    EXPECT_LT(eps, 9.8);
    ASSERT_TRUE(f.s.multiscale.assign(f.local(), d["resultId"], {{"materialId", "mat-ex-porous-dielectric"}}));
    const json a = analysis(f, "fs-porous-layers");
    EXPECT_NEAR(metric(a, "effective.permittivityParallel"), (1.0 * eps + 2.0 * 2.1) / 3.0, 1e-6 * eps);
    // The chain: layered metamaterial <- porous layer material <- pore RVE <- alumina and air.
    auto l = f.s.multiscale.lineage(f.local(), "mat-ex-porous-dielectric");
    bool found = false;
    for (const json& p : l.value()["properties"])
        if (p["key"] == "relativePermittivity")
            found = p["representation"]["id"] == "rve-porous-dielectric" && p["chain"].size() == 2;
    EXPECT_TRUE(found) << l.value().dump();
}
