// MatterService (RES-FAM-019): architectures set (validated), duplicated as
// alternatives, compared side by side, deleted - each one undo step - analyses,
// checks, exploration, manufacturability and pathways that change nothing, a
// stack added to the coupled engine, manufacturing records, reports and layer
// tables, and the permissions.
#include "LaboratoryFixture.h"

#include "matter/MatterArchitecture.h"

#include <gtest/gtest.h>

using namespace laboratory_test;

TEST(MatterServices, ArchitecturesAreSetValidatedDuplicatedAndUndone)
{
    Fixture f("b-energy-converter.resonant", "matter");
    auto list = f.s.matter.architectures(f.local());
    ASSERT_TRUE(list) << list.error().message;
    ASSERT_EQ(list.value().size(), 2u);
    EXPECT_EQ(list.value()[0]["errors"], 0);
    auto one = f.s.matter.architecture(f.local(), "arch-bonded");
    ASSERT_TRUE(one);
    json doc = one.value();
    doc.erase("revision");
    doc.erase("issues");
    // An invalid architecture is refused with the validation's reason.
    json bad = doc;
    bad["stacks"][0]["layers"][1]["thickness"] = -1.0;
    auto refused = f.s.matter.setArchitecture(f.local(), bad);
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.error().message.find("positive thickness"), std::string::npos);
    // A valid change is one undo step.
    doc["stacks"][0]["layers"][3]["thickness"] = 3e-4;
    auto set = f.s.matter.setArchitecture(f.local(), doc);
    ASSERT_TRUE(set) << set.error().message;
    EXPECT_NE(set.value().result["revision"], one.value()["revision"]);
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.s.matter.architecture(f.local(), "arch-bonded").value()["revision"], one.value()["revision"]);
    // An alternative keeps its parent and loses the manufacturing history.
    auto alt = f.s.matter.duplicateArchitecture(f.local(), "arch-bonded", "arch-bonded-2", "");
    ASSERT_TRUE(alt) << alt.error().message;
    auto copy = f.s.matter.architecture(f.local(), "arch-bonded-2");
    ASSERT_TRUE(copy);
    EXPECT_EQ(copy.value()["parent"], "arch-bonded");
    EXPECT_TRUE(copy.value()["records"].empty());
    EXPECT_FALSE(f.s.matter.duplicateArchitecture(f.local(), "arch-bonded", "arch-bonded-2", ""));
    ASSERT_TRUE(f.s.matter.deleteArchitecture(f.local(), "arch-bonded-2"));
    EXPECT_FALSE(f.s.matter.architecture(f.local(), "arch-bonded-2"));
    EXPECT_FALSE(f.s.matter.deleteArchitecture(f.local(), "arch-none"));
    // Readers read; they do not change.
    EXPECT_TRUE(f.s.matter.architectures(f.reader()));
    EXPECT_FALSE(f.s.matter.setArchitecture(f.reader(), doc));
    auto v = f.s.matter.validate(f.local(), bad);
    ASSERT_TRUE(v);
    EXPECT_FALSE(v.value()["valid"].get<bool>());
}

TEST(MatterServices, AnalysesChecksExplorationAndPathwaysChangeNothing)
{
    Fixture f("b-energy-converter.resonant", "matter");
    const auto revision = f.s.project.get(f.local()).value().revision;
    auto caps = f.s.matter.capabilities(f.local());
    ASSERT_TRUE(caps);
    EXPECT_GE(caps.value()["analyses"].size(), 7u);
    EXPECT_GE(caps.value()["interfaceModels"].size(), 10u);
    EXPECT_GE(caps.value()["processes"].size(), 19u);
    auto check = f.s.matter.check(f.local(), "arch-bonded", "st-beam", "electromagnetic");
    ASSERT_TRUE(check) << check.error().message;
    auto analysis = f.s.matter.analyze(f.local(), "arch-bonded", "st-beam", "harvester", {{"tipMass", 0.002}});
    ASSERT_TRUE(analysis) << analysis.error().message;
    EXPECT_GT(analysis.value()["metrics"]["power"].get<double>(), 0.0);
    auto refusedAnalysis = f.s.matter.analyze(f.local(), "arch-bonded", "st-beam", "quantum", json::object());
    EXPECT_FALSE(refusedAnalysis);
    auto props = f.s.matter.effectiveProperties(f.local(), "arch-bonded", "pzt");
    ASSERT_TRUE(props);
    EXPECT_TRUE(props.value()["anisotropic"].contains("piezoelectric"));
    auto make = f.s.matter.manufacturability(f.local(), "arch-film");
    ASSERT_TRUE(make);
    EXPECT_EQ(make.value()["status"], "infeasible");
    EXPECT_FALSE(make.value()["residualStresses"].empty());
    auto ex = f.s.matter.explore(f.local(), "arch-film");
    ASSERT_TRUE(ex);
    EXPECT_FALSE(ex.value()["findings"].empty());
    auto mech = f.s.matter.mechanisms(f.local(), "mechanical", "electrical", "arch-bonded");
    ASSERT_TRUE(mech);
    EXPECT_TRUE(mech.value()["modeled"].get<bool>());
    auto path = f.s.matter.pathway(f.local(), "arch-bonded", "f-harvest", json::object());
    ASSERT_TRUE(path) << path.error().message;
    EXPECT_TRUE(path.value()["complete"].get<bool>());
    EXPECT_EQ(f.s.project.get(f.local()).value().revision, revision) << "nothing was changed";
}

TEST(MatterServices, ComparisonsSystemsRecordsReportsAndLayerTables)
{
    Fixture f("b-energy-converter.resonant", "matter");
    auto cmp = f.s.matter.compare(f.local(), {{"architectures", json::array({"arch-bonded", "arch-film"})}, {"keep", true}});
    ASSERT_TRUE(cmp) << cmp.error().message;
    const json& m = cmp.value().result["metrics"];
    ASSERT_TRUE(m.contains("modes.f1"));
    EXPECT_TRUE(m["modes.f1"].contains("arch-bonded") && m["modes.f1"].contains("arch-film"));
    EXPECT_EQ(m["manufacturing.status"]["arch-film"], "infeasible");
    EXPECT_FALSE(cmp.value().result.contains("score"));
    EXPECT_TRUE(cmp.value().result["kept"].get<bool>());
    EXPECT_FALSE(f.s.matter.compare(f.local(), {{"architectures", json::array({"arch-bonded"})}}));
    // A stack in the coupled engine: the segmented electrode is refused (the slab cannot hold it) ...
    auto segmented = f.s.matter.createPhysicalSystem(f.local(), "arch-bonded", "st-beam", {{"id", "sys-beam"}});
    ASSERT_FALSE(segmented);
    EXPECT_NE(segmented.error().message.find("segmented"), std::string::npos) << segmented.error().message;
    // ... a uniform one is a physical system: one undo step, refused a second time unless replaced.
    json uniform = f.s.matter.architecture(f.local(), "arch-bonded").value();
    uniform.erase("revision");
    uniform.erase("issues");
    uniform["stacks"][0]["layers"][4]["pattern"] = {{"kind", "uniform"}};
    ASSERT_TRUE(f.s.matter.setArchitecture(f.local(), uniform));
    auto sys = f.s.matter.createPhysicalSystem(f.local(), "arch-bonded", "st-beam", {{"id", "sys-beam"}});
    ASSERT_TRUE(sys) << sys.error().message;
    EXPECT_EQ(sys.value().result["system"]["id"], "sys-beam");
    auto systems = f.s.multiphysics.systems(f.local());
    ASSERT_TRUE(systems);
    EXPECT_EQ(systems.value().size(), 1u);
    EXPECT_FALSE(f.s.matter.createPhysicalSystem(f.local(), "arch-bonded", "st-beam", {{"id", "sys-beam"}}));
    EXPECT_TRUE(f.s.matter.createPhysicalSystem(f.local(), "arch-bonded", "st-beam", {{"id", "sys-beam"}, {"replace", true}}));
    // A manufacturing record.
    auto rec = f.s.matter.addRecord(f.local(), "arch-bonded",
                                    {{"id", "rec-b-002"}, {"process", "proc-lam"}, {"batch", "B2"}, {"measuredDatasets", json::array({"ds-frf-run1"})}});
    ASSERT_TRUE(rec) << rec.error().message;
    EXPECT_EQ(f.s.matter.architecture(f.local(), "arch-bonded").value()["records"].size(), 2u);
    EXPECT_FALSE(f.s.matter.addRecord(f.local(), "arch-bonded", {{"id", "rec-b-003"}, {"process", "proc-none"}}));
    // The report and the layer table.
    auto md = f.s.matter.report(f.local(), "arch-film", "markdown");
    ASSERT_TRUE(md) << md.error().message;
    EXPECT_NE(md.value().find("## Manufacturing: infeasible"), std::string::npos);
    EXPECT_NE(md.value().find("Hypotheses"), std::string::npos);
    auto html = f.s.matter.report(f.local(), "arch-bonded", "html");
    ASSERT_TRUE(html);
    EXPECT_NE(html.value().find("<h2>"), std::string::npos);
    EXPECT_FALSE(f.s.matter.report(f.local(), "arch-bonded", "pdf"));
    auto csv = f.s.matter.exportLayers(f.local(), "arch-bonded", "st-beam", "csv");
    ASSERT_TRUE(csv);
    EXPECT_NE(csv.value().find("pzt.0,pzt,transduction,mat-fam-pzt5a,0.0002"), std::string::npos) << csv.value();
    auto layersJson = f.s.matter.exportLayers(f.local(), "arch-bonded", "st-beam", "json");
    ASSERT_TRUE(layersJson);
    EXPECT_EQ(json::parse(layersJson.value())["layers"].size(), 6u);
}
