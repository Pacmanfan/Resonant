// Error budgets and propagation (RES-LAB-007): distributions, sensitivities, stated
// correlations with their justification, Welch-Satterthwaite and the Student t coverage;
// first-order and Monte Carlo propagation, correlated inputs, a nonlinear model's bias.
#include "laboratory/ErrorBudget.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::laboratory;

TEST(LaboratoryUncertainty, BudgetsCombineDistinctComponents)
{
    ErrorBudget b;
    b.quantity = "resonance frequency";
    b.unit = "Hz";
    b.value = 100.0;
    b.components = {
        {"spec", "instrument", "analyser accuracy (specification)", "Hz", "data sheet", "normal", 0.2, 2.0, 1.0, 0.0},
        {"res", "resolution", "frequency resolution 0.5 Hz (half-width 0.25)", "Hz", "", "rectangular", 0.25, 1.0, 1.0, 0.0},
        {"rep", "repeatability", "five repeated runs", "Hz", "standard error of the mean", "normal", 0.12, 1.0, 1.0, 4.0},
    };
    auto r = b.evaluate();
    ASSERT_TRUE(r) << r.message();
    const double u1 = 0.1, u2 = 0.25 / std::sqrt(3.0), u3 = 0.12;
    const double uc = std::sqrt(u1 * u1 + u2 * u2 + u3 * u3);
    EXPECT_NEAR(r.value().combined, uc, 1e-12);
    EXPECT_NEAR(r.value().effectiveDegreesOfFreedom, std::pow(uc, 4) / (std::pow(u3, 4) / 4.0), 1e-9);
    EXPECT_NEAR(r.value().coverageFactor, coverageFactor(r.value().effectiveDegreesOfFreedom), 1e-12);
    double sum = 0.0;
    for (const auto& l : r.value().lines)
        sum += l.contribution;
    EXPECT_NEAR(sum, 1.0, 1e-12);
    ASSERT_EQ(r.value().lines.size(), 3u);  // resolution, accuracy and repeatability stay separate
    EXPECT_EQ(r.value().lines[1].source, "resolution");
    EXPECT_NE(r.value().correlation.find("uncorrelated"), std::string::npos);
    EXPECT_NEAR(coverageFactor(4.0), 2.776, 1e-3);
    EXPECT_NEAR(coverageFactor(0.0), 1.960, 1e-3);
    EXPECT_NEAR(coverageFactor(10.0, 0.99), 3.169, 1e-3);
}

TEST(LaboratoryUncertainty, CorrelationsAreStatedAndJustified)
{
    ErrorBudget b;
    b.unit = "W";
    b.components = {{"a", "calibration", "", "W", "", "normal", 1.0, 1.0, 1.0, 0.0}, {"b", "calibration", "", "W", "", "normal", 1.0, 1.0, 1.0, 0.0}};
    EXPECT_NEAR(b.evaluate().value().combined, std::sqrt(2.0), 1e-12);
    b.correlations = {{"a", "b", 1.0, ""}};
    EXPECT_FALSE(b.evaluate());  // no justification
    b.correlations[0].justification = "both channels calibrated against the same reference";
    EXPECT_NEAR(b.evaluate().value().combined, 2.0, 1e-12);
    b.correlations[0].r = 1.5;
    EXPECT_FALSE(b.evaluate());
    b.correlations.clear();
    b.components[0].source = "guesswork";
    EXPECT_FALSE(b.evaluate());
}

TEST(LaboratoryUncertainty, LinearAndMonteCarloPropagation)
{
    const Model product = [](const std::vector<double>& x) { return Result<double>(x[0] * x[1]); };
    const std::vector<InputQuantity> in{{"a", 10.0, 0.1}, {"b", 5.0, 0.2}};
    auto lin = propagateLinear(product, in);
    auto mc = propagateMonteCarlo(product, in, {}, 40000, 7);
    ASSERT_TRUE(lin && mc);
    const double expected = std::hypot(5.0 * 0.1, 10.0 * 0.2);
    EXPECT_NEAR(lin.value().standard, expected, 1e-6);
    EXPECT_NEAR(mc.value().standard, expected, 0.02 * expected);
    EXPECT_NEAR(lin.value().sensitivities[0], 5.0, 1e-6);
    // Fully correlated equal inputs in a difference: no uncertainty left.
    const Model difference = [](const std::vector<double>& x) { return Result<double>(x[0] - x[1]); };
    Eigen::MatrixXd r(2, 2);
    r << 1.0, 0.999999, 0.999999, 1.0;
    const std::vector<InputQuantity> same{{"a", 1.0, 0.1}, {"b", 1.0, 0.1}};
    EXPECT_LT(propagateLinear(difference, same, r).value().standard, 1e-3);
    EXPECT_LT(propagateMonteCarlo(difference, same, r, 20000, 3).value().standard, 1e-3);
    // A nonlinear model: Monte Carlo sees the bias and the skew that the first order misses.
    const Model expo = [](const std::vector<double>& x) { return Result<double>(std::exp(x[0])); };
    const std::vector<InputQuantity> wide{{"x", 0.0, 0.5}};
    auto l2 = propagateLinear(expo, wide);
    auto m2 = propagateMonteCarlo(expo, wide, {}, 40000, 11);
    EXPECT_NEAR(l2.value().mean, 1.0, 1e-12);
    EXPECT_NEAR(m2.value().mean, std::exp(0.125), 0.02);
    EXPECT_GT(m2.value().upper - m2.value().mean, m2.value().mean - m2.value().lower);
    // Rectangular inputs.
    auto rect = propagateMonteCarlo([](const std::vector<double>& x) { return Result<double>(x[0]); }, {{"q", 0.0, 1.0, "rectangular"}}, {}, 40000, 5);
    EXPECT_NEAR(rect.value().standard, 1.0, 0.02);
    EXPECT_NEAR(rect.value().upper, 0.95 * std::sqrt(3.0), 0.03);
}
