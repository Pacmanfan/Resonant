// Laboratory studies (RES-LAB-002): their JSON, calibration and validation data kept
// apart, persistence in the project, fingerprints, and links that go stale when the linked
// content changes.
#include "LaboratoryFixture.h"

#include "coupling/LaboratoryStudies.h"
#include "laboratory/Laboratory.h"

using namespace laboratory_test;
namespace lab = resonant::laboratory;

TEST(LaboratoryModel, StudiesRoundTripAndKeepCalibrationApartFromValidation)
{
    lab::Study s;
    s.id = "st-1";
    s.name = "A study";
    s.objective = "find k";
    s.models = {{"material", "mat-a", "abc", "", ""}};
    s.deviations = {{"d1", "length", "m", "caliper", "as built", "", 0.1, 0.098, 0.0005}};
    s.datasets = {{"m1", lab::DatasetRole::Calibration, ""}, {"m2", lab::DatasetRole::Validation, ""}};
    auto back = lab::Study::fromJson(s.toJson());
    ASSERT_TRUE(back) << back.message();
    EXPECT_EQ(back.value().toJson(), s.toJson());
    EXPECT_EQ(back.value().datasetsWith(lab::DatasetRole::Validation), std::vector<std::string>{"m2"});
    s.datasets.push_back({"m1", lab::DatasetRole::Validation, ""});
    auto both = s.validate();
    ASSERT_FALSE(both);
    EXPECT_NE(both.message().find("independent"), std::string::npos);
    EXPECT_FALSE(lab::ModelLink::fromJson({{"kind", "horoscope"}, {"id", "x"}}));
    EXPECT_EQ(lab::fingerprint(json{{"a", 1}}), lab::fingerprint(json{{"a", 1}}));
    EXPECT_NE(lab::fingerprint(json{{"a", 1}}), lab::fingerprint(json{{"a", 2}}));
}

TEST(LaboratoryModel, LinksRecordTheirRevisionAndGoStale)
{
    Fixture f("c-thermal-slab.resonant");
    auto st = f.s.laboratory.study(f.local(), "st-thermal");
    ASSERT_TRUE(st) << st.error().message;
    json doc = st.value();
    doc.erase("stale");
    ASSERT_TRUE(f.s.laboratory.setStudy(f.local(), doc));
    st = f.s.laboratory.study(f.local(), "st-thermal");
    for (const json& m : st.value()["models"])
        EXPECT_EQ(m["revision"].get<std::string>().size(), 16u);
    EXPECT_TRUE(st.value()["stale"].empty());
    // Change the material: the link goes stale.
    Material* mat = f.host.project().materials.findMutable(Id("mat-ex-lab-polymer"));
    ASSERT_NE(mat, nullptr);
    mat->setDescription("edited");
    st = f.s.laboratory.study(f.local(), "st-thermal");
    ASSERT_EQ(st.value()["stale"].size(), 1u);
    EXPECT_NE(st.value()["stale"][0].get<std::string>().find("changed since it was linked"), std::string::npos);
    // A link to nothing is refused.
    doc["models"].push_back({{"kind", "material"}, {"id", "mat-none"}});
    EXPECT_FALSE(f.s.laboratory.setStudy(f.local(), doc));
}

TEST(LaboratoryModel, RecordsSurviveSaveAndReload)
{
    Fixture f("a-resonator-frf.resonant");
    const json report = f.import("data/a-frf-run1.csv", "st-resonator", "calibration");
    ASSERT_TRUE(report["accepted"].get<bool>());
    auto copy = deserializeProject(serializeProject(f.host.project()));
    ASSERT_TRUE(copy);
    auto w = lab::workspaceOf(*copy.value().project);
    ASSERT_TRUE(w) << w.message();
    ASSERT_EQ(w.value().imports.size(), 1u);
    ASSERT_NE(w.value().study("st-resonator"), nullptr);
    EXPECT_EQ(w.value().study("st-resonator")->status, "measured");
    EXPECT_EQ(w.value().study("st-resonator")->datasetsWith(lab::DatasetRole::Calibration).size(), 1u);
    EXPECT_EQ(w.value().sources.size(), 1u);
}
