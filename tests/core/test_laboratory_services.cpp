// The laboratory services (RES-LAB-003, -004, -015, -017): imports stored once with their
// raw bytes kept, invalid imports recorded, permissions, processing with provenance,
// budgets, propagation, evidence summaries, reports and exports.
#include "LaboratoryFixture.h"

#include "laboratory/Sha256.h"

#include <fstream>
#include <sstream>

using namespace laboratory_test;

TEST(LaboratoryServices, ImportsAreStoredOnceWithTheirRawBytes)
{
    Fixture f("a-resonator-frf.resonant");
    const json report = f.import("data/a-frf-run1.csv", "st-resonator", "calibration");
    ASSERT_TRUE(report["accepted"].get<bool>()) << report.dump();
    const std::string id = Fixture::datasetOf(report);
    // The dataset is in the experiment framework's records and store.
    auto list = f.s.laboratory.datasets(f.local());
    ASSERT_TRUE(list);
    ASSERT_EQ(list.value().size(), 1u);
    EXPECT_EQ(list.value()[0]["category"], "raw");
    EXPECT_EQ(list.value()[0]["provenance"], "synthetic or simulated");
    auto d = f.s.laboratory.dataset(f.local(), id, 50);
    ASSERT_TRUE(d) << d.error().message;
    EXPECT_EQ(d.value()["data"]["points"], 161);
    EXPECT_EQ(d.value()["data"]["axis"].size(), 50u);  // bounded
    // The original file, unchanged and read-only, under its digest.
    std::ifstream original(f.dir / "data" / "a-frf-run1.csv", std::ios::binary);
    std::ostringstream bytes;
    bytes << original.rdbuf();
    auto sources = f.s.laboratory.records(f.local(), "sources");
    ASSERT_EQ(sources.value().size(), 1u);
    EXPECT_EQ(sources.value()[0]["sha256"], laboratory::sha256Hex(bytes.str()));
    const fs::path kept = f.dir / (f.file.stem().string() + ".data") / sources.value()[0]["path"].get<std::string>();
    ASSERT_TRUE(fs::exists(kept)) << kept;
    EXPECT_EQ((fs::status(kept).permissions() & fs::perms::owner_write), fs::perms::none);
    // The same bytes again: recognized, not duplicated.
    auto again = f.s.laboratory.importDataset(f.local(), {{"path", "data/a-frf-run1.csv"}});
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code, ErrorCode::AlreadyExists);
    // A reader may not import; nor may a client without file access read a path.
    EXPECT_FALSE(f.s.laboratory.importDataset(f.reader(), {{"content", "time [s], voltage [V]\n0, 1\n1, 2\n"}, {"fileName", "x.csv"}}));
    // One undo removes the import.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_TRUE(f.s.laboratory.datasets(f.local()).value().empty());
}

TEST(LaboratoryServices, ImportsNeedASavedProjectAndInvalidOnesAreRecorded)
{
    {
        OwnedProjectHost host(std::make_unique<Project>(), 1);
        Session session(host);
        host.setSession(&session);
        auto r = session.services().laboratory.importDataset(CallContext{}, {{"content", "time [s], voltage [V]\n0, 1\n1, 2\n"}, {"fileName", "x.csv"}});
        ASSERT_FALSE(r);
        EXPECT_NE(r.error().message.find("save the project first"), std::string::npos);
    }
    Fixture f("g-invalid-data.resonant");
    const json bad = f.import("data/g-malformed.csv", "st-invalid");
    EXPECT_FALSE(bad["accepted"].get<bool>());
    EXPECT_GT(bad["errors"].get<int>(), 0);
    const json unit = f.import("data/g-missing-unit.csv", "st-invalid");
    EXPECT_FALSE(unit["accepted"].get<bool>());
    EXPECT_EQ(f.s.laboratory.records(f.local(), "imports").value().size(), 2u);  // recorded although nothing was created
    EXPECT_TRUE(f.s.laboratory.datasets(f.local()).value().empty());
    const json partial = f.import("data/g-malformed.csv", "st-invalid", "raw", true);
    EXPECT_TRUE(partial["accepted"].get<bool>());
    EXPECT_EQ(f.s.laboratory.datasets(f.local()).value().size(), 1u);
}

TEST(LaboratoryServices, ProcessingBudgetsAndPropagation)
{
    Fixture f("b-ring-down.resonant");
    const std::string id = Fixture::datasetOf(f.import("data/b-ring-down.csv", "st-ring-down"));
    auto p = f.s.laboratory.process(f.local(), id, {{"operation", "rms"}});
    ASSERT_TRUE(p) << p.error().message;
    EXPECT_EQ(p.value().result["category"], "processed");
    EXPECT_EQ(p.value().result["sourceIds"][0], id);
    EXPECT_FALSE(f.s.laboratory.process(f.local(), id, {{"operation", "rms"}}));  // reproducible: not duplicated
    auto rings = f.s.laboratory.resonances(f.local(), {{"dataset", id}, {"options", {{"minHz", 100.0}, {"maxHz", 1000.0}}}});
    ASSERT_TRUE(rings) << rings.error().message;
    EXPECT_NEAR(rings.value()["ringDown"]["frequencyHz"].get<double>(), 440.0, 0.05);
    EXPECT_NEAR(rings.value()["ringDown"]["quality"].get<double>(), 220.0, 5.0);
    auto b = f.s.laboratory.budget(f.local(), {{"quantity", "f"}, {"unit", "Hz"}, {"value", 440.0},
                                               {"components", {{{"id", "res"}, {"source", "resolution"}, {"distribution", "rectangular"}, {"value", 0.1}}}}});
    ASSERT_TRUE(b);
    EXPECT_NEAR(b.value()["combined"].get<double>(), 0.1 / std::sqrt(3.0), 1e-12);
    auto pr = f.s.laboratory.propagate(f.local(), {{"expression", "sqrt(k / m) / (2 * pi)"}, {"method", "linear"},
                                                   {"inputs", {{{"id", "k"}, {"mean", 2e5}, {"standard", 2e3}}, {{"id", "m"}, {"mean", 0.5}, {"standard", 0.0}}}}});
    ASSERT_TRUE(pr) << pr.error().message;
    EXPECT_NEAR(pr.value()["standard"].get<double>(), 0.5 * 0.01 * std::sqrt(2e5 / 0.5) / (2 * 3.14159265358979), 1e-3);
    EXPECT_FALSE(f.s.laboratory.propagate(f.local(), {{"expression", "a + b"}, {"inputs", {{{"id", "a"}, {"mean", 1.0}, {"standard", 0.1}}}}}));
}

TEST(LaboratoryServices, EvidenceSummaries)
{
    Fixture f("a-resonator-frf.resonant");
    const std::string id = Fixture::datasetOf(f.import("data/a-frf-run1.csv", "st-resonator", "calibration"));
    auto c = f.s.laboratory.compare(f.local(), {{"id", "cmp-nominal"}, {"study", "st-resonator"}, {"kind", "response"}, {"measured", id},
                                                {"predicted", {{"model", "resonator.lumped"},
                                                               {"configuration", {{"mass", 0.45}, {"stiffness", 2.2e5}, {"damping", 5.0}}}}}});
    ASSERT_TRUE(c) << c.error().message;
    const auto revision = f.session.projectRevision();
    auto e = f.s.laboratory.explain(f.local(), "cmp-nominal");
    ASSERT_TRUE(e);
    EXPECT_EQ(f.session.projectRevision(), revision);  // an explanation changes nothing
    bool synthetic = false, deviation = false;
    for (const json& x : e.value()["findings"])
        synthetic = synthetic || x["statement"].get<std::string>().find("synthetic") != std::string::npos;
    for (const json& h : e.value()["hypotheses"])
        deviation = deviation || h["statement"].get<std::string>().find("free length") != std::string::npos;
    EXPECT_TRUE(synthetic);
    EXPECT_TRUE(deviation);  // the recorded deviation is offered as a hypothesis, not a finding
    EXPECT_FALSE(f.s.laboratory.explain(f.local(), "nothing"));
}

TEST(LaboratoryServices, ReportsAndExports)
{
    Fixture f("a-resonator-frf.resonant");
    const std::string id = Fixture::datasetOf(f.import("data/a-frf-run1.csv", "st-resonator", "calibration"));
    auto md = f.s.laboratory.report(f.local(), "st-resonator", "markdown");
    ASSERT_TRUE(md) << md.error().message;
    EXPECT_NE(md.value().find("Identify the resonator"), std::string::npos);
    EXPECT_NE(md.value().find("free length"), std::string::npos);
    EXPECT_NE(md.value().find("not validated"), std::string::npos);
    auto html = f.s.laboratory.report(f.local(), "st-resonator", "html");
    ASSERT_TRUE(html);
    EXPECT_NE(html.value().find("<h1>"), std::string::npos);
    EXPECT_FALSE(f.s.laboratory.report(f.local(), "st-resonator", "pdf"));
    auto csv = f.s.laboratory.exportDataset(f.local(), id, "csv");
    ASSERT_TRUE(csv);
    EXPECT_NE(csv.value().find("real, imag"), std::string::npos);
    auto js = f.s.laboratory.exportDataset(f.local(), id, "json");
    ASSERT_TRUE(js);
    EXPECT_EQ(json::parse(js.value())["data"]["points"], 161);
    auto models = f.s.laboratory.models(f.local());
    EXPECT_EQ(models.value().size(), 8u);  // with matter.stack-metric (REQ-CHAT-20261127-001)
}
