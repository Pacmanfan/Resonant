// The effective conductivity of a layered medium (RES-META-009): series and parallel
// conductances against the finite-volume solver on the explicit layers.
#include "thermal/LayeredConductivity.h"

#include <gtest/gtest.h>

using namespace resonant;

TEST(LayeredConductivity, SeriesAndParallelFormulas)
{
    // 2 mm of k = 200 and 1 mm of k = 0.2.
    auto e = thermal::layeredConductivity({{0.002, 200.0}, {0.001, 0.2}});
    ASSERT_TRUE(e) << e.message();
    EXPECT_NEAR(e.value().parallel, (0.002 * 200.0 + 0.001 * 0.2) / 0.003, 1e-12);
    EXPECT_NEAR(e.value().perpendicular, 0.003 / (0.002 / 200.0 + 0.001 / 0.2), 1e-12);
    EXPECT_GT(e.value().anisotropy, 200.0);  // 133.4 / 0.599
    EXPECT_FALSE(thermal::layeredConductivity({}));
    EXPECT_FALSE(thermal::layeredConductivity({{0.001, 0.0}}));
}

TEST(LayeredConductivity, FiniteVolumesReproduceTheTensor)
{
    const std::vector<thermal::ConductiveLayer> cell{{0.002, 200.0}, {0.001, 0.2}};
    auto e = thermal::layeredConductivity(cell);
    auto x = thermal::explicitLayeredConductivity(cell, 6, 3);
    ASSERT_TRUE(e && x) << x.message();
    EXPECT_NEAR(x.value().perpendicular, e.value().perpendicular, 1e-8 * e.value().perpendicular);
    EXPECT_NEAR(x.value().parallel, e.value().parallel, 1e-8 * e.value().parallel);
    EXPECT_LT(x.value().balanceResidual, 1e-8);
}

TEST(LayeredConductivity, EqualLayersAreIsotropic)
{
    auto e = thermal::layeredConductivity({{0.001, 3.0}, {0.004, 3.0}});
    ASSERT_TRUE(e);
    EXPECT_NEAR(e.value().anisotropy, 1.0, 1e-12);
    EXPECT_NEAR(e.value().perpendicular, 3.0, 1e-12);
}
