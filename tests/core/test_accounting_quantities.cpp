// Quantity families (RES-ACC-003): a family is a physical quantity, not a dimension; torque
// and energy, entropy and heat capacity, pressure and momentum flux never combine; force is
// not conserved; values carry their units, context and uncertainty.
#include "accounting/Quantities.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;

TEST(AccountingQuantities, FamiliesWithTheSameDimensionStayApart)
{
    EXPECT_EQ(info(Family::Torque).dimension, info(Family::Energy).dimension);
    EXPECT_EQ(info(Family::Entropy).dimension, info(Family::HeatCapacity).dimension);
    EXPECT_EQ(info(Family::Pressure).dimension, info(Family::MomentumFlux).dimension);
    const Quantity e = make(Family::Energy, 2.0, "J").take();
    const Quantity t = make(Family::Torque, 2.0, "N m").take();
    auto sum = add(e, t);
    ASSERT_FALSE(sum);
    EXPECT_NE(sum.message().find("same dimension"), std::string::npos);
    EXPECT_FALSE(add(make(Family::Entropy, 1.0, "J/K").take(), make(Family::HeatCapacity, 1.0, "J/K").take()));
    EXPECT_FALSE(add(make(Family::Pressure, 1.0, "Pa").take(), make(Family::MomentumFlux, 1.0, "Pa").take()));
    EXPECT_FALSE(add(make(Family::Force, 1.0, "N").take(), make(Family::LinearMomentum, 1.0, "N s").take()));
    EXPECT_FALSE(add(e, make(Family::Power, 1.0, "W").take()));
    EXPECT_TRUE(info(Family::Energy).conserved);
    EXPECT_TRUE(info(Family::LinearMomentum).conserved);
    EXPECT_FALSE(info(Family::Force).conserved);
    EXPECT_FALSE(info(Family::Torque).conserved);
    EXPECT_FALSE(info(Family::Power).conserved);
}

TEST(AccountingQuantities, ValuesConvertAndCombineWithinAFamily)
{
    auto kwh = make(Family::Energy, 1.0, "kWh", "stored.thermal");
    ASSERT_TRUE(kwh);
    EXPECT_DOUBLE_EQ(kwh.value().value, 3.6e6);
    EXPECT_FALSE(make(Family::Energy, 1.0, "W"));
    Quantity a = kwh.take();
    a.uncertainty = 3.0;
    Quantity b = make(Family::Energy, 4.0, "MJ", "stored.thermal").take();
    b.uncertainty = 4.0;
    auto s = add(a, b);
    ASSERT_TRUE(s);
    EXPECT_DOUBLE_EQ(s.value().value, 7.6e6);
    EXPECT_EQ(s.value().kind, "stored.thermal");
    EXPECT_DOUBLE_EQ(*s.value().uncertainty, 5.0);
    EXPECT_NEAR(valueIn(s.value(), "kWh").value(), 7.6e6 / 3.6e6, 1e-12);
    // Vectors in different frames do not combine.
    Quantity f1, f2;
    f1.family = f2.family = Family::Force;
    f1.isVector = f2.isVector = true;
    f1.components = {1, 0, 0};
    f2.components = {0, 1, 0};
    f2.frame = "body";
    EXPECT_FALSE(add(f1, f2));
    f2.frame = "world";
    auto r = add(f1, f2);
    ASSERT_TRUE(r);
    EXPECT_NEAR(r.value().magnitude(), std::sqrt(2.0), 1e-15);
    // JSON round trip, with the context.
    Quantity q = make(Family::LinearMomentum, 3.0, "kg m/s", "impulse").take();
    q.region = "slab";
    q.interval = std::array<double, 2>{0.0, 2.0};
    auto back = Quantity::fromJson(q.toJson());
    ASSERT_TRUE(back) << back.message();
    EXPECT_EQ(back.value().toJson(), q.toJson());
    EXPECT_FALSE(Quantity::fromJson({{"family", "energy"}, {"value", 1.0}, {"unit", "W"}}));
}
