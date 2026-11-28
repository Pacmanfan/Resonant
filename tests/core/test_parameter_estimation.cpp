// Parameter estimation (RES-LAB-008, -009): known parameters recovered within their
// stated uncertainty from synthetic observations; identifiability examined - a product of
// two parameters is not two parameters; bounds, priors, joint fits, unweighted fits.
#include "laboratory/ParameterEstimation.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

using namespace resonant;
using namespace resonant::laboratory;

namespace {

constexpr double kPi = 3.14159265358979323846;

// |H(f)| of a mass-spring-damper (receptance).
double receptance(double m, double k, double c, double f)
{
    const double w = 2 * kPi * f;
    return 1.0 / std::hypot(k - m * w * w, c * w);
}

Observation synthetic(double m, double k, double c, double noise, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> n(0.0, 1.0);
    Observation o;
    o.dataset = "frf";
    for (int i = 0; i < 120; ++i) {
        const double f = 60.0 + 80.0 * i / 119;
        const double y = receptance(m, k, c, f);
        o.axis.push_back(f);
        o.values.push_back(y * (1.0 + noise * n(rng)));
        o.standard.push_back(noise * y);
    }
    return o;
}

}  // namespace

TEST(ParameterEstimation, RecoversKnownParametersWithinTheirUncertainty)
{
    const double m = 0.5, k = 2.0e5, c = 6.0;  // f0 = 100.7 Hz, Q = 52.7
    const std::vector<Observation> obs{synthetic(m, k, c, 0.01, 42)};
    const Predictor predict = [](const std::vector<double>& p, size_t i) {
        (void)i;
        std::vector<double> y;
        for (int n = 0; n < 120; ++n)
            y.push_back(receptance(p[0], p[1], p[2], 60.0 + 80.0 * n / 119));
        return Result<std::vector<double>>(y);
    };
    std::vector<EstimatedParameter> params{{"mass", "Mass", "kg", "", 0.4, 0.01, 10.0, {}, {}},
                                           {"stiffness", "Stiffness", "N/m", "", 2.4e5, 1e3, 1e8, {}, {}},
                                           {"damping", "Damping", "N s/m", "", 4.0, 0.0, 100.0, {}, {}}};
    auto r = estimateParameters(params, obs, predict);
    ASSERT_TRUE(r) << r.message();
    EXPECT_TRUE(r.value().converged);
    EXPECT_TRUE(r.value().weighted);
    const double truth[3] = {m, k, c};
    for (size_t i = 0; i < 3; ++i) {
        const auto& e = r.value().parameters[i];
        EXPECT_LT(std::abs(e.estimate - truth[i]), 4.0 * e.standardError) << e.id << " " << e.estimate;
        EXPECT_LE(e.lower, truth[i]);
        EXPECT_GE(e.upper, truth[i]);
        EXPECT_EQ(e.identifiability, "identified") << e.id;
    }
    EXPECT_NEAR(r.value().reducedChiSquare, 1.0, 0.35);
    EXPECT_TRUE(r.value().identified());
}

TEST(ParameterEstimation, IdentifiabilityIsAssessed)
{
    // y = a b x: only the product is determined.
    Observation o;
    o.dataset = "line";
    for (int i = 1; i <= 20; ++i) {
        o.axis.push_back(i);
        o.values.push_back(6.0 * i);
        o.standard.push_back(0.1);
    }
    const Predictor product = [&](const std::vector<double>& p, size_t) {
        std::vector<double> y;
        for (double x : o.axis)
            y.push_back(p[0] * p[1] * x);
        return Result<std::vector<double>>(y);
    };
    auto r = estimateParameters({{"a", "", "", "", 1.0, 0.0, 100.0, {}, {}}, {"b", "", "", "", 1.0, 0.0, 100.0, {}, {}}}, {o}, product);
    ASSERT_TRUE(r) << r.message();
    EXPECT_NEAR(r.value().parameters[0].estimate * r.value().parameters[1].estimate, 6.0, 1e-6);
    for (const auto& p : r.value().parameters)
        EXPECT_EQ(p.identifiability, "not identifiable") << p.id;
    EXPECT_FALSE(r.value().identified());
    bool named = false;
    for (const auto& n : r.value().notes)
        named = named || n.find("many parameter combinations") != std::string::npos;
    EXPECT_TRUE(named);
    EXPECT_GT(r.value().conditionNumber, 1e6);
    // y = a x + b: both determined.
    const Predictor line = [&](const std::vector<double>& p, size_t) {
        std::vector<double> y;
        for (double x : o.axis)
            y.push_back(p[0] * x + p[1]);
        return Result<std::vector<double>>(y);
    };
    auto good = estimateParameters({{"a", "", "", "", 1.0, -100.0, 100.0, {}, {}}, {"b", "", "", "", 1.0, -100.0, 100.0, {}, {}}}, {o}, line);
    ASSERT_TRUE(good);
    EXPECT_NEAR(good.value().parameters[0].estimate, 6.0, 1e-6);
    EXPECT_NEAR(good.value().parameters[1].estimate, 0.0, 1e-4);
    EXPECT_EQ(good.value().parameters[0].identifiability, "identified");
}

TEST(ParameterEstimation, BoundsPriorsJointAndUnweightedFits)
{
    Observation a, b;
    a.dataset = "a";
    b.dataset = "b";
    for (int i = 1; i <= 10; ++i) {
        a.axis.push_back(i);
        a.values.push_back(2.0 * i + 1.0);
        b.axis.push_back(i);
        b.values.push_back(2.0 * i - 3.0);
    }
    // Joint: a shared slope, an offset per dataset (unweighted, exact data).
    const Predictor joint = [&](const std::vector<double>& p, size_t k) {
        std::vector<double> y;
        for (double x : (k == 0 ? a : b).axis)
            y.push_back(p[0] * x + (k == 0 ? p[1] : p[2]));
        return Result<std::vector<double>>(y);
    };
    auto r = estimateParameters({{"slope", "", "", "", 1.0, -10, 10, {}, {}}, {"offsetA", "", "", "", 0.5, -10, 10, {}, {}},
                                 {"offsetB", "", "", "", 0.5, -10, 10, {}, {}}},
                                {a, b}, joint);
    ASSERT_TRUE(r);
    EXPECT_NEAR(r.value().parameters[0].estimate, 2.0, 1e-6);
    EXPECT_NEAR(r.value().parameters[2].estimate, -3.0, 1e-5);
    EXPECT_FALSE(r.value().weighted);
    bool scaled = false;
    for (const auto& n : r.value().notes)
        scaled = scaled || n.find("unweighted") != std::string::npos;
    EXPECT_TRUE(scaled);
    // A bound stops the slope: flagged.
    auto bounded = estimateParameters({{"slope", "", "", "", 1.0, 0.0, 1.5, {}, {}}, {"offsetA", "", "", "", 0.5, -10, 10, {}, {}},
                                       {"offsetB", "", "", "", 0.5, -10, 10, {}, {}}},
                                      {a, b}, joint);
    ASSERT_TRUE(bounded);
    EXPECT_TRUE(bounded.value().parameters[0].atBound);
    // A tight prior pulls the estimate toward its mean (maximum a posteriori).
    a.standard.assign(a.values.size(), 1.0);
    b.standard.assign(b.values.size(), 1.0);
    EstimatedParameter slope{"slope", "", "", "", 1.0, -10, 10, 1.0, 0.001};
    auto map = estimateParameters({slope, {"offsetA", "", "", "", 0.5, -10, 10, {}, {}}, {"offsetB", "", "", "", 0.5, -10, 10, {}, {}}}, {a, b}, joint);
    ASSERT_TRUE(map);
    EXPECT_NEAR(map.value().parameters[0].estimate, 1.0, 0.01);
    EXPECT_NE(map.value().method.find("maximum a posteriori"), std::string::npos);
    EXPECT_FALSE(estimateParameters({{"x", "", "", "", 5.0, 0.0, 1.0, {}, {}}}, {a}, joint));  // the initial value outside its bounds
}
