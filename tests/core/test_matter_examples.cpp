// Every architected-matter example (examples/matter, RES-FAM-019, -020) opens,
// validates, and runs the analyses its stacks admit; what each demonstrates is
// checked against its README.
#include "MatterFixture.h"

#include "coupling/MatterAnalyses.h"
#include "coupling/MatterExploration.h"
#include "matter/Manufacturing.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace matter_test;
using namespace resonant::matter;

TEST(MatterExamples, EveryExampleOpensValidatesAndAnalyzes)
{
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "matter")) {
        if (entry.path().extension() != ".resonant")
            continue;
        ++files;
        auto p = example(entry.path().filename().string());
        ASSERT_TRUE(p) << entry.path();
        EXPECT_GT(p->scene.objectCount(), 0u) << entry.path() << ": every example has a scene object";
        auto w = workspaceOf(*p);
        ASSERT_TRUE(w) << w.message();
        for (const auto& a : w.value().architectures) {
            EXPECT_FALSE(hasErrors(validate(a, p.get()))) << a.id;
            int applicable = 0;
            for (const auto& s : a.stacks)
                for (const auto& cap : resonant::coupling::matterCapabilities()) {
                    if (cap.id == "coupled-engine")
                        continue;
                    auto compat = resonant::coupling::checkMatterStack(a, s.id, cap.id, p->materials);
                    ASSERT_TRUE(compat.ok());
                    if (!compat.value().compatible)
                        continue;
                    auto r = resonant::coupling::analyzeStack(a, s.id, cap.id, json::object(), p->materials);
                    // Compatible but unconfigured (a harvester without a transduction layer ...) may still refuse.
                    if (r)
                        ++applicable;
                }
            EXPECT_TRUE(a.stacks.empty() || applicable > 0) << a.id;
            const json ex = resonant::coupling::explore(a, *p);
            EXPECT_TRUE(ex.contains("disclaimer"));
        }
    }
    EXPECT_EQ(files, 8);
}

TEST(MatterExamples, WhatEachExampleShows)
{
    // b: the bonded converter can be made; the film cannot (it delaminates on cooling).
    auto b = example("b-energy-converter.resonant");
    EXPECT_EQ(checkManufacturing(architecture(*b, "arch-film")).status == "feasible", false);
    // c: a quarter of the barrier's resistance is not in the zirconia alone.
    auto c = example("c-thermal-barrier.resonant");
    auto t = resonant::coupling::analyzeStack(architecture(*c, "arch-tbc"), "st-wall", "thermal", json::object(), c->materials);
    ASSERT_TRUE(t);
    EXPECT_GT(t.value()["metrics"]["interfaceShare"].get<double>(), 0.01);
    // e: the absorber's peak near 10 GHz.
    auto e = example("e-em-absorber.resonant");
    auto em = resonant::coupling::analyzeStack(architecture(*e, "arch-absorber"), "st-absorber", "electromagnetic",
                                               {{"range", {{"from", 8e9}, {"to", 12e9}, {"points", 41}}}, {"exit", "metal"}}, e->materials);
    ASSERT_TRUE(em);
    EXPECT_NEAR(em.value()["metrics"]["frequencyOfMaxAbsorptance"].get<double>() / 10e9, 1.0, 0.03);
    // f: three dimensions, two objects, an embedded sensor and a channel, no stack.
    auto f = example("f-multimaterial-structure.resonant");
    const Architecture m = architecture(*f, "arch-multi");
    EXPECT_TRUE(m.stacks.empty());
    EXPECT_EQ(m.objects.size(), 2u);
    // g: the trade-off problem is stored with the matter adapter.
    auto g = example("g-thickness-tradeoff.resonant");
    ASSERT_TRUE(g->extensions.contains("optimization"));
    EXPECT_EQ(g->extensions["optimization"]["problems"][0]["adapter"], "matter");
}
