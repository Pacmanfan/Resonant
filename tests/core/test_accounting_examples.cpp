// The accounting demonstrations (RES-ACC-004..016): every example study runs through the
// services, and each is checked against what it demonstrates.
#include "AccountingFixture.h"

#include <cmath>

using namespace accounting_test;

namespace {

const json& checkOf(const json& report, const std::string& model)
{
    for (const json& c : report["checks"])
        if (c["model"] == model)
            return c;
    static const json none = json::object();
    ADD_FAILURE() << "no check " << model;
    return none;
}

const json& system(const json& result, const std::string& id)
{
    for (const json& r : result["reports"])
        if (r["system"]["id"] == id)
            return r;
    static const json none = json::object();
    ADD_FAILURE() << "no system " << id;
    return none;
}

}  // namespace

TEST(AccountingExamples, EveryStudyRunsAndNothingExceeds)
{
    for (const char* file : {"a-closed-collision.resonant", "b-damped-driven-oscillator.resonant", "c-resonator-buildup-decay.resonant",
                             "d-coupled-resonators.resonant", "e-microwave-absorption.resonant", "f-radiation-pressure.resonant",
                             "g-material-energy.resonant", "h-torsional-flywheels.resonant"}) {
        Fixture f(file);
        const json studies = f.s.accounting.studies(f.local()).value();  // a copy: the outcome is a temporary
        for (const json& st : studies) {
            const json r = f.ran(f.s.accounting.run(f.local(), st["id"], json()));
            ASSERT_TRUE(r.contains("reports")) << file << " " << st["id"];
            for (const json& rep : r["reports"])
                for (const json& c : rep["checks"]) {
                    EXPECT_NE(c["status"], "exceeded") << file << " " << st["id"] << " " << rep["system"]["id"] << " " << c["model"] << ": " << c["attribution"];
                    EXPECT_NE(c["status"], "not evaluated") << file << " " << st["id"] << " " << c["model"];
                }
        }
    }
}

TEST(AccountingExamples, CollisionAndFlywheels)
{
    {
        Fixture f("a-closed-collision.resonant");
        const json r = f.ran(f.s.accounting.run(f.local(), "st-collision", json()));
        EXPECT_EQ(checkOf(system(r, "whole"), "momentum.linear")["status"], "verified");
        EXPECT_NEAR(checkOf(system(r, "whole"), "momentum.linear")["final"][0].get<double>(), 1.0, 1e-12);
        // Glider a gave momentum to glider b through the bumper.
        EXPECT_LT(checkOf(system(r, "glider-a"), "momentum.linear")["final"][0].get<double>(), 1.0);
        EXPECT_GT(checkOf(system(r, "glider-b"), "momentum.linear")["final"][0].get<double>(), 0.0);
        ASSERT_TRUE(r.contains("convergence"));
        bool fourth = false;
        for (const json& c : r["convergence"])
            if (c["system"] == "glider-a" && c["model"] == "energy.total" && c["ratio"].is_number())
                fourth = c["ratio"].get<double>() > 8.0;
        EXPECT_TRUE(fourth) << r["convergence"].dump();
    }
    Fixture f("h-torsional-flywheels.resonant");
    const json r = f.ran(f.s.accounting.run(f.local(), "st-spinup", json()));
    const json& L = checkOf(system(r, "whole"), "momentum.angular");
    EXPECT_EQ(L["status"], "verified");
    EXPECT_NEAR(L["impulse"][2].get<double>(), 1.0, 0.01);
    EXPECT_EQ(checkOf(system(r, "rotor"), "momentum.angular")["status"], "verified");
    EXPECT_EQ(L["unit"], "kg m^2/s");
}

TEST(AccountingExamples, ResonatorDrivenAndRingingDown)
{
    Fixture f("c-resonator-buildup-decay.resonant");
    const json q = f.ran(f.s.accounting.run(f.local(), "st-q", json()));
    const json& Q = checkOf(q["reports"][0], "resonance.quality-factor");
    EXPECT_EQ(Q["status"], "verified");
    EXPECT_NEAR(Q["details"]["qualityDriven"].get<double>(), 2 * 3.14159265358979 * 10.0 / 0.3, 1e-3);
    const json b = f.ran(f.s.accounting.run(f.local(), "st-buildup", json()));
    EXPECT_EQ(checkOf(b["reports"][0], "resonance.buildup-decay")["status"], "verified");
}

TEST(AccountingExamples, MicrowaveRadiationAndMaterials)
{
    {
        Fixture f("e-microwave-absorption.resonant");
        const json r = f.ran(f.s.accounting.run(f.local(), "st-steady", json()));
        EXPECT_EQ(checkOf(system(r, "whole"), "energy.total")["status"], "verified");
        EXPECT_EQ(checkOf(system(r, "wave"), "energy.total")["status"], "verified");
        EXPECT_EQ(checkOf(system(r, "whole"), "momentum.linear")["status"], "insufficient data");
        // The effective permittivity of the food is a homogenized value: its scale contract is recorded.
        ASSERT_FALSE(r["ledger"]["contracts"].empty());
        EXPECT_FALSE(r["ledger"]["contracts"][0]["sourceRepresented"].get<bool>());
        const json e = f.ran(f.s.accounting.run(f.local(), "st-heating", json()));
        EXPECT_EQ(checkOf(e["reports"][0], "energy.total")["status"], "verified");
        EXPECT_EQ(e["ledger"]["times"].size(), 16u);
    }
    {
        Fixture f("f-radiation-pressure.resonant");
        const json m = f.ran(f.s.accounting.run(f.local(), "st-mirror", json()));
        const json& force = checkOf(m["reports"][0], "radiation.force-methods");
        EXPECT_EQ(force["status"], "verified");
        EXPECT_NEAR(force["details"]["surfaces"].get<double>(), 2.0 * 1000.0 * 0.25 / 299792458.0, 2e-3 * 2.0 * 250.0 / 299792458.0);
    }
    Fixture f("g-material-energy.resonant");
    const json core = f.ran(f.s.accounting.run(f.local(), "st-acc-core", json()));
    EXPECT_EQ(checkOf(core["reports"][0], "hysteresis.loop-loss")["status"], "verified");
    const json melt = f.ran(f.s.accounting.run(f.local(), "st-acc-melt", json()));
    const json& s = checkOf(melt["reports"][0], "entropy.second-law");
    EXPECT_GT(s["generated"].get<double>(), 0.0);
    EXPECT_EQ(checkOf(melt["reports"][0], "energy.total")["status"], "verified");
}
