// Comparing measurement and prediction (RES-LAB-005, -006): E_n for scalars; frequency
// responses aligned on their overlap with peak offsets; transients with the best shift;
// spatial fields after a frame transform; incompatible data refused.
#include "laboratory/Validation.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::laboratory;
namespace ex = resonant::experiments;

namespace {

constexpr double kPi = 3.14159265358979323846;

ex::FrequencySeries sdof(double f0, double q, double from, double to, int n, double gain = 1.0)
{
    ex::FrequencySeries s;
    s.quantity.quantity = ex::Quantity::Displacement;
    s.quantity.per = ex::Quantity::Force;
    for (int i = 0; i < n; ++i) {
        const double f = from + (to - from) * i / (n - 1);
        const double r = f / f0;
        s.frequencyHz.push_back(f);
        s.values.push_back(gain / std::complex<double>(1.0 - r * r, r / q));
    }
    return s;
}

}  // namespace

TEST(LaboratoryComparison, ScalarsAreJudgedByTheirUncertainty)
{
    ScalarValue m{10.0, 0.1, 2.0, "Hz", "measured"};
    auto inconsistent = compareScalars(m, {10.3, 0.1, 2.0, "Hz", "model"});
    ASSERT_TRUE(inconsistent);
    EXPECT_NEAR(*inconsistent.value().normalizedError, 0.3 / std::hypot(0.2, 0.2), 1e-12);
    EXPECT_EQ(inconsistent.value().verdict, "inconsistent");
    EXPECT_EQ(compareScalars(m, {10.2, 0.1, 2.0, "Hz", "model"}).value().verdict, "consistent");
    EXPECT_EQ(compareScalars({10.0, 0.0, 2.0, "Hz", ""}, {10.2, 0.1, 2.0, "Hz", ""}).value().verdict, "not assessable");
    auto converted = compareScalars({0.010, 1e-4, 2.0, "m", ""}, {10.1, 0.0, 2.0, "mm", ""});
    ASSERT_TRUE(converted);
    EXPECT_NEAR(converted.value().predicted, 0.0101, 1e-15);
    EXPECT_FALSE(converted.value().notes.empty());
}

TEST(LaboratoryComparison, FrequencyResponsesAreAlignedOnTheirOverlap)
{
    ex::FrequencySeries measured = sdof(100.0, 20.0, 50.0, 150.0, 401);
    measured.category = ex::DataCategory::Raw;
    for (const auto& v : measured.values)
        measured.standardUncertainty.push_back(0.02 * std::abs(v));
    ex::FrequencySeries predicted = sdof(102.0, 20.0, 40.0, 140.0, 233);  // another grid, a 2 % stiffer model
    predicted.category = ex::DataCategory::Simulated;
    auto c = compareResponses(measured, predicted);
    ASSERT_TRUE(c) << c.message();
    EXPECT_NEAR(c.value().peakOffsetHz, -2.0, 0.5);
    EXPECT_NEAR(c.value().peakOffsetRelative, -0.02, 0.005);
    EXPECT_LE(c.value().frequencyHz.back(), 140.0);  // the overlap only
    EXPECT_LT(c.value().spectralSimilarity, 1.0);
    EXPECT_GT(c.value().spectralSimilarity, 0.8);
    EXPECT_LT(c.value().fractionWithinCombined, 0.95);
    EXPECT_NE(c.value().verdict.find("inconsistent"), std::string::npos);
    auto same = compareResponses(measured, sdof(100.0, 20.0, 40.0, 160.0, 801));
    ASSERT_TRUE(same);
    EXPECT_NEAR(same.value().peakOffsetHz, 0.0, 0.3);
    EXPECT_GT(same.value().fractionWithinCombined, 0.95);
    EXPECT_LT(same.value().base.rmsPhaseErrorDeg, 1.0);
}

TEST(LaboratoryComparison, TransientsAndTheirShift)
{
    std::vector<double> t, mv, pt, pv;
    for (int i = 0; i <= 400; ++i) {
        t.push_back(i * 0.005);
        mv.push_back(std::sin(2 * kPi * 2.0 * (t.back() - 0.05)) * std::exp(-t.back()));
    }
    for (int i = 0; i <= 300; ++i) {
        pt.push_back(i * 0.007);
        pv.push_back(std::sin(2 * kPi * 2.0 * pt.back()) * std::exp(-pt.back()));
    }
    auto c = compareTransients(t, mv, {0.01}, pt, pv, 0.0, "m");
    ASSERT_TRUE(c) << c.message();
    EXPECT_LE(c.value().to, pt.back());
    EXPECT_NEAR(c.value().shiftSeconds, 0.05, 0.01);
    EXPECT_GT(c.value().rms, 0.0);
    EXPECT_FALSE(compareTransients({0.0, 1.0}, {1.0, 2.0}, {}, {5.0, 6.0}, {1.0, 2.0}, 0.0, "m"));
}

TEST(LaboratoryComparison, FieldsAfterAFrameTransform)
{
    FieldGrid g;
    g.origin = {0, 0, 0};
    g.spacing = {0.1, 0.1, 1.0};
    g.size = {11, 11, 1};
    g.frame = "scene";
    for (int j = 0; j < 11; ++j)
        for (int i = 0; i < 11; ++i)
            g.values.push_back(0.1 * i + 2 * 0.1 * j);  // x + 2 y: exact under trilinear interpolation
    // The measurement frame: the scene shifted by (0.2, 0.3) and turned 90 degrees about z.
    const std::array<double, 16> toScene{0, -1, 0, 0.2, 1, 0, 0, 0.3, 0, 0, 1, 0, 0, 0, 0, 1};
    std::vector<std::array<double, 3>> pos;
    std::vector<double> mv;
    for (double a : {0.0, 0.1, 0.2})
        for (double b : {-0.1, 0.0, 0.1}) {
            pos.push_back({a, b, 0.0});
            const double x = -b + 0.2, y = a + 0.3;
            mv.push_back(x + 2 * y);
        }
    pos.push_back({5.0, 5.0, 0.0});  // outside
    mv.push_back(0.0);
    auto c = compareFields(pos, mv, {0.01}, "rig", g, toScene);
    ASSERT_TRUE(c) << c.message();
    EXPECT_EQ(c.value().points, 9u);
    EXPECT_EQ(c.value().outside, 1u);
    EXPECT_LT(c.value().rms, 1e-12);
    EXPECT_DOUBLE_EQ(c.value().fractionWithin, 1.0);
    const std::array<double, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    auto refused = compareFields(pos, mv, {}, "rig", g, identity);
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.message().find("transform"), std::string::npos);
    EXPECT_FALSE(compareFields(pos, mv, {}, "", g, identity));
}

TEST(LaboratoryComparison, IncompatibleDataIsRefused)
{
    ex::FrequencySeries pressure = sdof(100, 10, 50, 150, 50);
    pressure.quantity = {ex::Quantity::Pressure, std::nullopt, false};
    EXPECT_FALSE(compareResponses(pressure, sdof(100, 10, 50, 150, 50)));
    EXPECT_FALSE(compareScalars({1.0, 0.1, 2.0, "Pa", ""}, {1.0, 0.1, 2.0, "m", ""}));
    EXPECT_FALSE(compareResponses(sdof(100, 10, 50, 60, 20), sdof(100, 10, 70, 80, 20)));
}
