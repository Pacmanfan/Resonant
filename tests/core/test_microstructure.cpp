// Voxel microstructures (RES-MSC-005): seeded generators are deterministic and reach
// their targets; imported data is checked and says whether it is periodic.
#include "multiscale/Microstructure.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::multiscale;
using nlohmann::json;

TEST(Microstructure, SeededGeneratorsAreDeterministic)
{
    const json spec{{"generator", "spheres"}, {"voxels", {24, 24, 24}}, {"size", {1e-3, 1e-3, 1e-3}}, {"phases", {"matrix", "inclusion"}},
                    {"radius", 1.2e-4}, {"volumeFraction", 0.2}, {"seed", 7}};
    auto a = generateRve(spec), b = generateRve(spec);
    ASSERT_TRUE(a && b) << a.message();
    EXPECT_EQ(a.value().phase, b.value().phase);
    json other = spec;
    other["seed"] = 8;
    EXPECT_NE(generateRve(other).value().phase, a.value().phase);
    EXPECT_TRUE(a.value().periodic);
    // The voxelized fraction is near the target (whole spheres, voxelization).
    EXPECT_NEAR(a.value().volumeFractions()[1], 0.2, 0.04);
}

TEST(Microstructure, LayeredAndVoronoiStructures)
{
    auto layered = generateRve({{"generator", "layered"}, {"voxels", {4, 4, 30}}, {"size", {1, 1, 1}}, {"phases", {"a", "b"}},
                                {"axis", 2}, {"fractions", {1, 2}}, {"periods", 2}});
    ASSERT_TRUE(layered) << layered.message();
    EXPECT_NEAR(layered.value().volumeFractions()[0], 1.0 / 3.0, 1e-12);
    auto grains = generateRve({{"generator", "voronoi"}, {"voxels", {16, 16, 16}}, {"size", {1, 1, 1}}, {"phases", {"grain"}}, {"grains", 12},
                               {"texture", "aligned"}, {"axis", {0, 0, 1}}, {"spreadDeg", 5.0}, {"seed", 3}});
    ASSERT_TRUE(grains) << grains.message();
    EXPECT_EQ(grains.value().grainCount(), 12);
    for (const auto& a : grains.value().axes) {
        EXPECT_NEAR(a[0] * a[0] + a[1] * a[1] + a[2] * a[2], 1.0, 1e-12);
        EXPECT_GT(std::abs(a[2]), std::cos(25.0 * 3.14159265358979 / 180.0));  // within a few spreads of z
    }
    for (auto g : grains.value().grain)
        EXPECT_GE(g, 0);
}

TEST(Microstructure, ImportedVoxelsAreChecked)
{
    json spec{{"generator", "voxels"}, {"voxels", {2, 1, 1}}, {"size", {1, 1, 1}}, {"phases", {"a", "b"}}, {"data", {0, 1}}};
    EXPECT_FALSE(generateRve(spec));  // must say whether it is periodic
    spec["periodic"] = false;
    auto r = generateRve(spec);
    ASSERT_TRUE(r) << r.message();
    EXPECT_FALSE(r.value().periodic);
    spec["data"] = {0, 2};
    EXPECT_FALSE(generateRve(spec));
    EXPECT_FALSE(generateRve({{"generator", "fractal"}, {"voxels", {2, 2, 2}}, {"size", {1, 1, 1}}, {"phases", {"a"}}}));
    EXPECT_FALSE(generateRve({{"generator", "spheres"}, {"voxels", {8, 8, 8}}, {"size", {1, 1, 1}}, {"phases", {"a", "b"}}, {"radius", 0.1},
                              {"volumeFraction", 0.7}}));
}
