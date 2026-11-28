// The accounting services (RES-ACC-016): families and conversions, studies stored undoably,
// runs as jobs, reports for any system without running again, comparisons, exports,
// records that survive save and reload, permissions; and the coupled engine's results
// carrying their own accounting.
#include "AccountingFixture.h"

#include "coupling/AccountingStudies.h"

using namespace accounting_test;

namespace {

const json* checkOf(const json& report, const std::string& model)
{
    for (const json& c : report["checks"])
        if (c["model"] == model)
            return &c;
    return nullptr;
}

}  // namespace

TEST(AccountingServices, FamiliesConversionsAndModels)
{
    Fixture f("a-closed-collision.resonant");
    auto fam = f.s.accounting.families(f.local());
    ASSERT_TRUE(fam);
    bool torque = false;
    for (const json& x : fam.value())
        torque = torque || (x["id"] == "torque" && x["conserved"] == false);
    EXPECT_TRUE(torque);
    auto kwh = f.s.accounting.convert(f.local(), 2.0, "kWh", "MJ");
    ASSERT_TRUE(kwh);
    EXPECT_NEAR(kwh.value()["value"].get<double>(), 7.2, 1e-12);
    EXPECT_FALSE(f.s.accounting.convert(f.local(), 1.0, "J", "W"));
    EXPECT_GE(f.s.accounting.checkModels(f.local()).value().size(), 11u);
    EXPECT_EQ(f.s.accounting.sourceKinds(f.local()).value().size(), 6u);
}

TEST(AccountingServices, RunReclassifyCompareExportAndKeep)
{
    Fixture f("d-coupled-resonators.resonant");
    ASSERT_EQ(f.s.accounting.studies(f.local()).value().size(), 1u);
    const json r = f.ran(f.s.accounting.run(f.local(), "st-beats", json()));
    ASSERT_EQ(r["format"], coupling::kAccountingResultFormat);
    ASSERT_EQ(r["reports"].size(), 3u);  // whole, r1, r2
    EXPECT_EQ((*checkOf(r["reports"][0], "energy.total"))["status"], "verified");
    // Another system, without running again.
    const std::string id = r["resultId"];
    auto rep = f.s.accounting.report(f.local(), id, {{"id", "r1-only"}, {"kind", "object"}, {"members", {"body:r1"}}});
    ASSERT_TRUE(rep) << rep.error().message;
    EXPECT_EQ((*checkOf(rep.value(), "energy.total"))["status"], "verified");
    EXPECT_FALSE(f.s.accounting.report(f.local(), id, {{"id", "bad"}, {"members", {"nobody"}}}));
    auto cmp = f.s.accounting.compare(f.local(), id, json::array({json::object(), {{"id", "r1"}, {"members", {"body:r1", "element:k1"}}}}));
    ASSERT_TRUE(cmp) << cmp.error().message;
    bool coupling = false;
    for (const json& d : cmp.value()["differences"])
        coupling = coupling || (d["exchange"] == "work:r1->coupling" && d["roles"][0] == "internal" && d["roles"][1] == "output");
    EXPECT_TRUE(coupling) << cmp.value()["differences"].dump();
    for (const char* format : {"json", "csv", "markdown"}) {
        auto e = f.s.accounting.exportResult(f.local(), id, format);
        ASSERT_TRUE(e) << format;
        EXPECT_FALSE(e.value().empty());
    }
    EXPECT_NE(f.s.accounting.exportResult(f.local(), id, "markdown").value().find("| Check | Status |"), std::string::npos);
    EXPECT_FALSE(f.s.accounting.exportResult(f.local(), id, "pdf"));
    // Keep it: a reader may not; the record survives save and reload with its reports evaluated again.
    EXPECT_FALSE(f.s.accounting.saveRecord(f.reader(), id, "acc-beats", "Beats"));
    ASSERT_TRUE(f.s.accounting.saveRecord(f.local(), id, "acc-beats", "Energy beats"));
    auto copy = deserializeProject(serializeProject(f.host.project()));
    ASSERT_TRUE(copy);
    const json kept = copy.value().project->extensions["accounting"]["records"][0];
    EXPECT_EQ(kept["id"], "acc-beats");
    EXPECT_LE(kept["retainedSamples"].get<int>(), 200);  // selective retention
    auto rec = f.s.accounting.record(f.local(), "acc-beats");
    ASSERT_TRUE(rec) << rec.error().message;
    ASSERT_EQ(rec.value()["reports"].size(), 3u);
    EXPECT_EQ((*checkOf(rec.value()["reports"][0], "energy.total"))["status"], "verified");
    EXPECT_EQ(f.s.accounting.records(f.local()).value().size(), 1u);
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_TRUE(f.s.accounting.records(f.local()).value().empty());
}

TEST(AccountingServices, StudiesAreValidatedAndUndoable)
{
    Fixture f("a-closed-collision.resonant");
    EXPECT_FALSE(f.s.accounting.setStudy(f.local(), {{"id", "x"}, {"source", {{"kind", "perpetual-motion"}}}}));
    const json layer{{"thickness", 0.01}, {"permittivity", 3.0}, {"lossTangent", 0.1}};
    const json source{{"kind", "radiation"}, {"intensity", 10.0}, {"layers", json::array({layer})}};
    const json study{{"id", "st-new"}, {"name", "A new one"}, {"source", source}};
    ASSERT_TRUE(f.s.accounting.setStudy(f.local(), study));
    EXPECT_EQ(f.s.accounting.studies(f.local()).value().size(), 2u);
    const json r = f.ran(f.s.accounting.run(f.local(), "st-new", json()));
    EXPECT_EQ((*checkOf(r["reports"][0], "radiation.force-methods"))["status"], "verified");
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.s.accounting.studies(f.local()).value().size(), 1u);
    EXPECT_FALSE(f.s.accounting.run(f.local(), "st-none", json()));
    EXPECT_FALSE(f.s.accounting.result(f.local(), "no-such-result"));
}

TEST(AccountingServices, CoupledResultsCarryTheirAccounting)
{
    Fixture f("e-microwave-absorption.resonant");
    auto job = f.s.multiphysics.start(f.local(), "sys-microwave-food", json());
    ASSERT_TRUE(job) << job.error().message;
    ASSERT_TRUE(f.waitForJobs());
    auto view = f.s.jobs.get(f.local(), job.value().id);
    ASSERT_TRUE(view);
    ASSERT_FALSE(view.value().resultIds.empty());
    auto doc = f.s.multiphysics.result(f.local(), view.value().resultIds[0]);
    ASSERT_TRUE(doc);
    ASSERT_TRUE(doc.value()->contains("accounting"));
    const json& report = (*doc.value())["accounting"]["report"];
    EXPECT_EQ((*checkOf(report, "energy.total"))["status"], "verified");
    EXPECT_EQ((*checkOf(report, "momentum.linear"))["status"], "insufficient data");
}
