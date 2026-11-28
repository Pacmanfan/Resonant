// Cross-domain accounting of the coupled engine (RES-ACC-006, -012): a microwave-heated
// slab - incident, reflected, transmitted and absorbed power, the absorbed power transferred
// once from the wave to the heat, the same transfer internal to the whole and a boundary
// flow of each participant alone, momentum reported as missing - and a time-stepped
// evolution accounted step by step.
#include "accounting/Conservation.h"
#include "coupling/AccountingAdapters.h"
#include "coupling/CoupledRun.h"
#include "coupling/StateEvolution.h"
#include "multiphysics/PhysicalSystem.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;
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
             {PhysicsDomain::Thermal, "specificHeat", 1000.0}})
        EXPECT_TRUE(m.setProperty(d, k, value(v)).ok());
    MaterialLibrary lib;
    EXPECT_TRUE(lib.add(m).ok());
    return lib;
}

multiphysics::PhysicalSystem slab()
{
    const json em{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
                  {"settings", {{"frequencyHz", 2.45e9}, {"incidentIntensity", 5e4}}}};
    const json faces{{"-z", {{"kind", "convective"}, {"h", 20.0}}}, {"+z", {{"kind", "convective"}, {"h", 20.0}}}};
    const json settings{{"faces", faces}};
    const json heat{{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"}, {"settings", settings}};
    const json j{{"format", "resonant.physical-system"}, {"version", 1}, {"id", "sys-acc"}, {"name", "heated slab"},
                 {"geometry", {{"kind", "layered-slab"}, {"lateralArea", 0.01}}}, {"environment", {{"ambientTemperature", 293.15}}},
                 {"regions", {{{"id", "slab"}, {"name", "Slab"}, {"materialId", "mat-slab"}, {"thickness", 0.01}, {"cells", 40}}}},
                 {"domains", {em, heat}},
                 {"couplings", {{{"id", "absorb"}, {"model", "em.absorption-to-heat"}, {"from", "em"}, {"to", "heat"}}}},
                 {"materialLaws", json::array()},
                 {"scheme", {{"kind", "one-way"}, {"maxIterations", 20}, {"temperatureTolerance", 1e-7}}}};
    auto s = multiphysics::PhysicalSystem::fromJson(j);
    EXPECT_TRUE(s) << s.message();
    return s.take();
}

Check check(const Report& r, const std::string& model)
{
    for (const Check& c : r.checks)
        if (c.model == model)
            return c;
    ADD_FAILURE() << "no check " << model;
    return {};
}

}  // namespace

TEST(AccountingCoupled, AbsorbedPowerMovesOnceFromTheWaveToTheHeat)
{
    auto run = coupling::runPhysicalSystem(slab(), library());
    ASSERT_TRUE(run) << run.message();
    ASSERT_TRUE(run.value().ok()) << run.value().message;
    auto l = coupling::coupledLedger(run.value().ledger.toJson(), run.value().audit.toJson(), "heated slab");
    ASSERT_TRUE(l) << l.message();
    EXPECT_EQ(l.value().basis, Basis::Rate);
    const Tolerances t{1e-6, 1e-3, 1e-15};
    const Report whole = evaluate(l.value(), wholeSystem(l.value()), t);
    const Check e = check(whole, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    EXPECT_EQ(e.unit, "W");
    // The incident power: 5e4 W/m^2 on 0.01 m^2.
    EXPECT_NEAR(e.input[0], 500.0, 1e-9);
    const Exchange* transfer = nullptr;
    for (const Exchange& x : l.value().exchanges)
        if (x.category == "transfer")
            transfer = &x;
    ASSERT_NE(transfer, nullptr);
    EXPECT_EQ(transfer->fromDomain, "electromagnetic");
    EXPECT_EQ(transfer->toDomain, "thermal");
    EXPECT_NEAR(e.internal[0], transfer->amount, 1e-12);  // internal to the whole: not an input, not a loss
    // The wave alone: incident in; reflected, transmitted and the transfer out.
    AccountingSystem wave{"em", "The wave", SystemKind::Subsystem, {"em"}};
    const Check ce = check(evaluate(l.value(), wave, t), "energy.total");
    EXPECT_EQ(ce.status, CheckStatus::Verified) << ce.attribution;
    EXPECT_GT(ce.output[0], transfer->amount);
    // The heat alone: the transfer in, the faces out.
    AccountingSystem heat{"heat", "The heat", SystemKind::Subsystem, {"heat"}};
    const Check ch = check(evaluate(l.value(), heat, t), "energy.total");
    EXPECT_EQ(ch.status, CheckStatus::Verified) << ch.attribution;
    EXPECT_NEAR(ch.input[0], transfer->amount, 1e-12);
    // The coupled participants report no momentum: said, not assumed.
    EXPECT_EQ(check(whole, "momentum.linear").status, CheckStatus::InsufficientData);
}

TEST(AccountingCoupled, AnEvolutionIsAccountedStepByStep)
{
    coupling::EvolutionSettings s;
    s.duration = 120.0;
    s.steps = 12;
    s.substeps = 4;
    auto doc = coupling::evolveSystem(slab(), library(), s, nullptr);
    ASSERT_TRUE(doc) << doc.message();
    auto l = coupling::evolutionLedger(doc.value());
    ASSERT_TRUE(l) << l.message();
    EXPECT_EQ(l.value().basis, Basis::Interval);
    ASSERT_EQ(l.value().times.size(), 13u);
    const Report r = evaluate(l.value(), wholeSystem(l.value()), {1e-6, 1e-3, 1e-15});
    const Check e = check(r, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    EXPECT_EQ(e.unit, "J");
    ASSERT_EQ(e.residualHistory.size(), 13u);
    for (double x : e.residualHistory)
        EXPECT_LT(x, 1e-6 * e.input[0]);
    // The heat stored grows step by step.
    const Holding* stored = nullptr;
    for (const Holding& h : l.value().holdings)
        if (h.entity == "heat")
            stored = &h;
    ASSERT_NE(stored, nullptr);
    for (size_t k = 1; k < stored->history.size(); ++k)
        EXPECT_GT(stored->history[k], stored->history[k - 1]);
    ASSERT_FALSE(r.series.empty());
}
