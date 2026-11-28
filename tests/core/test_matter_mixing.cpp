// Effective properties of compositions (RES-FAM-008): every estimate with its
// bounds and validity, against closed forms; no estimator where there is none.
#include "matter/Mixing.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::matter;

TEST(MatterMixing, TransportBoundsAreNestedAndMaxwellGarnettIsTheMatrixSideBound)
{
    // Alumina spheres (30 W/(m K)) at 20 % in epoxy (0.2 W/(m K)).
    const std::vector<PhaseProperty> phases{{"epoxy", 0.8, 0.2}, {"alumina", 0.2, 30.0}};
    auto hs = combine("thermal.thermalConductivity", phases, "hashin-shtrikman-lower");
    ASSERT_TRUE(hs.ok()) << hs.message();
    const EffectiveEstimate& e = hs.value();
    EXPECT_LE(e.absoluteLower, e.lower);
    EXPECT_LT(e.lower, e.upper);
    EXPECT_LE(e.upper, e.absoluteUpper);
    EXPECT_NEAR(e.absoluteLower, 1.0 / (0.8 / 0.2 + 0.2 / 30.0), 1e-12);
    EXPECT_NEAR(e.absoluteUpper, 0.8 * 0.2 + 0.2 * 30.0, 1e-12);
    // The two-phase Hashin-Shtrikman lower bound in closed form.
    const double k1 = 0.2, k2 = 30.0, f2 = 0.2, f1 = 0.8;
    EXPECT_NEAR(e.lower, k1 + f2 / (1.0 / (k2 - k1) + f1 / (3.0 * k1)), 1e-12);
    auto mg = combine("thermal.thermalConductivity", phases, "maxwell-garnett", 0);
    ASSERT_TRUE(mg.ok());
    EXPECT_NEAR(mg.value().value, e.lower, 1e-12);  // dilute spheres in the less conducting matrix
    EXPECT_TRUE(mg.value().valid);
    auto br = combine("thermal.thermalConductivity", phases, "bruggeman");
    ASSERT_TRUE(br.ok());
    EXPECT_GT(br.value().value, e.lower);
    EXPECT_LT(br.value().value, e.upper);
    // Maxwell Garnett beyond the dilute range is computed but not valid.
    auto dense = combine("thermal.thermalConductivity", {{"epoxy", 0.5, 0.2}, {"alumina", 0.5, 30.0}}, "maxwell-garnett", 0);
    ASSERT_TRUE(dense.ok());
    EXPECT_FALSE(dense.value().valid);
    EXPECT_FALSE(combine("thermal.thermalConductivity", phases, "maxwell-garnett").ok()) << "Maxwell Garnett needs the matrix";
}

TEST(MatterMixing, BruggemanPercolatesAtOneThird)
{
    // Conducting grains in an insulator: sigma = sigma1 (3 f - 1) / 2 above f = 1/3, zero below.
    auto above = combine("electromagnetic.conductivity", {{"metal", 0.6, 1e6}, {"void", 0.4, 0.0}}, "bruggeman");
    ASSERT_TRUE(above.ok());
    EXPECT_NEAR(above.value().value, 1e6 * (3.0 * 0.6 - 1.0) / 2.0, 1.0);
    auto below = combine("electromagnetic.conductivity", {{"metal", 0.25, 1e6}, {"void", 0.75, 0.0}}, "bruggeman");
    ASSERT_TRUE(below.ok());
    EXPECT_LT(below.value().value, 1e-3);
    EXPECT_DOUBLE_EQ(below.value().lower, 0.0);  // an insulating coating is possible
}

TEST(MatterMixing, ElasticBoundsThroughBulkAndShearModuli)
{
    // Steel and epoxy, half and half.
    const std::vector<PhaseProperty> phases{{"steel", 0.5, 200e9, 0.29}, {"epoxy", 0.5, 3e9, 0.35}};
    auto lower = combine("mechanical.youngsModulus", phases, "hashin-shtrikman-lower");
    auto upper = combine("mechanical.youngsModulus", phases, "hashin-shtrikman-upper");
    auto voigt = combine("mechanical.youngsModulus", phases, "voigt");
    auto reuss = combine("mechanical.youngsModulus", phases, "reuss");
    ASSERT_TRUE(lower.ok() && upper.ok() && voigt.ok() && reuss.ok());
    EXPECT_LT(reuss.value().value, lower.value().value);
    EXPECT_LT(lower.value().value, upper.value().value);
    EXPECT_LT(upper.value().value, voigt.value().value);
    // The two-phase Hashin-Shtrikman upper bound on the bulk modulus (phase 1 the stiffer).
    const ElasticModuli s = elasticFrom(200e9, 0.29), p = elasticFrom(3e9, 0.35);
    const double kUpper = s.bulk + 0.5 / (1.0 / (p.bulk - s.bulk) + 3.0 * 0.5 / (3.0 * s.bulk + 4.0 * s.shear));
    EXPECT_NEAR(walpoleBounds(phases).second.bulk / kUpper, 1.0, 1e-12);
    EXPECT_GT(upper.value().poisson, 0.0);
    EXPECT_LT(upper.value().poisson, 0.5);
    // A single phase is recovered by every method.
    auto one = combine("mechanical.youngsModulus", {{"steel", 1.0, 200e9, 0.29}}, "hashin-shtrikman-lower");
    ASSERT_TRUE(one.ok());
    EXPECT_NEAR(one.value().value / 200e9, 1.0, 1e-12);
    EXPECT_NEAR(one.value().poisson, 0.29, 1e-12);
}

TEST(MatterMixing, CellularSolidsDensityAndSpecificHeat)
{
    // An open-cell aluminium foam at 10 % relative density: E* = 0.01 E_s (Gibson-Ashby).
    auto foam = combine("mechanical.youngsModulus", {{"aluminium", 0.1, 70e9, 0.33}, {"pores", 0.9, 0.0, 0.0}}, "gibson-ashby-open-cell", 0);
    ASSERT_TRUE(foam.ok()) << foam.message();
    EXPECT_NEAR(foam.value().value, 0.7e9, 1.0);
    EXPECT_TRUE(foam.value().valid);
    EXPECT_DOUBLE_EQ(foam.value().lower, 0.0);  // pores make the lower bound vanish
    auto dense = combine("mechanical.youngsModulus", {{"aluminium", 0.5, 70e9, 0.33}, {"pores", 0.5, 0.0, 0.0}}, "gibson-ashby-open-cell", 0);
    ASSERT_TRUE(dense.ok());
    EXPECT_FALSE(dense.value().valid);
    auto rho = combine("mechanical.density", {{"a", 0.3, 2700.0}, {"b", 0.7, 1200.0}}, "volume-average");
    ASSERT_TRUE(rho.ok());
    EXPECT_TRUE(rho.value().exact);
    EXPECT_DOUBLE_EQ(rho.value().value, 0.3 * 2700.0 + 0.7 * 1200.0);
    PhaseProperty a{"a", 0.3, 900.0}, b{"b", 0.7, 1100.0};
    a.density = 2700.0;
    b.density = 1200.0;
    auto cp = combine("thermal.specificHeat", {a, b}, "mass-average");
    ASSERT_TRUE(cp.ok());
    EXPECT_NEAR(cp.value().value, (0.3 * 2700.0 * 900.0 + 0.7 * 1200.0 * 1100.0) / (0.3 * 2700.0 + 0.7 * 1200.0), 1e-9);
}

TEST(MatterMixing, NoEstimatorWhereThePropertyDependsOnTheMicrostructure)
{
    auto loss = combine("mechanical.lossFactor", {{"a", 0.5, 0.01}, {"b", 0.5, 0.1}}, "voigt");
    ASSERT_FALSE(loss.ok());
    EXPECT_NE(loss.message().find("measured or derived"), std::string::npos) << loss.message();
    EXPECT_FALSE(combine("thermal.thermalConductivity", {{"a", 0.5, 1.0}, {"b", 0.4, 2.0}}, "series").ok()) << "fractions that do not sum to 1";
    EXPECT_FALSE(combine("thermal.thermalConductivity", {{"a", 0.5, 1.0}, {"b", 0.5, 2.0}}, "voigt").ok()) << "voigt is an elastic method";
    EXPECT_EQ(propertyKindOf("electromagnetic.relativePermittivity"), PropertyKind::Transport);
}
