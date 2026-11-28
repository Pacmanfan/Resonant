// Spatial property fields (RES-FAM-007): evaluation, series and parallel
// combinations against their integrals, discretization and its error,
// validation, and the idealized / realizable distinction.
#include "matter/PropertyField.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::matter;

namespace {

PropertyField field(const std::string& kind, double from, double to, double exponent = 1.0)
{
    PropertyField f;
    f.id = "f";
    f.region = "r";
    f.property = "thermal.thermalConductivity";
    f.kind = kind;
    f.from = from;
    f.to = to;
    f.exponent = exponent;
    return f;
}

}  // namespace

TEST(MatterFields, LinearGradingCombinesInSeriesByTheLogarithmicMean)
{
    // A metal-to-ceramic conductivity gradient: k(s) = k1 + (k2 - k1) s. Through the
    // thickness the resistance is the integral of ds / k: h ln(k2/k1) / (k2 - k1).
    const PropertyField f = field("linear", 90.0, 2.0);
    EXPECT_NEAR(f.harmonicMean(), (2.0 - 90.0) / std::log(2.0 / 90.0), 1e-12);
    EXPECT_NEAR(f.mean(), 46.0, 1e-12);
    EXPECT_LT(f.harmonicMean(), f.mean());  // series below parallel
    EXPECT_DOUBLE_EQ(f.at(0.25), 90.0 - 88.0 * 0.25);
    EXPECT_DOUBLE_EQ(f.minimum(), 2.0);
    EXPECT_DOUBLE_EQ(f.maximum(), 90.0);
}

TEST(MatterFields, PowerLawExponentialAndTablesAgainstTheirIntegrals)
{
    const PropertyField p = field("power-law", 70e9, 380e9, 2.0);
    EXPECT_NEAR(p.mean(), 70e9 + (380e9 - 70e9) / 3.0, 1e-3);
    // Exponential: v = a r^s, the integral of 1/v is (1 - 1/r) / (a ln r).
    const PropertyField e = field("exponential", 10.0, 1000.0);
    const double r = 100.0;
    EXPECT_NEAR(e.harmonicMean(), 10.0 * std::log(r) / (1.0 - 1.0 / r), 1e-9);
    EXPECT_NEAR(e.mean(), 10.0 * (r - 1.0) / std::log(r), 1e-9);
    PropertyField t = field("table", 0.0, 0.0);
    t.table = {{0.0, 4.0}, {0.5, 4.0}, {0.5 + 1e-9, 1.0}, {1.0, 1.0}};
    ASSERT_TRUE(t.validate().ok()) << t.validate().message();
    EXPECT_NEAR(t.mean(), 2.5, 1e-8);                      // two halves
    EXPECT_NEAR(t.harmonicMean(), 1.0 / (0.5 / 4.0 + 0.5 / 1.0), 1e-8);
}

TEST(MatterFields, DiscretizationConvergesAtSecondOrder)
{
    const PropertyField f = field("linear", 90.0, 2.0);
    const auto layers = f.discretize(10);
    ASSERT_EQ(layers.size(), 10u);
    EXPECT_DOUBLE_EQ(layers.front(), f.at(0.05));
    // Midpoint sublayers converge at second order (a mild gradient is in the asymptotic range).
    const PropertyField mild = field("linear", 10.0, 5.0);
    EXPECT_NEAR(mild.discretizationError(10) / mild.discretizationError(20), 4.0, 0.1);
    // On a steep gradient the estimate tracks the true error of the series combination.
    for (int n : {10, 40}) {
        double sum = 0.0;
        for (double v : f.discretize(n))
            sum += 1.0 / (n * v);
        const double trueError = std::abs(1.0 / sum - f.harmonicMean()) / f.harmonicMean();
        const double estimate = f.discretizationError(n);
        EXPECT_GT(estimate, 0.5 * trueError) << n;
        EXPECT_LT(estimate, 1.5 * trueError) << n;
    }
    EXPECT_GT(f.discretizationError(10), f.discretizationError(40));
}

TEST(MatterFields, ValidationAndRoundTrip)
{
    PropertyField f = field("power-law", 1.0, 2.0, -1.0);
    EXPECT_FALSE(f.validate().ok());
    f.exponent = 0.5;
    f.realizable = true;
    EXPECT_FALSE(f.validate().ok()) << "realizable needs the process that makes it";
    f.process = "proc-ded";
    EXPECT_TRUE(f.validate().ok());
    auto back = PropertyField::fromJson(f.toJson());
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(back.value().toJson(), f.toJson());
    PropertyField bad = field("table", 0.0, 0.0);
    bad.table = {{0.0, 1.0}, {0.0, 2.0}};
    EXPECT_FALSE(bad.validate().ok());
    EXPECT_FALSE(PropertyField::fromJson({{"id", "x"}, {"region", "r"}, {"property", "p"}, {"kind", "spline"}}).ok());
    EXPECT_FALSE(field("exponential", 1.0, -1.0).validate().ok());
}
