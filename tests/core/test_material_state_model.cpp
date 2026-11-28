// The material-state model and catalogue (RES-DMS-001, -003, -013): variables with their
// classes, fields and summaries, records and workspaces that round-trip, every implemented
// model running its loading presets, and the catalogue naming what is not implemented.
#include "coupling/MaterialStateModels.h"
#include "materialstate/StateModel.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

using namespace resonant;
namespace mst = resonant::materialstate;
using nlohmann::json;

TEST(MaterialStateModel, FieldsSummarizeAndRecordsRoundTrip)
{
    mst::StateField f;
    f.model = "mechanics.damage.point";
    for (double d : {0.1, 0.4, 0.2})
        f.points.push_back({{"damage", {d}}});
    const auto s = f.summary("damage");
    EXPECT_DOUBLE_EQ(s.min, 0.1);
    EXPECT_DOUBLE_EQ(s.max, 0.4);
    EXPECT_NEAR(s.mean, 0.7 / 3, 1e-15);
    EXPECT_EQ(mst::toString(mst::VariableClass::Irreversible), "irreversible");
    Project p;
    mst::Workspace w;
    w.studies.push_back({{"id", "st-1"}, {"model", "mechanics.j2.point"}});
    mst::StateRecord r;
    r.id = "rec-1";
    r.model = "mechanics.j2.point";
    r.history = json::array({{{"step", 1}, {"stress", 1e8}}});
    r.committable = json::array({{{"domain", "mechanical"}, {"key", "youngsModulus"}, {"value", 1e9}}});
    w.records.push_back(r);
    mst::storeWorkspace(p, w);
    auto back = deserializeProject(serializeProject(p));
    ASSERT_TRUE(back) << back.message();
    auto w2 = mst::workspaceOf(*back.value().project);
    ASSERT_TRUE(w2) << w2.message();
    ASSERT_NE(w2.value().record("rec-1"), nullptr);
    EXPECT_EQ(w2.value().record("rec-1")->toJson(), r.toJson());
    EXPECT_EQ(w2.value().studies, w.studies);
    EXPECT_FALSE(mst::StateRecord::fromJson(json::object()));
}

TEST(MaterialStateModel, EveryImplementedModelRunsItsPresets)
{
    int implemented = 0;
    for (const auto& card : coupling::materialStateModels()) {
        if (card.status != "implemented" || card.id == "coupled.evolution")
            continue;
        ++implemented;
        EXPECT_FALSE(card.variables.empty()) << card.id;
        EXPECT_FALSE(card.parameters.empty()) << card.id;
        ASSERT_FALSE(card.loadings.empty()) << card.id;
        for (const json& preset : card.loadings) {
            auto r = coupling::runMaterialStateStudy({{"model", card.id}, {"loading", preset["loading"]}}, nullptr);
            ASSERT_TRUE(r) << card.id << " " << preset["id"] << ": " << r.message();
            EXPECT_EQ(r.value()["format"], coupling::kStateRunFormat);
            EXPECT_FALSE(r.value()["curves"].empty()) << card.id;
            EXPECT_FALSE(r.value()["history"].empty()) << card.id;
            EXPECT_TRUE(r.value().contains("reliable"));
        }
    }
    EXPECT_EQ(implemented, 7);
}

TEST(MaterialStateModel, CatalogueSaysWhatIsNotImplemented)
{
    std::vector<std::string> missing;
    for (const auto& card : coupling::materialStateModels())
        if (card.status == "not implemented") {
            missing.push_back(card.id);
            EXPECT_FALSE(card.excluded.empty()) << card.id;
            auto r = coupling::runMaterialStateStudy({{"model", card.id}}, nullptr);
            ASSERT_FALSE(r);
            EXPECT_NE(r.message().find("not implemented"), std::string::npos);
        }
    for (const char* id : {"mechanics.fracture.propagation", "mechanics.fatigue", "mechanics.hyperelastic", "optics.nonlinear", "acoustics.nonlinear"})
        EXPECT_NE(std::find(missing.begin(), missing.end(), id), missing.end()) << id;
    EXPECT_FALSE(coupling::runMaterialStateStudy({{"model", "no.such.model"}}, nullptr));
    EXPECT_FALSE(coupling::runMaterialStateStudy({{"model", "mechanics.j2.point"}, {"parameters", {{"yieldStress", -1.0}}}, {"loading", {{"amplitudes", {0.01}}}}},
                                                 nullptr));
}
