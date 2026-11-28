// Experimental energy accounting (RES-LAB-013): a measured balance judged against its
// expanded uncertainty - consistent, inconsistent, incomplete with an unmeasured pathway,
// unjudgeable without uncertainties - and derived terms naming their derivation.
#include "accounting/Conservation.h"
#include "laboratory/ExperimentalEnergy.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::laboratory;
namespace acc = resonant::accounting;

namespace {

ExperimentalBalance heater(double out, std::vector<std::pair<std::string, std::string>> unmeasured = {})
{
    ExperimentalBalance b;
    b.title = "a heated block";
    b.members = {"block"};
    b.terms = {{"electrical", "input", "supply", "block", "Joule heating", 100.0, 1.0, false, "P = V I from voltage and current", {"v", "i"}},
               {"stored", "stored", "", "block", "thermal (m c dT)", 60.0, 2.0, false, "m c dT from the block's mass, specific heat and temperature rise", {}},
               {"convection", "output", "block", "surroundings", "convection", out, 1.5, false, "h A (T - T_air) dt with h from a correlation", {}}};
    b.unmeasured = std::move(unmeasured);
    return b;
}

acc::Check energy(const acc::Ledger& l)
{
    const auto r = acc::evaluate(l, acc::wholeSystem(l), {});
    for (const auto& c : r.checks)
        if (c.model == "energy.total")
            return c;
    return {};
}

}  // namespace

TEST(LaboratoryEnergy, AMeasuredBalanceIsJudgedByItsUncertainty)
{
    auto l = experimentalLedger(heater(39.0));
    ASSERT_TRUE(l) << l.message();
    EXPECT_TRUE(l.value().measured);
    const auto c = energy(l.value());
    EXPECT_EQ(c.status, acc::CheckStatus::Verified) << c.attribution;
    EXPECT_NEAR(c.measurementUncertainty, std::sqrt(1.0 + 4.0 + 2.25), 1e-12);
    EXPECT_NE(c.attribution.find("consistent within the expanded measurement uncertainty"), std::string::npos);
    const auto bad = energy(experimentalLedger(heater(20.0)).value());
    EXPECT_EQ(bad.status, acc::CheckStatus::Exceeded);
    EXPECT_NE(bad.attribution.find("inconsistent"), std::string::npos);
    EXPECT_NE(bad.attribution.find("unmeasured pathway"), std::string::npos);
}

TEST(LaboratoryEnergy, UnmeasuredPathwaysAndMissingUncertainties)
{
    const auto c = energy(experimentalLedger(heater(39.0, {{"radiation from the block", "no radiometer"}})).value());
    EXPECT_EQ(c.status, acc::CheckStatus::InsufficientData);
    ASSERT_EQ(c.missing.size(), 1u);
    ExperimentalBalance b = heater(39.0);
    for (auto& t : b.terms)
        t.standard = 0.0;
    EXPECT_EQ(energy(experimentalLedger(b).value()).status, acc::CheckStatus::NotEvaluated);
    b = heater(39.0);
    b.terms[0].derivation.clear();
    EXPECT_FALSE(experimentalLedger(b));  // a derived term names its derivation
    b = heater(39.0);
    b.terms[0].role = "teleported";
    EXPECT_FALSE(experimentalLedger(b));
    auto round = ExperimentalBalance::fromJson(heater(39.0).toJson());
    ASSERT_TRUE(round);
    EXPECT_EQ(round.value().toJson(), heater(39.0).toJson());
}
