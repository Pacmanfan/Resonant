// Calibration and validation (RES-LAB-008..010, RES-LAB-018 benchmarks 24.1, 24.3, 24.8):
// known parameters recovered from synthetic data within their uncertainty; identifiability
// when too many parameters are freed; validation only on other data; the calibration
// applied explicitly as a new revision, never over the original.
#include "LaboratoryFixture.h"

#include "materialstate/StateModel.h"

using namespace laboratory_test;

TEST(LaboratoryCalibration, AResonatorIsCalibratedAndValidatedOnAnotherRun)
{
    Fixture f("a-resonator-frf.resonant");
    const std::string run1 = Fixture::datasetOf(f.import("data/a-frf-run1.csv", "st-resonator", "calibration"));
    const std::string run2 = Fixture::datasetOf(f.import("data/a-frf-run2.csv", "st-resonator", "validation"));
    const json cal = f.calibrate({{"id", "cal-frf"}, {"study", "st-resonator"}, {"model", "resonator.lumped"},
                                  {"configuration", {{"response", "receptance"}, {"mass", 0.45}, {"stiffness", 2.2e5}, {"damping", 5.0}}},
                                  {"parameters", {{{"id", "mass"}}, {{"id", "stiffness"}}, {{"id", "damping"}}}},
                                  {"datasets", {run1}}, {"observable", "magnitude"}});
    ASSERT_EQ(cal["status"], "calibrated") << cal.dump().substr(0, 600);
    for (const auto& [id, truth] : std::vector<std::pair<std::string, double>>{{"mass", 0.5}, {"stiffness", 2.0e5}, {"damping", 6.0}}) {
        const json* p = parameter(cal, id);
        ASSERT_NE(p, nullptr);
        EXPECT_LE((*p)["lower"].get<double>(), truth) << id;
        EXPECT_GE((*p)["upper"].get<double>(), truth) << id;
        EXPECT_EQ((*p)["identifiability"], "identified") << id;
    }
    EXPECT_NE(cal["notes"].dump().find("synthetic"), std::string::npos);
    // Validation on the calibration data is refused; on the other run it passes.
    auto same = f.s.laboratory.validate(f.local(), "cal-frf", {run1}, "val-same");
    ASSERT_FALSE(same);
    EXPECT_NE(same.error().message.find("not independent"), std::string::npos);
    auto val = f.s.laboratory.validate(f.local(), "cal-frf", {run2}, "val-frf");
    ASSERT_TRUE(val) << val.error().message;
    EXPECT_EQ(val.value().result["status"], "validated within uncertainty") << val.value().result.dump();
    EXPECT_LT(val.value().result["validationPerformance"][0]["calibratedRms"].get<double>(),
              val.value().result["validationPerformance"][0]["originalRms"].get<double>());
    EXPECT_EQ(f.s.laboratory.study(f.local(), "st-resonator").value()["status"], "validated");
    // Nothing in the project holds a lumped resonator's parameters: applying is refused, the record keeps them.
    EXPECT_FALSE(f.s.laboratory.applyCalibration(f.local(), "cal-frf", json::object()));
}

TEST(LaboratoryCalibration, IdentifiabilityWhenTooManyParametersAreFree)
{
    Fixture f("d-dielectric-reflection.resonant");
    const std::string s11 = Fixture::datasetOf(f.import("data/d-s11.csv", "st-dielectric", "calibration"));
    const json cfg{{"layers", {{{"name", "sheet"}, {"thickness", 0.005}, {"permittivity", 4.0}, {"lossTangent", 0.01}}}}, {"backing", "metal"}};
    const json two = f.calibrate({{"id", "cal-two"}, {"model", "em.layered-reflection"}, {"configuration", cfg},
                                  {"parameters", {{{"id", "sheet.permittivity"}}, {{"id", "sheet.lossTangent"}}}}, {"datasets", {s11}}, {"observable", "real-imag"}});
    ASSERT_EQ(two["status"], "calibrated") << two.dump().substr(0, 800);
    EXPECT_NEAR((*parameter(two, "sheet.permittivity"))["estimate"].get<double>(), 4.2, 0.02);
    EXPECT_NEAR((*parameter(two, "sheet.lossTangent"))["estimate"].get<double>(), 0.02, 0.002);
    // The thickness freed too: permittivity and thickness enter mostly as a product (the electrical length).
    const json three = f.calibrate({{"id", "cal-three"}, {"model", "em.layered-reflection"}, {"configuration", cfg},
                                    {"parameters", {{{"id", "sheet.permittivity"}}, {{"id", "sheet.lossTangent"}}, {{"id", "sheet.thickness"}}}},
                                    {"datasets", {s11}}, {"observable", "magnitude"}});
    bool flagged = three["status"] != "calibrated";
    for (const json& n : three["notes"])
        flagged = flagged || n.get<std::string>().find("correlated") != std::string::npos;
    EXPECT_TRUE(flagged) << three.dump().substr(0, 1200);
    // Only the model's physical parameters can be varied: no correction factor.
    auto fudge = f.s.laboratory.calibrate(f.local(), {{"model", "em.layered-reflection"}, {"configuration", cfg},
                                                      {"parameters", {{{"id", "correctionFactor"}}}}, {"datasets", {s11}}});
    ASSERT_TRUE(fudge);
    ASSERT_TRUE(f.waitForJobs());
    auto view = f.s.jobs.get(f.local(), fudge.value().id);
    EXPECT_EQ(view.value().state, "failed");
}

TEST(LaboratoryCalibration, J2PlasticityAppliedAsANewStudyRevision)
{
    Fixture f("e-stress-strain.resonant");
    const json report = f.import("data/e-stress-strain.csv", "st-coupon", "calibration");
    const std::string stress = Fixture::datasetOf(report, "stress"), strain = Fixture::datasetOf(report, "strain");
    const json cal = f.calibrate({{"id", "cal-j2"}, {"study", "st-coupon"}, {"model", "material-state.j2-uniaxial"},
                                  {"configuration", {{"youngsModulus", 200e9}}},
                                  {"parameters", {{{"id", "yieldStress"}, {"initial", 250e6}}, {{"id", "isotropicHardening"}, {"initial", 2e9}}}},
                                  {"datasets", {stress}}, {"axisDatasets", {strain}}, {"observable", "value"}});
    ASSERT_EQ(cal["status"], "calibrated") << cal.dump().substr(0, 800);
    EXPECT_NEAR((*parameter(cal, "yieldStress"))["estimate"].get<double>(), 260e6, 2e6);
    EXPECT_NEAR((*parameter(cal, "isotropicHardening"))["estimate"].get<double>(), 2.5e9, 0.2e9);
    auto applied = f.s.laboratory.applyCalibration(f.local(), "cal-j2", {{"study", "st-steel-nominal"}});
    ASSERT_TRUE(applied) << applied.error().message;
    auto ws = materialstate::workspaceOf(f.host.project());
    ASSERT_EQ(ws.value().studies.size(), 2u);
    EXPECT_DOUBLE_EQ(ws.value().studies[0]["parameters"]["yieldStress"].get<double>(), 250e6);  // the original is unchanged
    EXPECT_NEAR(ws.value().studies[1]["parameters"]["yieldStress"].get<double>(), 260e6, 2e6);
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(materialstate::workspaceOf(f.host.project()).value().studies.size(), 1u);
}
