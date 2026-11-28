// Exploration, mechanisms and pathways (RES-FAM-012, -017): Direction A lists
// mechanisms and cascades with their implementation status; Direction B finds
// what an architecture can do and what may fail, with numbers, keeping findings
// apart from hypotheses; pathways are evaluated stage by stage with balanced
// ledgers, and a chain efficiency only when every stage is modeled.
#include "MatterFixture.h"

#include "coupling/MatterExploration.h"

#include <gtest/gtest.h>

using namespace matter_test;
using namespace resonant::matter;
using namespace resonant::coupling;

namespace {

bool mentions(const json& list, const std::string& phrase)
{
    for (const json& x : list) {
        const std::string text = x.is_string() ? x.get<std::string>() : x.value("statement", std::string());
        if (text.find(phrase) != std::string::npos)
            return true;
    }
    return false;
}

}  // namespace

TEST(MatterExploration, DirectionAMechanismsWithTheirStatus)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture a = architecture(*p, "arch-bonded");
    const json me = mechanismsFor("mechanical", "electrical", &a, *p);
    bool piezo = false, induction = false;
    for (const json& m : me["direct"]) {
        if (m["id"] == "piezoelectric-direct") {
            piezo = m["implemented"].get<bool>();
            EXPECT_EQ(m["regions"], json::array({"pzt"}));
        }
        if (m["id"] == "electromagnetic-induction")
            induction = !m["implemented"].get<bool>();
    }
    EXPECT_TRUE(piezo);
    EXPECT_TRUE(induction) << "listed, and marked not implemented";
    EXPECT_TRUE(me["modeled"].get<bool>());
    // Thermal to electrical: the thermoelectric effect is not modeled; a cascade through
    // mechanics (expansion, then piezoelectricity) is.
    const json te = mechanismsFor("thermal", "electrical", &a, *p);
    for (const json& m : te["direct"])
        EXPECT_FALSE(m["implemented"].get<bool>()) << m["id"];
    bool cascade = false;
    for (const json& c : te["cascades"])
        if (c["through"] == "mechanical" && c["implemented"].get<bool>())
            cascade = true;
    EXPECT_TRUE(cascade);
    // The engine's catalogue keeps its own statuses: its piezoelectric coupling is planned.
    const Mechanism* engine = findMechanism("engine:em.piezoelectric");
    ASSERT_NE(engine, nullptr);
    EXPECT_FALSE(engine->implemented());
    EXPECT_TRUE(findMechanism("engine:em.absorption-to-heat")->implemented());
    const json ae = mechanismsFor("optical", "electrical", nullptr, *p);
    EXPECT_FALSE(ae["modeled"].get<bool>());
    // Standalone models (REQ-CHAT-20261128-005) are named, without claiming an analysis of architectures.
    for (const char* id : {"thermoelectric", "magnetostrictive", "photovoltaic"}) {
        const Mechanism* m = findMechanism(id);
        ASSERT_NE(m, nullptr) << id;
        EXPECT_EQ(m->status, "standalone") << id;
        EXPECT_FALSE(m->implemented()) << id;
        EXPECT_FALSE(m->model.empty()) << id;
    }
}

TEST(MatterExploration, DirectionBFindsWhatTheConverterCanDoAndWhatMayFail)
{
    auto p = example("b-energy-converter.resonant");
    const json bonded = explore(architecture(*p, "arch-bonded"), *p);
    EXPECT_TRUE(mentions(bonded["findings"], "delivers")) << bonded["findings"].dump(2);
    EXPECT_TRUE(mentions(bonded["findings"], "from the neutral surface"));
    EXPECT_TRUE(mentions(bonded["findings"], "first mode of 'st-beam'")) << "against the 120 Hz target";
    EXPECT_FALSE(bonded["disclaimer"].get<std::string>().empty());
    EXPECT_EQ(explore(architecture(*p, "arch-bonded"), *p), bonded) << "deterministic";
    // The film: crystallized at 923 K on steel, it delaminates.
    const json film = explore(architecture(*p, "arch-film"), *p);
    EXPECT_TRUE(mentions(film["findings"], "delamination")) << film["findings"].dump(2);
    EXPECT_TRUE(mentions(film["hypotheses"], "idealized model"));
    EXPECT_TRUE(mentions(film["suggestions"], "measure the d31"));
    // A film at the neutral surface of a symmetric stack converts little; an electrode removed.
    Architecture sym = architecture(*p, "arch-bonded");
    auto& layers = sym.stack("st-beam")->layers;
    layers.push_back({"steel-top", "substrate", 5e-4, std::nullopt, 0.0, Pattern{}});
    sym.interfaces.push_back({"i-top", "", {"encapsulation", "substrate"}, {{"perfect", {}}}, std::nullopt, std::nullopt, "", {}, "", ""});
    const json centred = explore(sym, *p);
    EXPECT_TRUE(mentions(centred["findings"], "bending strains it little")) << centred["findings"].dump(2);
    Architecture bare = architecture(*p, "arch-bonded");
    bare.stack("st-beam")->layers.erase(bare.stack("st-beam")->layers.begin() + 4, bare.stack("st-beam")->layers.end());
    const json open = explore(bare, *p);
    EXPECT_TRUE(mentions(open["findings"], "no layer above")) << open["findings"].dump(2);
}

TEST(MatterExploration, PathwaysStageByStage)
{
    auto p = example("b-energy-converter.resonant");
    auto harvest = evaluatePathway(architecture(*p, "arch-bonded"), "f-harvest", json::object(), p->materials);
    ASSERT_TRUE(harvest.ok()) << harvest.message();
    const json& stage = harvest.value()["stages"][0];
    EXPECT_EQ(stage["status"], "evaluated");
    EXPECT_EQ(stage["balance"], "verified");
    EXPECT_GT(stage["efficiency"].get<double>(), 0.0);
    EXPECT_LT(stage["efficiency"].get<double>(), 1.0);
    EXPECT_TRUE(harvest.value()["complete"].get<bool>());
    EXPECT_DOUBLE_EQ(harvest.value()["chainEfficiency"].get<double>(), stage["efficiency"].get<double>());
    EXPECT_FALSE(harvest.value()["violatesConservation"].get<bool>());
    // The absorber: the absorbed share with its ledger; the barrier: transport with its entropy.
    auto e = example("e-em-absorber.resonant");
    auto absorb = evaluatePathway(architecture(*e, "arch-absorber"), "f-absorb", json::object(), e->materials);
    ASSERT_TRUE(absorb.ok()) << absorb.message();
    EXPECT_EQ(absorb.value()["stages"][0]["balance"], "verified");
    EXPECT_GT(absorb.value()["stages"][0]["efficiency"].get<double>(), 0.9);
    auto c = example("c-thermal-barrier.resonant");
    auto insulate = evaluatePathway(architecture(*c, "arch-tbc"), "f-insulate", json::object(), c->materials);
    ASSERT_TRUE(insulate.ok());
    EXPECT_GT(insulate.value()["stages"][0]["entropyGeneration"].get<double>(), 0.0);
    // A stage nobody models leaves the chain incomplete.
    Architecture a = architecture(*p, "arch-bonded");
    Function f;
    f.id = "f-thermo";
    f.stages = {{"s-heat", "thermal", "electrical", "thermoelectric", {"pzt"}, json::object()}};
    a.functions.push_back(f);
    auto incomplete = evaluatePathway(a, "f-thermo", json::object(), p->materials);
    ASSERT_TRUE(incomplete.ok());
    EXPECT_EQ(incomplete.value()["stages"][0]["status"], "not modeled");
    EXPECT_FALSE(incomplete.value()["complete"].get<bool>());
    EXPECT_TRUE(incomplete.value()["chainEfficiency"].is_null());
    EXPECT_FALSE(evaluatePathway(a, "f-none", json::object(), p->materials).ok());
}
