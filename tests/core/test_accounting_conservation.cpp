// Conservation monitoring (RES-ACC-010, -011, -013, -015): honest statuses - missing data
// is never verified, absent quantities are not applicable - residuals attributed to the
// stated numerical errors and never to dissipation, double counting refused, negative
// entropy production rejected, and histories of the residual.
#include "accounting/Conservation.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;

namespace {

Ledger oneBody(double input, double stored, double dissipated)
{
    Ledger l;
    l.t1 = 1.0;
    l.entities = {{"src", "Source", EntityKind::Source}, {"body", "Body", EntityKind::Body}, {"heat", "Heat", EntityKind::Sink}};
    Exchange in;
    in.id = "in";
    in.from = "src";
    in.to = "body";
    in.category = "work";
    in.mechanism = "drive";
    in.amount = input;
    Exchange d = in;
    d.id = "loss";
    d.from = "body";
    d.to = "heat";
    d.category = "dissipation";
    d.mechanism = "damping";
    d.amount = dissipated;
    l.exchanges = {in, d};
    Holding h;
    h.id = "kinetic";
    h.entity = "body";
    h.final = stored;
    l.holdings = {h};
    return l;
}

Check check(const Report& r, const std::string& model)
{
    for (const Check& c : r.checks)
        if (c.model == model)
            return c;
    return {};
}

}  // namespace

TEST(AccountingConservation, StatusesAreHonest)
{
    Ledger l = oneBody(10.0, 7.0, 3.0);
    const Report r = evaluate(l, wholeSystem(l), {});
    EXPECT_EQ(check(r, "energy.total").status, CheckStatus::Verified);
    EXPECT_EQ(check(r, "momentum.linear").status, CheckStatus::NotApplicable);  // nothing reports momentum
    EXPECT_EQ(check(r, "entropy.second-law").status, CheckStatus::NotApplicable);
    // A declared missing contribution: never verified, even with a zero residual.
    l.missing.push_back({Family::Energy, "body", "radiation through the boundary", "the solver has no radiating boundary"});
    const Check c = check(evaluate(l, wholeSystem(l), {}), "energy.total");
    EXPECT_EQ(c.status, CheckStatus::InsufficientData);
    EXPECT_EQ(c.missing.size(), 1u);
    l.missing = {{Family::LinearMomentum, "", "momentum", "not reported"}};
    EXPECT_EQ(check(evaluate(l, wholeSystem(l), {}), "momentum.linear").status, CheckStatus::InsufficientData);
    // A relative residual only when the scale is meaningful.
    Ledger tiny = oneBody(0.0, 0.0, 0.0);
    Tolerances t;
    t.absolute = 1e-12;
    const Check z = check(evaluate(tiny, wholeSystem(tiny), t), "energy.total");
    EXPECT_FALSE(z.relative.has_value());
    EXPECT_EQ(z.status, CheckStatus::Verified);
    EXPECT_EQ(worst({z, c}), CheckStatus::InsufficientData);
    for (const char* id : {"energy.total", "momentum.linear", "momentum.angular", "entropy.second-law", "mass", "charge", "resonance.quality-factor",
                           "resonance.buildup-decay", "radiation.force-methods", "absorption.local-global", "hysteresis.loop-loss"}) {
        const CheckModel* m = findCheckModel(id);
        ASSERT_NE(m, nullptr) << id;
        EXPECT_FALSE(m->equation.empty() || m->assumptions.empty() || m->requiredData.empty()) << id;
    }
}

TEST(AccountingConservation, NumericalErrorIsNotDissipation)
{
    // 0.01 J unaccounted, a stated integration error of 0.02 J: within it - and the dissipation stays 3 J.
    Ledger l = oneBody(10.0, 6.99, 3.0);
    l.numerical.push_back({Family::Energy, "integrator", "Runge-Kutta", 0.02});
    Tolerances t;
    t.relative = 1e-6;
    t.warning = 1e-2;
    Check c = check(evaluate(l, wholeSystem(l), t), "energy.total");
    EXPECT_NEAR(c.residualNorm(), 0.01, 1e-12);
    EXPECT_DOUBLE_EQ(c.dissipated[0], 3.0);
    EXPECT_EQ(c.status, CheckStatus::Warning);
    EXPECT_NE(c.attribution.find("within the stated numerical errors"), std::string::npos) << c.attribution;
    // Beyond the stated errors: unaccounted, and exceeded.
    l.holdings[0].final = 6.0;
    c = check(evaluate(l, wholeSystem(l), t), "energy.total");
    EXPECT_EQ(c.status, CheckStatus::Exceeded);
    EXPECT_NE(c.attribution.find("unaccounted"), std::string::npos);
    EXPECT_DOUBLE_EQ(c.dissipated[0], 3.0);
}

TEST(AccountingConservation, EffectiveAndExplicitRepresentationsAreNotBothCounted)
{
    Ledger l = oneBody(10.0, 7.0, 3.0);
    Entity micro{"rve", "Microstructure", EntityKind::Representation};
    micro.representation = "rve-1";
    l.entities.push_back(micro);
    l.entities[1].effectiveOf = "rve-1";  // the body uses the effective material of rve-1
    Holding h;
    h.id = "rve-energy";
    h.entity = "rve";
    h.final = 1.0;
    l.holdings.push_back(h);
    const Check both = check(evaluate(l, wholeSystem(l), {}), "energy.total");
    EXPECT_EQ(both.status, CheckStatus::NotEvaluated);
    EXPECT_NE(both.notes.back().find("double counting across scales"), std::string::npos);
    AccountingSystem effective{"eff", "", SystemKind::Object, {"body"}};
    EXPECT_EQ(check(evaluate(l, effective, {}), "energy.total").status, CheckStatus::Verified);
    // The same mechanism twice between the same entities: flagged.
    Ledger d = oneBody(10.0, 7.0, 3.0);
    Exchange again = d.exchanges[0];
    again.id = "in-again";
    again.amount = 0.0;
    d.exchanges.push_back(again);
    const Check twice = check(evaluate(d, wholeSystem(d), {}), "energy.total");
    EXPECT_EQ(twice.status, CheckStatus::Warning);
    EXPECT_NE(twice.notes.back().find("double counting"), std::string::npos);
}

TEST(AccountingConservation, EntropyProductionAndHistories)
{
    Ledger l = oneBody(10.0, 7.0, 3.0);
    l.times = {0.0, 0.5, 1.0};
    l.exchanges[0].history = {0.0, 5.0, 10.0};
    l.exchanges[1].history = {0.0, 1.0, 3.0};
    l.holdings[0].history = {0.0, 4.0, 7.0};
    Generation g;
    g.id = "production";
    g.entity = "body";
    g.family = Family::Entropy;
    g.amount = -0.01;
    l.generations.push_back(g);
    Holding s;
    s.id = "entropy";
    s.entity = "body";
    s.family = Family::Entropy;
    s.final = -0.01;
    l.holdings.push_back(s);
    const Report r = evaluate(l, wholeSystem(l), {});
    const Check e = check(r, "energy.total");
    ASSERT_EQ(e.residualHistory.size(), 3u);
    for (double x : e.residualHistory)
        EXPECT_NEAR(x, 0.0, 1e-12);
    const Check sc = check(r, "entropy.second-law");
    EXPECT_EQ(sc.status, CheckStatus::Exceeded);  // negative production
    bool said = false;
    for (const std::string& n : sc.notes)
        said = said || n.find("second law") != std::string::npos;
    EXPECT_TRUE(said);
    ASSERT_FALSE(r.series.empty());
    EXPECT_EQ(r.series[0]["stored"].back(), 7.0);
}
