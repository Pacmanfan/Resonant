// The material-state demonstrations (RES-DMS-005..011, -014): an evolving conductor, a
// constrained heated plate, a plastic bar, damage and its resonance shift, a melting wall
// and a hysteretic core - each through the services, checked against its physics.
#include "MaterialStateFixture.h"

#include <cmath>

using namespace materialstate_test;

TEST(MaterialStateExamples, AnEvolvingConductorAbsorbsLessAsItHeats)
{
    Fixture f("a-evolving-conductor.resonant");
    const json r = f.ran(f.s.materialState.run(f.local(), "st-evolve-gel", json()));
    ASSERT_TRUE(r["completed"].get<bool>()) << r["reliability"].dump();
    EXPECT_TRUE(r["reliable"].get<bool>()) << r["reliability"].dump();
    const json& h = r["history"];
    ASSERT_EQ(h.size(), 20u);
    double previousT = 293.15, previousSigma = 1.2;
    for (const json& s : h) {
        const double T = s["regions"]["gel"]["meanTemperatureK"].get<double>();
        const double sigma = s["regions"]["gel"]["properties"]["electromagnetic.conductivity"]["value"].get<double>();
        EXPECT_GT(T, previousT);
        EXPECT_LT(sigma, previousSigma);  // the conductivity follows the temperature down
        EXPECT_NEAR(sigma, 1.2 - 0.003 * (T - 293.15), 1e-12);
        previousT = T;
        previousSigma = sigma;
    }
    EXPECT_NEAR(r["energy"]["residual"].get<double>(), 0.0, 1e-6 * r["energy"]["externalInput"].get<double>());
}

TEST(MaterialStateExamples, AConstrainedPlateThickensAndBuildsStress)
{
    Fixture f("b-thermomechanical-expansion.resonant");
    const json r = f.ran(f.s.materialState.run(f.local(), "st-evolve-plate", json()));
    ASSERT_TRUE(r["completed"].get<bool>()) << r["reliability"].dump();
    const json& last = r["history"].back();
    const double T = last["regions"]["plate"]["meanTemperatureK"].get<double>();
    EXPECT_GT(T, 293.15 + 1.0);
    EXPECT_GT(last["regions"]["plate"]["thickness"].get<double>(), 0.01);
    double mean = 0.0;
    for (double s : last["stressXX"])
        mean += s / last["stressXX"].size();
    EXPECT_LT(mean, 0.0);  // compressive
}

TEST(MaterialStateExamples, PlasticityAndDamageAndTheirStructures)
{
    {
        Fixture f("c-plastic-bar.resonant");
        const json k = f.ran(f.s.materialState.run(f.local(), "st-steel-bauschinger", json()));
        EXPECT_TRUE(k["reliable"].get<bool>());
        EXPECT_GT(std::abs(k["final"]["backStress"].get<double>()), 0.0);
    }
    Fixture f("d-damage-resonance.resonant");
    // The structure's analysis: modes of the damaged beam, through the functional framework.
    const std::string id = f.resultOf(f.s.functional.analyze(f.local(), "fs-damaged-cantilever", ""));
    auto a = f.s.functional.result(f.local(), id);
    ASSERT_TRUE(a);
    double f1 = 0, f0 = 0;
    for (const json& m : a.value()["metrics"]) {
        if (m["key"] == "damage.f1")
            f1 = m["value"].get<double>();
        if (m["key"] == "damage.f1Undamaged")
            f0 = m["value"].get<double>();
    }
    EXPECT_GT(f0, 0.0);
    EXPECT_LT(f1, f0);
    EXPECT_EQ(a.value()["modes"][0]["nature"], "natural");
    // The study: the frequency falls cycle by cycle.
    const json s = f.ran(f.s.materialState.run(f.local(), "st-beam-cycles", json()));
    const json* freq = curve(s, "frequency");
    ASSERT_NE(freq, nullptr);
    ASSERT_EQ((*freq)["y"].size(), 3u);
    EXPECT_LT((*freq)["y"][2].get<double>(), (*freq)["y"][0].get<double>());
    // The power law's damage depends only on the largest strain reached: three growing cycles and the analysis's one cycle to
    // the same largest deflection give the same modes (to the fixed-point tolerance).
    EXPECT_NEAR((*freq)["y"][2].get<double>(), f1, 1e-6 * f1);
    EXPECT_NEAR(s["energy"]["residual"].get<double>(), 0.0, 3e-3 * s["energy"]["work"].get<double>());
}

TEST(MaterialStateExamples, AWallMeltsAndACoreLosesEnergyPerCycle)
{
    {
        Fixture f("e-phase-change.resonant");
        const json r = f.ran(f.s.materialState.run(f.local(), "st-melt", json()));
        EXPECT_NEAR(r["summary"]["finalFront"].get<double>(), r["summary"]["stefanFront"].get<double>(), 0.02 * r["summary"]["stefanFront"].get<double>());
        EXPECT_NEAR(r["energy"]["residual"].get<double>(), 0.0, 1e-9 * r["energy"]["heatIn"].get<double>());
        EXPECT_FALSE(r["profiles"].empty());
    }
    Fixture f("f-hysteretic-core.resonant");
    const json r = f.ran(f.s.materialState.run(f.local(), "st-core", json()));
    const json& loops = r["energy"]["loops"];
    ASSERT_EQ(loops.size(), 2u);
    EXPECT_GT(loops[0]["lossPerCycle"].get<double>(), loops[1]["lossPerCycle"].get<double>());  // the minor loop loses less
    EXPECT_GT(loops[1]["lossPerCycle"].get<double>(), 0.0);
    EXPECT_GT(r["summary"]["powerAtFrequency"].get<double>(), 0.0);
}
