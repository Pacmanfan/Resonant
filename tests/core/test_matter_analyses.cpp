// Analyses of architectures (RES-FAM-010, -011, -020): each against its closed
// form or the solver it wraps, a change of architecture changing the prediction,
// orientation rotating the properties, and what an analysis cannot represent
// refused with the reason.
#include "MatterFixture.h"

#include "coupling/MatterAnalyses.h"
#include "matter/PropertyResolution.h"
#include "mechanics/Laminate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace matter_test;
using namespace resonant::matter;
using resonant::coupling::analyzeStack;

namespace {

json run(const Architecture& a, const std::string& stack, const std::string& analysis, const json& options, const Project& p)
{
    auto r = analyzeStack(a, stack, analysis, options, p.materials);
    EXPECT_TRUE(r.ok()) << analysis << ": " << r.message();
    return r ? r.value() : json{{"metrics", json::object()}, {"result", json::object()}, {"warnings", json::array()}};
}

SourcedValue sv(double v)
{
    SourcedValue s;
    s.value = v;
    return s;
}

double metric(const json& r, const char* key)
{
    return r["metrics"].value(key, std::nan(""));
}

}  // namespace

TEST(MatterAnalyses, TheThermalBarrierInSeriesWithItsContactResistance)
{
    auto p = example("c-thermal-barrier.resonant");
    const Architecture a = architecture(*p, "arch-tbc");
    const json r = run(a, "st-wall", "thermal", {{"temperatures", json::array({1400.0, 900.0})}}, *p);
    const double expected = 3e-3 / 11.4 + 1e-4 / 4.3 + 3e-4 / 1.0 + 2e-5;
    EXPECT_NEAR(metric(r, "resistance") / expected, 1.0, 1e-12);
    EXPECT_NEAR(metric(r, "heatFlux"), 500.0 / expected, 1e-9);
    EXPECT_NEAR(metric(r, "interfaceShare"), 2e-5 / expected, 1e-12);
    // The profile has a jump of q R at the bond coat - top coat interface.
    const json& profile = r["result"]["profile"];
    ASSERT_EQ(profile.size(), 5u);
    EXPECT_NEAR(profile[2][1].get<double>() - profile[3][1].get<double>(), metric(r, "heatFlux") * 2e-5, 1e-9);
    EXPECT_NEAR(profile.back()[1].get<double>(), 900.0, 1e-9);
    // In the plane, the layers act in parallel (and the contact resistance does not enter).
    EXPECT_NEAR(metric(r, "inPlaneConductivity"), (3e-3 * 11.4 + 1e-4 * 4.3 + 3e-4 * 1.0) / 3.4e-3, 1e-12);
}

TEST(MatterAnalyses, AGradedLayerIntegratedInSeriesDiffersFromTwoDiscreteLayers)
{
    auto p = example("d-graded-metal-ceramic.resonant");
    const Architecture fgm = architecture(*p, "arch-fgm");
    const json graded = run(fgm, "st-fgm", "thermal", json::object(), *p);
    // The series integral of 1/k(s) through the graded layer, independently by Simpson's rule.
    const FunctionalRegion& r = *fgm.region("graded");
    const int n = 2000;
    double integral = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double s = static_cast<double>(i) / n;
        const double w = (i == 0 || i == n) ? 1.0 : (i % 2 ? 4.0 : 2.0);
        integral += w / resolve(fgm, r, "thermal.thermalConductivity", s, p->materials).value;
    }
    integral *= 2e-3 / (3.0 * n);
    EXPECT_NEAR(metric(graded, "resistance") / integral, 1.0, 1e-4);
    EXPECT_LT(graded["result"]["discretizationError"].get<double>(), 1e-4);
    const json step = run(architecture(*p, "arch-step"), "st-fgm", "thermal", json::object(), *p);
    EXPECT_NEAR(metric(step, "resistance"), 2e-3 * 2 / 3 / 90.9 + 2e-3 / 3 / 30.0, 1e-15);
    EXPECT_GT(std::abs(metric(graded, "resistance") / metric(step, "resistance") - 1.0), 0.05);
    // Graded stiffness through lamination theory: between the two constituents' plates.
    const json lam = run(fgm, "st-fgm", "laminate", {{"sublayers", 80}}, *p);
    EXPECT_GT(metric(lam, "Ex"), 200e9);
    EXPECT_LT(metric(lam, "Ex"), 370e9);
}

TEST(MatterAnalyses, TheCoatedCantileverIsNotTheBareOne)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture a = architecture(*p, "arch-bonded");
    const json m = run(a, "st-beam", "modes", json::object(), *p);
    // The bare substrate: a steel strip 40 x 10 x 0.5 mm.
    const double ei = 193e9 * 0.01 * std::pow(5e-4, 3) / 12.0, mpl = 7990 * 0.01 * 5e-4;
    EXPECT_NEAR(metric(m, "f1Bare"), resonant::mechanics::cantileverFrequencies(ei, mpl, 0.04, 1)[0], 1e-9);
    // The coated beam by the parallel-axis theorem over every layer (narrow beam).
    std::vector<resonant::mechanics::Ply> plies{
        resonant::mechanics::Ply::isotropic("steel", 5e-4, 193e9, 0.27, 7990), resonant::mechanics::Ply::isotropic("epoxy", 2e-5, 3e9, 0.35, 1200),
        resonant::mechanics::Ply::isotropic("gold", 5e-6, 79e9, 0.42, 19300),   resonant::mechanics::Ply::isotropic("pzt", 2e-4, 61e9, 0.35, 7750),
        resonant::mechanics::Ply::isotropic("gold", 1e-6, 79e9, 0.42, 19300),   resonant::mechanics::Ply::isotropic("parylene", 1e-5, 2.8e9, 0.40, 1289)};
    const auto section = resonant::mechanics::beamSection(plies);
    const double coated = resonant::mechanics::cantileverFrequencies(section.bendingStiffness * 0.01, section.massPerArea * 0.01, 0.04, 1)[0];
    EXPECT_NEAR(metric(m, "f1") / coated, 1.0, 1e-9);
    EXPECT_GT(std::abs(metric(m, "f1Shift")), 0.05) << "the bonded plate changes the first mode by more than 5 %";
    EXPECT_NEAR(metric(m, "massAdded"), section.massPerArea - 7990 * 5e-4, 1e-9);
    // The thin film changes it far less.
    const json film = run(architecture(*p, "arch-film"), "st-beam", "modes", json::object(), *p);
    EXPECT_LT(std::abs(metric(film, "f1Shift")), std::abs(metric(m, "f1Shift")));
    // A plate of the homogeneous example against its closed form.
    auto plate = example("a-homogeneous-plate.resonant");
    const json pm = run(architecture(*plate, "arch-plate"), "st-plate", "modes", {{"boundary", "simply-supported-plate"}}, *plate);
    const double D = 69e9 * std::pow(2e-3, 3) / (12.0 * (1.0 - 0.33 * 0.33));
    const double f11 = std::numbers::pi / 2.0 * (1.0 / (0.2 * 0.2) + 1.0 / (0.15 * 0.15)) * std::sqrt(D / (2700 * 2e-3));
    EXPECT_NEAR(metric(pm, "f1") / f11, 1.0, 1e-9);
    EXPECT_NEAR(metric(pm, "f1Shift"), 0.0, 1e-12);
}

TEST(MatterAnalyses, TheEnergyConverterHarvestsWithABalancedLedger)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture a = architecture(*p, "arch-bonded");
    const json h = run(a, "st-beam", "harvester", {{"tipMass", 0.002}, {"sweep", {{"points", 21}}}}, *p);
    EXPECT_GT(metric(h, "power"), 0.0);
    EXPECT_LT(metric(h, "efficiency"), 1.0);
    EXPECT_GT(metric(h, "openCircuitHz"), metric(h, "shortCircuitHz"));
    EXPECT_EQ(h["result"]["balance"], "verified");
    EXPECT_NEAR(metric(h, "inputPower"), metric(h, "power") + metric(h, "dissipatedPower"), 1e-12 * metric(h, "inputPower"));
    EXPECT_EQ(h["result"]["sweep"].size(), 21u);
    // The patterned top electrode covers 80 % from the root: theta scales by phi'(0.8 L) / phi'(L).
    Architecture full = a;
    full.stack("st-beam")->layers[4].pattern = Pattern{};
    const json hf = run(full, "st-beam", "harvester", {{"tipMass", 0.002}}, *p);
    EXPECT_NEAR(h["result"]["model"]["theta"].get<double>() / hf["result"]["model"]["theta"].get<double>(), (6.0 * 0.8 - 3.0 * 0.64) / 3.0, 1e-12);
    // The damping comes from the layers' loss factors unless stated.
    EXPECT_NE(h["result"]["model"]["dampingSource"].get<std::string>().find("loss factors"), std::string::npos);
    // The converse effect.
    const json drive = run(a, "st-beam", "harvester", {{"drive", 10.0}}, *p);
    EXPECT_GT(metric(drive, "actuatorDisplacement"), 0.0);
}

TEST(MatterAnalyses, OrientationRotatesTheProperties)
{
    // A drawn PVDF film: d31 along the drawing direction, d32 = d31 / 7.7 across it.
    auto p = example("h-oriented-film.resonant");
    const json along = run(architecture(*p, "arch-pvdf"), "st-beam", "harvester", {{"tipMass", 0.002}}, *p);
    const json across = run(architecture(*p, "arch-pvdf-turned"), "st-beam", "harvester", {{"tipMass", 0.002}}, *p);
    const double ratio = across["result"]["model"]["theta"].get<double>() / along["result"]["model"]["theta"].get<double>();
    EXPECT_NEAR(ratio, 3.0 / 23.0, 1e-9);
    EXPECT_LT(metric(across, "power"), 0.1 * metric(along, "power"));
    // Principal conductivities rotated: a fibre-like layer tilted by 90 degrees about x.
    auto tbc = example("c-thermal-barrier.resonant");
    Architecture a = architecture(*tbc, "arch-tbc");
    AnisotropicSet set;
    set.values = {{"k1", sv(10.0)}, {"k2", sv(1.0)}, {"k3", sv(1.0)}};
    a.region("topcoat")->anisotropic["thermal"] = set;
    const json flat = run(a, "st-wall", "thermal", json::object(), *tbc);
    EXPECT_NEAR(flat["result"]["layers"][2]["conductivity"].get<double>(), 1.0, 1e-12);  // k3 through the thickness
    a.region("topcoat")->orientation = Orientation{0.0, std::numbers::pi / 2.0, std::numbers::pi / 2.0, "stack"};
    const json tilted = run(a, "st-wall", "thermal", json::object(), *tbc);
    EXPECT_NEAR(tilted["result"]["layers"][2]["conductivity"].get<double>(), 10.0, 1e-9);  // the 1-axis now through the thickness
}

TEST(MatterAnalyses, TheSalisburyScreenAbsorbsAtItsQuarterWave)
{
    auto p = example("e-em-absorber.resonant");
    const Architecture a = architecture(*p, "arch-absorber");
    const json r = run(a, "st-absorber", "electromagnetic", json{{"range", {{"from", 6e9}, {"to", 14e9}, {"points", 161}}}, {"exit", "metal"}}, *p);
    EXPECT_NEAR(metric(r, "frequencyOfMaxAbsorptance") / 10e9, 1.0, 0.03);
    EXPECT_GT(metric(r, "maxAbsorptance"), 0.95);
    EXPECT_LT(metric(r, "transmittance"), 1e-6);
    const json open = run(a, "st-absorber", "electromagnetic", {{"frequencies", json::array({10e9})}}, *p);
    EXPECT_LT(metric(open, "absorptance"), 0.6) << "without the metal plate the screen is a lossy sheet";
}

TEST(MatterAnalyses, LaminateElectricalAndInterfaces)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture a = architecture(*p, "arch-bonded");
    const json lam = run(a, "st-beam", "laminate", {{"deltaT", -100.0}}, *p);
    EXPECT_NEAR(metric(lam, "massPerArea"), 7990 * 5e-4 + 1200 * 2e-5 + 19300 * 6e-6 + 7750 * 2e-4 + 1289 * 1e-5, 1e-9);
    EXPECT_GT(metric(lam, "maxInterfaceStress"), 0.0);
    EXPECT_EQ(lam["result"]["thermal"]["interfaces"].size(), 5u);
    const json el = run(a, "st-beam", "electrical", {{"voltage", 10.0}}, *p);
    EXPECT_GT(metric(el, "seriesResistance"), 1e8);  // the epoxy bond line insulates
    EXPECT_GT(metric(el, "capacitancePerArea"), 0.0);
    const json in = run(a, "st-beam", "interfaces", {{"deltaT", -100.0}}, *p);
    const json& first = in["result"]["interfaces"][0];
    EXPECT_EQ(first["interface"], "i-substrate-bond");
    ASSERT_TRUE(first.contains("acoustic"));
    EXPECT_LT(first["acoustic"]["transmission"].get<double>(), 0.5);  // steel to epoxy: a large impedance mismatch
    EXPECT_TRUE(first.contains("thermalMismatch"));
    EXPECT_EQ(in["representation"], "a computed result of an as-designed model");
}

TEST(MatterAnalyses, UnsupportedFeaturesAreRefused)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture base = architecture(*p, "arch-bonded");
    auto refused = [&](const Architecture& a, const std::string& analysis, const std::string& phrase) {
        auto r = analyzeStack(a, "st-beam", analysis, json::object(), p->materials);
        ASSERT_FALSE(r.ok()) << analysis << " should refuse";
        EXPECT_NE(r.message().find(phrase), std::string::npos) << r.message();
    };
    // A finite-stiffness bond is not lamination theory's perfect bonding.
    Architecture a = base;
    a.interfaceById("i-substrate-bond")->models = {{"finite-stiffness", {{"normalStiffness", sv(1e13)}, {"shearStiffness", sv(1e12)}}}, {"perfect", {}}};
    refused(a, "laminate", "finite-stiffness");
    // An interface that says nothing about heat is not assumed perfect.
    a = base;
    a.interfaceById("i-substrate-bond")->models = {{"cohesive-zone", {{"strength", sv(2e7)}, {"fractureEnergy", sv(300.0)}}}};
    refused(a, "thermal", "no thermal model");
    // No interface at all.
    a = base;
    a.interfaces.erase(a.interfaces.begin());
    refused(a, "laminate", "without an interface");
    // An electrical contact resistance is not in the plane-wave solver.
    a = base;
    a.interfaceById("i-pzt-electrode")->models.push_back({"electrical-contact-resistance", {{"resistance", sv(1e-8)}}});
    refused(a, "electromagnetic", "electrical-contact-resistance");
    // Out-of-plane orientation of an orthotropic layer.
    a = base;
    AnisotropicSet elastic;
    elastic.values = {{"E1", sv(181e9)}, {"E2", sv(10e9)}, {"G12", sv(7e9)}, {"nu12", sv(0.28)}};
    a.region("bond")->anisotropic["elastic"] = elastic;
    a.region("bond")->orientation = Orientation{0.0, 0.4, 0.0, "stack"};
    refused(a, "laminate", "tilted out of the plane");
    // Anisotropic elastic sets in a solver that treats layers as isotropic would be refused;
    // in-plane they are used.
    a.region("bond")->orientation.reset();
    EXPECT_TRUE(analyzeStack(a, "st-beam", "laminate", json::object(), p->materials).ok());
    // A composition without a method is missing, with its bounds.
    a = base;
    a.region("bond")->materialId.clear();
    a.region("bond")->composition.constituents = {{"resin", "mat-fam-epoxy", "", "", "polymer", 0.6, std::nullopt},
                                                  {"filler", "mat-fam-gold", "", "", "metal", 0.4, std::nullopt}};
    refused(a, "laminate", "names no method");
    // A pattern the analysis has no model for.
    a = base;
    a.stack("st-beam")->layers[0].pattern.kind = "periodic";
    refused(a, "laminate", "periodic pattern");
    // A transduction region without coefficients, and a stack without any.
    a = base;
    a.region("pzt")->anisotropic.clear();
    refused(a, "harvester", "no piezoelectric coefficients");
    auto plate = example("a-homogeneous-plate.resonant");
    EXPECT_FALSE(analyzeStack(architecture(*plate, "arch-plate"), "st-plate", "harvester", json::object(), plate->materials).ok());
    EXPECT_FALSE(analyzeStack(base, "st-none", "laminate", json::object(), p->materials).ok());
    EXPECT_FALSE(analyzeStack(base, "st-beam", "quantum-chemistry", json::object(), p->materials).ok());
}

TEST(MatterAnalyses, EffectivePropertiesAreListedWithEveryMethod)
{
    auto p = example("f-multimaterial-structure.resonant");
    const json e = resonant::coupling::effectiveRegionProperties(architecture(*p, "arch-multi"), "core", p->materials);
    bool found = false;
    for (const json& prop : e["properties"])
        if (prop["key"] == "mechanical.youngsModulus") {
            EXPECT_EQ(prop["origin"], "composition");
            found = true;
        }
    EXPECT_TRUE(found);
    EXPECT_GE(e["methods"].size(), 5u);
    EXPECT_TRUE(resonant::coupling::effectiveRegionProperties(architecture(*p, "arch-multi"), "nothing", p->materials).is_null());
}

// --- the three-dimensional layered solid (REQ-CHAT-20261128-004, ADR-000232) -------------------

TEST(MatterAnalyses, TheLayeredSolidConductsLikeTheSeriesModelThroughAFlatStack)
{
    auto c = example("c-thermal-barrier.resonant");
    const auto a = architecture(*c, "arch-tbc");
    auto one = resonant::coupling::analyzeStack(a, "st-wall", "thermal", json::object(), c->materials);
    ASSERT_TRUE(one) << one.message();
    auto three = resonant::coupling::analyzeStack(a, "st-wall", "layered-solid-thermal",
                                                  json{{"thermal", {{"innerTemperature", 373.15}, {"outerTemperature", 293.15}}}}, c->materials);
    ASSERT_TRUE(three) << three.message();
    // Laterally insulated and flat, the 3D conduction is one-dimensional: the same resistance.
    const double r = one.value()["metrics"]["resistance"].get<double>();
    EXPECT_NEAR(three.value()["metrics"]["heatFluxIn"].get<double>(), 80.0 / r, 0.01 * 80.0 / r);
    EXPECT_NE(three.value()["result"]["fidelity"].get<std::string>().find("three-dimensional"), std::string::npos);
    // The mechanical 3D solid refuses the cohesive zone it cannot represent.
    auto mech = resonant::coupling::analyzeStack(a, "st-wall", "layered-solid", json::object(), c->materials);
    ASSERT_FALSE(mech);
    EXPECT_NE(mech.message().find("cohesive-zone"), std::string::npos) << mech.message();
}

TEST(MatterAnalyses, TheLayeredSolidModesLieBetweenTheNarrowBeamAndTheWidePlate)
{
    auto d = example("d-graded-metal-ceramic.resonant");
    const auto a = architecture(*d, "arch-step");
    auto narrow = resonant::coupling::analyzeStack(a, "st-fgm", "modes", json::object(), d->materials);
    auto wide = resonant::coupling::analyzeStack(a, "st-fgm", "modes", json{{"wide", true}}, d->materials);
    auto solid = resonant::coupling::analyzeStack(a, "st-fgm", "layered-solid", json{{"mesh", {{"elementsU", 10}, {"elementsV", 6}}}}, d->materials);
    ASSERT_TRUE(narrow && wide) << narrow.message() << wide.message();
    ASSERT_TRUE(solid) << solid.message();
    const double f = solid.value()["metrics"]["f1"].get<double>();
    EXPECT_GT(f, 0.99 * narrow.value()["metrics"]["f1"].get<double>());
    EXPECT_LT(f, 1.02 * wide.value()["metrics"]["f1"].get<double>());
    // The graded architecture is divided into sublayers of constant properties, and says so.
    const auto g = architecture(*d, "arch-fgm");
    auto graded = resonant::coupling::analyzeStack(g, "st-fgm", "layered-solid", json{{"sublayers", 6}}, d->materials);
    ASSERT_TRUE(graded) << graded.message();
    EXPECT_EQ(graded.value()["result"]["layers"].get<int>(), 6);
}

TEST(MatterAnalyses, TheLayeredSolidReportsInterfaceTractionsAndRefusesWhatItCannotDo)
{
    auto d = example("d-graded-metal-ceramic.resonant");
    auto a = architecture(*d, "arch-step");
    // Declare the interface as a spring layer with strengths (the 3D solid reads them).
    for (auto& i : a.interfaces)
        if (i.id == "i-step") {
            InterfaceModel m;
            m.kind = "finite-stiffness";
            for (const auto& [key, value] : std::vector<std::pair<std::string, double>>{
                     {"normalStiffness", 1e14}, {"shearStiffness", 5e13}, {"normalStrength", 50e6}, {"shearStrength", 80e6}}) {
                SourcedValue v;
                v.value = value;
                v.source = "test";
                m.parameters[key] = v;
            }
            i.models = {m};
        }
    const json options{{"analysis", "static"},
                       {"mesh", {{"elementsU", 10}, {"elementsV", 2}}},
                       {"loads", {{"edgeForces", json::array({{{"edge", "u1"}, {"force", json::array({0, 0, -50.0})}}})}}}};
    auto r = resonant::coupling::analyzeStack(a, "st-fgm", "layered-solid", options, d->materials);
    ASSERT_TRUE(r) << r.message();
    const json& result = r.value()["result"];
    ASSERT_EQ(result["interfaces"].size(), 1u);
    EXPECT_GT(result["interfaces"][0]["maxShearTraction"].get<double>(), 0.0);
    EXPECT_TRUE(result["interfaces"][0].contains("maxFailureIndex"));
    EXPECT_TRUE(r.value()["metrics"].contains("maxFailureIndex"));
    EXPECT_GT(r.value()["metrics"]["maxDisplacement"].get<double>(), 0.0);
    // A static analysis without a load, and an unknown analysis, are refused.
    EXPECT_FALSE(resonant::coupling::analyzeStack(a, "st-fgm", "layered-solid", json{{"analysis", "static"}}, d->materials));
    EXPECT_FALSE(resonant::coupling::analyzeStack(a, "st-fgm", "layered-solid", json{{"analysis", "buckling"}}, d->materials));
    // A curved surface from the options: a quarter cylinder of the same stack.
    auto curved = resonant::coupling::analyzeStack(
        a, "st-fgm", "layered-solid",
        json{{"surface", {{"kind", "cylinder"}, {"radius", 0.05}, {"length", 0.02}, {"sweep", 1.5707963267948966}}},
             {"supports", {{"clamped", json::array({"u0", "u1"})}}},
             {"analysis", "static"},
             {"loads", {{"innerPressure", 1e6}}}},
        d->materials);
    ASSERT_TRUE(curved) << curved.message();
    EXPECT_NE(curved.value()["result"]["surface"].get<std::string>().find("cylinder"), std::string::npos);
}

// --- tensor material fields in architectures (REQ-CHAT-20261128-006, RES-FAM-021) ------------

TEST(MatterAnalyses, MaterialFieldsReachTheThreeDimensionalSolidAndOnlyIt)
{
    auto d = example("d-graded-metal-ceramic.resonant");
    const auto plain = architecture(*d, "arch-step");
    auto withFields = plain;
    // Fields equal to each region's resolved properties, and then a fourfold density.
    const auto fieldsFor = [&](double densityScale) {
        std::vector<resonant::materials::MaterialField> out;
        for (const auto& r : plain.regions) {
            const double e = resolve(plain, r, "mechanical.youngsModulus", 0.5, d->materials).value;
            const double nu = resolve(plain, r, "mechanical.poissonsRatio", 0.5, d->materials).value;
            const double rho = resolve(plain, r, "mechanical.density", 0.5, d->materials).value;
            const double l = e * nu / ((1 + nu) * (1 - 2 * nu)), g = e / (2 * (1 + nu));
            resonant::materials::MaterialField c;
            c.id = "c-" + r.id;
            c.region = r.id;
            c.property = "mechanical.stiffness";
            c.rank = resonant::materials::FieldRank::Stiffness6;
            c.profile = "constant";
            for (int i = 0; i < 6; ++i)
                for (int j = i; j < 6; ++j)
                    c.value0.push_back(i < 3 && j < 3 ? (i == j ? l + 2 * g : l) : (i == j ? g : 0.0));
            resonant::materials::MaterialField m;
            m.id = "rho-" + r.id;
            m.region = r.id;
            m.property = "mechanical.density";
            m.profile = "constant";
            m.value0 = {densityScale * rho};
            out.push_back(c);
            out.push_back(m);
        }
        return out;
    };
    const json options{{"mesh", {{"elementsU", 8}, {"elementsV", 2}}}, {"modes", 3}};
    auto reference = analyzeStack(plain, "st-fgm", "layered-solid", options, d->materials);
    ASSERT_TRUE(reference) << reference.message();
    withFields.materialFields = fieldsFor(1.0);
    EXPECT_FALSE(hasErrors(validate(withFields, d.get())));
    auto same = analyzeStack(withFields, "st-fgm", "layered-solid", options, d->materials);
    ASSERT_TRUE(same) << same.message();
    const auto f0 = reference.value()["result"]["frequencies"], f1 = same.value()["result"]["frequencies"];
    for (size_t i = 0; i < f0.size(); ++i)
        EXPECT_NEAR(f1[i].get<double>(), f0[i].get<double>(), 1e-9 * f0[i].get<double>());
    withFields.materialFields = fieldsFor(4.0);
    auto heavy = analyzeStack(withFields, "st-fgm", "layered-solid", options, d->materials);
    ASSERT_TRUE(heavy) << heavy.message();
    for (size_t i = 0; i < f0.size(); ++i)
        EXPECT_NEAR(heavy.value()["result"]["frequencies"][i].get<double>(), 0.5 * f0[i].get<double>(), 1e-9 * f0[i].get<double>());
    // The fields survive the architecture's JSON form.
    auto back = Architecture::fromJson(withFields.toJson());
    ASSERT_TRUE(back) << back.message();
    EXPECT_EQ(back.value().materialFields.size(), withFields.materialFields.size());
    // An analysis that does not read tensor fields refuses the stack, with the reason.
    auto laminate = analyzeStack(withFields, "st-fgm", "laminate", json::object(), d->materials);
    ASSERT_FALSE(laminate);
    EXPECT_NE(laminate.message().find("does not read the material field"), std::string::npos) << laminate.message();
}
