// Field momentum and radiation force (RES-ACC-009, -012): a plane wave on layered bodies
// between vacuum half-spaces - the force from the vacuum Maxwell stress at the control
// surfaces, from the volume Lorentz force on the induced currents, and from (1 + R - T) I/c
// agree; a mirror gets 2 I/c; the body's momentum and energy balance; the absorbed power
// locally and globally; convergence with the integration points; refused configurations.
#include "accounting/Conservation.h"
#include "coupling/AccountingAdapters.h"
#include "electromagnetics/RadiationForce.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::accounting;

namespace {

constexpr double kC = 299792458.0;

em::LayeredSolution solve(std::vector<em::MediumLayer> layers, double f = 10e9)
{
    em::LayeredProblem p;
    p.frequencyHz = f;
    p.incidentIntensity = 1000.0;
    p.layers = std::move(layers);
    auto s = em::LayeredSolution::solve(p);
    EXPECT_TRUE(s) << s.message();
    return s.take();
}

em::MediumLayer layer(const char* name, double d, double eps, double tan, double sigma)
{
    em::MediumLayer l;
    l.name = name;
    l.thickness = d;
    l.permittivityReal = eps;
    l.lossTangent = tan;
    l.conductivity = sigma;
    return l;
}

Check check(const Report& r, const std::string& model)
{
    for (const Check& c : r.checks)
        if (c.model == model)
            return c;
    ADD_FAILURE() << "no check " << model;
    return {};
}

}  // namespace

TEST(AccountingRadiation, ThreeMethodsAgreeOnALossySlab)
{
    const auto s = solve({layer("lossy", 0.02, 4.0, 0.3, 0.0), layer("glass", 0.01, 6.0, 0.0, 0.0)});
    auto f = em::radiationForce(s, 4001);
    ASSERT_TRUE(f) << f.message();
    const double ref = (1 + f.value().reflectance - f.value().transmittance) * 1000.0 / kC;
    EXPECT_NEAR(f.value().pressureFluxes, ref, 1e-15);
    EXPECT_NEAR(f.value().pressureSurfaces, ref, 1e-9 * ref);  // exact in vacuum
    EXPECT_NEAR(f.value().pressureVolume, ref, 1e-7 * ref);
    EXPECT_NEAR(f.value().absorbedLocal, f.value().absorbedGlobal, 1e-7 * f.value().absorbedGlobal);
    auto l = coupling::radiationLedger(s, 0.5, 4001, 1e-6);
    ASSERT_TRUE(l) << l.message();
    const Report r = evaluate(l.value(), wholeSystem(l.value()), {1e-6, 1e-3, 1e-15});
    EXPECT_EQ(check(r, "radiation.force-methods").status, CheckStatus::Verified);
    const Check local = check(r, "absorption.local-global");
    EXPECT_EQ(local.status, CheckStatus::Verified);
    EXPECT_EQ(local.scope, "local");
    const Check p = check(r, "momentum.linear");
    EXPECT_EQ(p.status, CheckStatus::Verified) << p.attribution;
    EXPECT_EQ(p.unit, "kg m/s^2");
    EXPECT_NEAR(p.delta[2], ref * 0.5, 1e-6 * ref);  // the force on 0.5 m^2
    EXPECT_EQ(check(r, "energy.total").status, CheckStatus::Verified);
    ASSERT_EQ(l.value().forces.size(), 1u);
    EXPECT_EQ(l.value().forces[0].category, "radiation-pressure");
}

TEST(AccountingRadiation, AbsorberAndMirrorLimits)
{
    // A good conductor reflects nearly everything: twice the momentum flux.
    const auto mirror = solve({layer("copper", 2e-5, 1.0, 0.0, 5.8e7)});
    auto m = em::radiationForce(mirror, 4001);
    ASSERT_TRUE(m) << m.message();
    EXPECT_GT(m.value().reflectance, 0.999);
    EXPECT_NEAR(m.value().pressureSurfaces, 2.0 * 1000.0 / kC, 2e-3 * 2000.0 / kC);
    EXPECT_NEAR(m.value().pressureVolume, m.value().pressureFluxes, 1e-6 * m.value().pressureFluxes);
    // A thick lossy slab with a matching layer absorbs almost everything: close to I/c.
    const auto absorber = solve({layer("graded", 0.01, 1.5, 0.05, 0.0), layer("lossy", 0.2, 2.5, 0.4, 0.0)});
    auto a = em::radiationForce(absorber, 8001);
    ASSERT_TRUE(a);
    EXPECT_LT(a.value().transmittance, 1e-6);
    EXPECT_NEAR(a.value().pressureVolume, (1 + a.value().reflectance) * 1000.0 / kC, 1e-6 * 1000.0 / kC);
    EXPECT_LT(a.value().reflectance, 0.1);
}

TEST(AccountingRadiation, LocalAndGlobalAbsorptionAgreeAndConvergeWithThePoints)
{
    const auto s = solve({layer("lossy", 0.03, 5.0, 0.2, 0.01)});
    double previous = 0.0, ratio = 0.0;
    for (int points : {41, 81}) {
        auto f = em::radiationForce(s, points);
        ASSERT_TRUE(f);
        const double err = std::abs(f.value().pressureVolume - f.value().pressureFluxes);
        if (previous > 0.0)
            ratio = previous / err;
        previous = err;
    }
    EXPECT_GT(ratio, 10.0);  // Simpson: fourth order
}

TEST(AccountingRadiation, ConfigurationsWithoutAUniqueForceAreRefused)
{
    em::LayeredProblem p;
    p.frequencyHz = 1e9;
    p.incidentIntensity = 1.0;
    p.incidence.permittivityReal = 2.0;  // a dielectric half-space: the formulation inside matter matters
    p.layers = {layer("slab", 0.01, 3.0, 0.1, 0.0)};
    auto s = em::LayeredSolution::solve(p);
    ASSERT_TRUE(s);
    auto f = em::radiationForce(s.value());
    ASSERT_FALSE(f);
    EXPECT_NE(f.message().find("formulation"), std::string::npos);
    em::MediumLayer magnetic = layer("ferrite", 0.01, 3.0, 0.0, 0.0);
    magnetic.permeability = 2.0;
    p.incidence.permittivityReal = 1.0;
    p.layers = {magnetic};
    auto s2 = em::LayeredSolution::solve(p);
    ASSERT_TRUE(s2);
    EXPECT_FALSE(em::radiationForce(s2.value()));
}
