// The material-state models in the ledger (RES-ACC-011, -013, -014): work stored
// reversibly, locked in hardening or dissipated, within each model's stated quadrature
// error; the isothermal entropy production; the melting slab's energy and entropy with a
// non-negative production; the Preisach loss against the switching hysterons.
#include "accounting/Conservation.h"
#include "coupling/AccountingAdapters.h"
#include "coupling/MaterialStateModels.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;
using nlohmann::json;

namespace {

Check check(const Report& r, const std::string& model)
{
    for (const Check& c : r.checks)
        if (c.model == model)
            return c;
    ADD_FAILURE() << "no check " << model;
    return {};
}

Ledger ledgerOf(json study)
{
    if (!study.contains("loading"))  // the model's first loading preset
        study["loading"] = coupling::findMaterialStateModel(study["model"].get<std::string>())->loadings.at(0)["loading"];
    auto run = coupling::runMaterialStateStudy(study, nullptr);
    EXPECT_TRUE(run) << run.message();
    if (!run)
        return {};
    auto l = coupling::materialStateLedger(run.value());
    EXPECT_TRUE(l) << l.message();
    return l ? l.take() : Ledger{};
}

}  // namespace

TEST(AccountingMaterialState, PlasticWorkIsStoredLockedOrDissipated)
{
    const Ledger l = ledgerOf({{"model", "mechanics.j2.point"}, {"loading", {{"amplitudes", {0.004, 0.006}}, {"increments", 60}}}});
    EXPECT_EQ(l.per, "m^3");
    const Report r = evaluate(l, wholeSystem(l), {1e-6, 1e-3, 1e-15});
    const Check e = check(r, "energy.total");
    EXPECT_NE(e.status, CheckStatus::Exceeded) << e.attribution;
    EXPECT_NE(e.attribution.find("within the stated numerical errors"), std::string::npos) << e.attribution;
    EXPECT_EQ(e.unit, "(J)/m^3");
    bool locked = false;
    for (const Holding& h : l.holdings)
        locked = locked || (h.form.find("hardening") != std::string::npos && !h.recoverable);
    EXPECT_TRUE(locked);
    const Check s = check(r, "entropy.second-law");
    EXPECT_EQ(s.status, CheckStatus::Verified);
    EXPECT_GT(s.generated[0], 0.0);
    for (const char* model : {"mechanics.damage.point", "mechanics.viscoelastic.sls", "mechanics.j2.bar", "mechanics.damaged-cantilever"}) {
        const Ledger m = ledgerOf({{"model", model}});
        const Check c = check(evaluate(m, wholeSystem(m), {1e-4, 1e-2, 1e-15}), "energy.total");
        EXPECT_NE(c.status, CheckStatus::Exceeded) << model << ": " << c.attribution;
        EXPECT_NE(c.status, CheckStatus::NotEvaluated) << model;
    }
}

TEST(AccountingMaterialState, MeltingSlabEnergyAndEntropy)
{
    const Ledger l = ledgerOf({{"model", "thermal.phase-change.slab"}, {"parameters", {{"cells", 100}}}, {"loading", {{"duration", 1800.0}, {"snapshots", 6}}}});
    EXPECT_EQ(l.per, "m^2");
    const Report r = evaluate(l, wholeSystem(l), {1e-6, 1e-2, 1e-15});
    EXPECT_EQ(check(r, "energy.total").status, CheckStatus::Verified);
    const Check s = check(r, "entropy.second-law");
    EXPECT_GT(s.generated[0], 0.0);
    EXPECT_NE(s.status, CheckStatus::Exceeded) << s.attribution;
    ASSERT_TRUE(s.relative.has_value());
    EXPECT_LT(*s.relative, 1e-2);  // the explicit step's first-order error
}

TEST(AccountingMaterialState, HysteresisLossAgreesWithTheHysterons)
{
    // The difference stays within the rigorous bound 2 mu0 Ms dH (each switch is inside one increment), which falls with
    // the increment; the difference itself is not monotone (the field steps alias with the relays' thresholds).
    double previousBound = 0.0;
    for (int increments : {37, 50, 400, 1201}) {
        const Ledger l = ledgerOf({{"model", "em.preisach"}, {"loading", {{"amplitudes", {1000.0, 300.0}}, {"increments", increments}}}});
        const Report r = evaluate(l, wholeSystem(l), {1e-6, 1e-2, 1e-15});
        const Check h = check(r, "hysteresis.loop-loss");
        EXPECT_EQ(h.status, CheckStatus::Verified) << h.details.dump();
        const double diff = std::abs(h.details["loopIntegral"].get<double>() - h.details["hysteronSum"].get<double>());
        const double bound = h.details["errorBound"].get<double>();
        EXPECT_LE(diff, bound) << increments;
        if (previousBound > 0.0)
            EXPECT_LT(bound, previousBound);
        previousBound = bound;
        // The energy balance uses the relays' dissipation: within the increment bound.
        const Check e = check(r, "energy.total");
        EXPECT_NE(e.attribution.find("within the stated numerical errors"), std::string::npos) << e.attribution;
    }
}
