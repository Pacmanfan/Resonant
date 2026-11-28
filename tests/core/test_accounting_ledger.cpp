// The ledger (RES-ACC-004, -005): the same exchange is internal or a boundary input or
// output depending on the system; rate and interval units; validation; JSON round trips.
#include "accounting/Conservation.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;

namespace {

Ledger twoBodies()
{
    Ledger l;
    l.basis = Basis::Interval;
    l.t1 = 1.0;
    l.entities = {{"a", "A", EntityKind::Body}, {"b", "B", EntityKind::Body}, {"heat", "Heat", EntityKind::Sink}};
    Exchange x;
    x.id = "a->b";
    x.from = "a";
    x.to = "b";
    x.family = Family::Energy;
    x.category = "work";
    x.mechanism = "coupling";
    x.amount = 5.0;
    l.exchanges.push_back(x);
    Exchange d = x;
    d.id = "b->heat";
    d.from = "b";
    d.to = "heat";
    d.category = "dissipation";
    d.mechanism = "friction";
    d.amount = 1.0;
    l.exchanges.push_back(d);
    Holding ha;
    ha.id = "ka";
    ha.entity = "a";
    ha.initial = 10.0;
    ha.final = 5.0;
    Holding hb = ha;
    hb.id = "kb";
    hb.entity = "b";
    hb.initial = 0.0;
    hb.final = 4.0;
    l.holdings = {ha, hb};
    return l;
}

const Check& energy(const Report& r)
{
    for (const Check& c : r.checks)
        if (c.model == "energy.total")
            return c;
    throw std::runtime_error("no energy check");
}

}  // namespace

TEST(AccountingLedger, SameEventIsInternalOrBoundaryByTheSystem)
{
    const Ledger l = twoBodies();
    ASSERT_TRUE(l.validate()) << l.validate().message();
    const AccountingSystem whole = wholeSystem(l);
    EXPECT_EQ(whole.members, (std::vector<std::string>{"a", "b"}));
    const Check w = energy(evaluate(l, whole, {}));
    EXPECT_EQ(w.status, CheckStatus::Verified);
    EXPECT_DOUBLE_EQ(w.internal[0], 5.0);
    EXPECT_DOUBLE_EQ(w.input[0], 0.0);
    EXPECT_DOUBLE_EQ(w.dissipated[0], 1.0);
    AccountingSystem a{"a", "A alone", SystemKind::Subsystem, {"a"}};
    const Check ca = energy(evaluate(l, a, {}));
    EXPECT_EQ(ca.status, CheckStatus::Verified);
    EXPECT_DOUBLE_EQ(ca.output[0], 5.0);  // the same exchange, now leaving the system
    AccountingSystem b{"b", "B alone", SystemKind::Subsystem, {"b"}};
    const Report rb = evaluate(l, b, {});
    EXPECT_DOUBLE_EQ(energy(rb).input[0], 5.0);
    EXPECT_EQ(energy(rb).status, CheckStatus::Verified);
    // The classified list says so too.
    bool seen = false;
    for (const auto& x : rb.classified)
        if (x["id"] == "a->b")
            seen = x["role"] == "input";
    EXPECT_TRUE(seen);
    // Sinks are never members.
    AccountingSystem bad{"bad", "", SystemKind::Subsystem, {"a", "heat"}};
    EXPECT_FALSE(validateFor(bad, l));
}

TEST(AccountingLedger, UnitsValidationAndRoundTrip)
{
    Ledger l = twoBodies();
    EXPECT_EQ(l.unit(Family::Energy), "J");
    l.basis = Basis::Rate;
    EXPECT_EQ(l.unit(Family::Energy), "W");
    EXPECT_EQ(l.unit(Family::LinearMomentum), "kg m/s^2");
    l.per = "m^3";
    EXPECT_EQ(l.unit(Family::Energy), "(W)/m^3");
    l.basis = Basis::Interval;
    l.per.clear();
    auto back = Ledger::fromJson(l.toJson());
    ASSERT_TRUE(back) << back.message();
    EXPECT_EQ(back.value().toJson(), l.toJson());
    Ledger bad = l;
    bad.exchanges[0].to = "nobody";
    EXPECT_FALSE(bad.validate());
    bad = l;
    bad.exchanges[0].family = Family::Force;  // force is not a conserved quantity: it is never exchanged
    EXPECT_FALSE(bad.validate());
    bad = l;
    bad.holdings[0].entity = "heat";  // a sink holds nothing
    EXPECT_FALSE(bad.validate());
}
