// The laboratory examples (RES-LAB-018): each study's files imported with their roles and
// its analysis run, against the known ground truth of the synthetic data.
#include "LaboratoryFixture.h"

#include <cmath>

using namespace laboratory_test;

namespace {

// Imports the study's files; the ids by role (several files per role are kept in order).
std::map<std::string, std::vector<json>> importStudy(Fixture& f, const std::string& study)
{
    std::map<std::string, std::vector<json>> out;
    const json st = f.s.laboratory.study(f.local(), study).value();
    for (const json& file : st["conditions"]["files"])
        out[file["role"].get<std::string>()].push_back(f.import(file["path"], study, file["role"]));
    return out;
}

}  // namespace

TEST(LaboratoryExamples, EveryExampleLoadsWithItsStudies)
{
    for (const char* file : {"a-resonator-frf.resonant", "b-ring-down.resonant", "c-thermal-slab.resonant", "d-dielectric-reflection.resonant",
                             "e-stress-strain.resonant", "f-energy-balance.resonant", "g-invalid-data.resonant"}) {
        Fixture f(file);
        auto studies = f.s.laboratory.studies(f.local());
        ASSERT_TRUE(studies) << file;
        ASSERT_EQ(studies.value().size(), 1u) << file;
        EXPECT_FALSE(studies.value()[0]["objective"].get<std::string>().empty()) << file;
    }
}

TEST(LaboratoryExamples, RingDownDielectricAndCoupon)
{
    {
        Fixture f("b-ring-down.resonant");
        auto files = importStudy(f, "st-ring-down");
        const std::string id = Fixture::datasetOf(files["raw"][0]);
        const json predicted = f.s.laboratory.study(f.local(), "st-ring-down").value()["conditions"]["predicted"];
        auto c = f.s.laboratory.compare(f.local(), {{"id", "cmp-fork"}, {"study", "st-ring-down"}, {"kind", "resonance"}, {"measured", id},
                                                    {"predicted", predicted}, {"options", {{"minHz", 100.0}, {"maxHz", 1000.0}}}});
        ASSERT_TRUE(c) << c.error().message;
        EXPECT_NEAR(c.value().result["extracted"]["frequencyHz"].get<double>(), 440.0, 0.05);
        EXPECT_EQ(c.value().result["result"]["frequency"]["verdict"], "consistent");
        EXPECT_EQ(c.value().result["result"]["quality"]["verdict"], "inconsistent");  // Q 220 measured against 300 +- 10 predicted
    }
    {
        Fixture f("d-dielectric-reflection.resonant");
        auto files = importStudy(f, "st-dielectric");
        const json model = f.s.laboratory.study(f.local(), "st-dielectric").value()["conditions"]["model"];
        const json cal = f.calibrate({{"id", "cal-sheet"}, {"study", "st-dielectric"}, {"model", model["id"]}, {"configuration", model["configuration"]},
                                      {"parameters", {{{"id", "sheet.permittivity"}}, {{"id", "sheet.lossTangent"}}}},
                                      {"datasets", {Fixture::datasetOf(files["calibration"][0])}}, {"observable", "real-imag"}});
        EXPECT_NEAR((*parameter(cal, "sheet.permittivity"))["estimate"].get<double>(), 4.2, 0.02);
        auto v = f.s.laboratory.validate(f.local(), "cal-sheet", {Fixture::datasetOf(files["validation"][0])}, "val-sheet");
        ASSERT_TRUE(v) << v.error().message;
        EXPECT_EQ(v.value().result["status"], "validated within uncertainty");
    }
    Fixture f("e-stress-strain.resonant");
    auto files = importStudy(f, "st-coupon");
    const json cal = f.calibrate({{"id", "cal-coupon"}, {"study", "st-coupon"}, {"model", "material-state.j2-uniaxial"},
                                  {"parameters", {{{"id", "yieldStress"}}, {{"id", "isotropicHardening"}}}},
                                  {"datasets", {Fixture::datasetOf(files["calibration"][0], "stress")}},
                                  {"axisDatasets", {Fixture::datasetOf(files["calibration"][0], "strain")}}, {"observable", "value"}});
    ASSERT_EQ(cal["status"], "calibrated") << cal.dump().substr(0, 600);
    auto v = f.s.laboratory.validate(f.local(), "cal-coupon", {Fixture::datasetOf(files["validation"][0], "stress")}, "val-coupon",
                                     {Fixture::datasetOf(files["validation"][0], "strain")});
    ASSERT_TRUE(v) << v.error().message;
    EXPECT_EQ(v.value().result["status"], "validated within uncertainty") << v.value().result.dump();
}
