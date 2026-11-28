// Dimensional analysis (RES-ACC-002): unit expressions to SI dimensions and factors,
// conversions that check the dimension, affine temperature scales refused, and every
// registered coupled-engine quantity's unit understood.
#include "foundation/Dimensions.h"
#include "multiphysics/Quantity.h"

#include <gtest/gtest.h>

using namespace resonant;

TEST(Dimensions, ExpressionsBecomeSiDimensions)
{
    const auto dim = [](const char* u) {
        auto r = parseUnit(u);
        EXPECT_TRUE(r) << u << ": " << r.message();
        return r ? r.value().dimension.toString() : std::string("?");
    };
    EXPECT_EQ(dim("W/m^3"), "kg m^-1 s^-3");
    EXPECT_EQ(dim("J/(kg K)"), "m^2 s^-2 K^-1");
    EXPECT_EQ(dim("kg m/s"), dim("N s"));
    EXPECT_EQ(dim("N*m"), dim("J"));  // the same dimension: torque and energy (the families keep them apart)
    EXPECT_EQ(dim("W/m^2/K"), "kg s^-3 K^-1");
    EXPECT_EQ(dim("1"), "1");
    EXPECT_EQ(dim(""), "1");
    EXPECT_EQ(dim("rad/s"), dim("Hz"));
    EXPECT_EQ(dim("V/m"), dim("N/C"));
    EXPECT_EQ(dim("ohm"), dim("V/A"));
    EXPECT_EQ(dim("T"), dim("Wb/m^2"));
    EXPECT_EQ(dim("kg m^2/s"), dim("J s"));
}

TEST(Dimensions, ConversionsCheckTheDimension)
{
    const auto conv = [](double v, const char* a, const char* b) {
        auto r = convertUnit(v, a, b);
        EXPECT_TRUE(r) << a << " -> " << b << ": " << r.message();
        return r ? r.value() : 0.0;
    };
    EXPECT_DOUBLE_EQ(conv(1.0, "kWh", "J"), 3.6e6);
    EXPECT_DOUBLE_EQ(conv(1.0, "eV", "J"), 1.602176634e-19);
    EXPECT_NEAR(conv(2.0, "mm", "m"), 2e-3, 1e-18);
    EXPECT_NEAR(conv(1.0, "\xC2\xB5s", "s"), 1e-6, 1e-21);  // the micro sign
    EXPECT_NEAR(conv(1.0, "bar", "kPa"), 100.0, 1e-12);
    EXPECT_NEAR(conv(1.0, "g/cm^3", "kg/m^3"), 1000.0, 1e-9);
    EXPECT_NEAR(conv(3.0, "kN m", "J"), 3000.0, 1e-9);
    EXPECT_NEAR(conv(1.0, "min", "s"), 60.0, 1e-12);
    EXPECT_NEAR(conv(180.0, "deg", "rad"), 3.14159265358979323846, 1e-12);
    auto bad = convertUnit(1.0, "J", "W");
    ASSERT_FALSE(bad);
    EXPECT_NE(bad.message().find("dimensions differ"), std::string::npos);
    auto affine = parseUnit("degC");
    ASSERT_FALSE(affine);
    EXPECT_NE(affine.message().find("affine"), std::string::npos);
    EXPECT_FALSE(parseUnit("furlong"));
    EXPECT_FALSE(parseUnit("m^"));
    EXPECT_FALSE(parseUnit("(m"));
}

TEST(Dimensions, EveryRegisteredQuantityHasAKnownUnit)
{
    for (const auto& q : multiphysics::quantityRegistry()) {
        auto u = parseUnit(q.unit);
        EXPECT_TRUE(u) << q.id << " '" << q.unit << "': " << u.message();
    }
    EXPECT_TRUE(hasDimension("W/m^3", parseUnit("J/(s m^3)").value().dimension));
}
