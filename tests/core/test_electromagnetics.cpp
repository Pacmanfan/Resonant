// Validation of the electromagnetic models against closed-form references.
// docs/electromagnetics/ELECTROMAGNETIC_VALIDATION.md records the results.
#include "dsp/Spectrum.h"
#include "electromagnetics/Antennas.h"
#include "electromagnetics/ArrayFactor.h"
#include "electromagnetics/EmMaterial.h"
#include "electromagnetics/Lines.h"
#include "electromagnetics/Yee.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr double kF = 1e9;  // 1 GHz: wavelength 0.2998 m

Complex hankel2(int n, double x)
{
    return {std::cyl_bessel_j(static_cast<double>(n), x), -std::cyl_neumann(static_cast<double>(n), x)};
}

double degrees(double rad) { return rad * 180.0 / kPi; }
double radians(double deg) { return deg * kPi / 180.0; }

}  // namespace

// --- foundations -------------------------------------------------------------------------

TEST(EmFoundations, ConstantsSineCosineIntegralsAndMaterials)
{
    EXPECT_NEAR(1.0 / std::sqrt(kMu0 * kEpsilon0), kSpeedOfLight, 1e-3);
    EXPECT_NEAR(std::sqrt(kMu0 / kEpsilon0), kEta0, 1e-6);
    // Reference values (Abramowitz and Stegun, table 5.1).
    EXPECT_NEAR(sineIntegral(1.0), 0.946083070367183, 1e-13);
    EXPECT_NEAR(cosineIntegral(1.0), 0.337403922900968, 1e-13);
    EXPECT_NEAR(sineIntegral(kPi), 1.851937051982466, 1e-12);
    EXPECT_NEAR(cosineIntegral(5.0), -0.190029749656644, 1e-12);
    EXPECT_NEAR(sineIntegral(10.0), 1.658347594218874, 1e-12);
    EXPECT_NEAR(cosineIntegral(2.0 * kPi), -0.0225606617463461, 1e-12);  // mpmath
    EXPECT_NEAR(sineIntegral(2.0 * kPi), 1.41815157613263, 1e-12);

    EmMaterial copper;
    copper.name = "copper";
    copper.conductivity = 5.8e7;
    EXPECT_EQ(copper.classify(kF), MaterialClass::GoodConductor);
    // Skin depth sqrt(2 / (omega mu sigma)) = 2.09 um at 1 GHz.
    EXPECT_NEAR(copper.skinDepth(kF), std::sqrt(2.0 / (2 * kPi * kF * kMu0 * 5.8e7)), 1e-10);
    EXPECT_NEAR(copper.surfaceResistance(kF), 1.0 / (5.8e7 * copper.skinDepth(kF)), 1e-6);
    EmMaterial ptfe;
    ptfe.relativePermittivity = 2.1;
    ptfe.lossTangent = 2e-4;
    EXPECT_EQ(ptfe.classify(kF), MaterialClass::Dielectric);
    EXPECT_NEAR(std::abs(ptfe.impedance(kF)), kEta0 / std::sqrt(2.1), 1e-3);
    EXPECT_NEAR(ptfe.wavenumber(kF).real(), wavenumber(kF) * std::sqrt(2.1), 1e-6);
    EXPECT_LT(ptfe.wavenumber(kF).imag(), 0.0);  // e^{+j omega t}: decays along the propagation
    // Normal incidence from vacuum onto er = 4: Gamma = -1/3, T = 2/3.
    EXPECT_NEAR(normalReflection(kEta0, kEta0 / 2.0).real(), -1.0 / 3.0, 1e-15);
    EXPECT_NEAR(normalTransmission(kEta0, kEta0 / 2.0).real(), 2.0 / 3.0, 1e-15);
    // From a record, with frequency tables and validity.
    Material record(Id("m"), "Glass-ish");
    PropertyValue er;
    er.value = 6.0;
    er.frequencyTable = {{1e6, 7.0}, {1e9, 6.0}};
    er.validity.maxFrequencyHz = 2e9;
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Electromagnetic, "relativePermittivity", er).ok());
    PropertyValue mu;
    mu.value = 1.0;
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Electromagnetic, "relativePermeability", mu).ok());
    auto m = emMaterialOf(record, 5e5);
    ASSERT_TRUE(m.ok());
    EXPECT_DOUBLE_EQ(m.value().relativePermittivity, 7.0);  // clamped to the table's end
    EXPECT_FALSE(m.value().outsideValidity);
    EXPECT_TRUE(emMaterialOf(record, 5e9).value().outsideValidity);
    Material water(Id("w"), "No EM");
    EXPECT_FALSE(emMaterialOf(water, kF).ok());
}

// --- antennas ----------------------------------------------------------------------------

TEST(EmAntennas, HertzianDipoleRadiatesTheClassicalPowerAtEveryDistance)
{
    const Complex il(1e-3, 0.0);  // A m
    const double lambda = wavelength(kF);
    const double expected = kEta0 * kPi / 3.0 * std::norm(il / lambda);  // eta pi / 3 |I l / lambda|^2 (40 pi^2 |I l / lambda|^2 with eta = 120 pi)
    for (const double r : {0.05 * lambda, 0.3 * lambda, 20.0 * lambda}) {
        double power = 0.0;
        const int n = 400;
        for (int i = 0; i < n; ++i) {
            const double t = (i + 0.5) * kPi / n;
            const Vec3 p = r * Vec3(std::sin(t), 0.0, std::cos(t));
            const FieldPoint f = hertzianDipoleField(p, il, kF);
            const Vec3 s = 0.5 * (f.e.cross(f.h.conjugate())).real();
            power += s.dot(p / r) * 2 * kPi * r * r * std::sin(t) * kPi / n;
        }
        EXPECT_NEAR(power, expected, 1e-5 * expected) << r / lambda;
    }
    // Far field: E = eta H x r^, E_theta = j eta k I l sin(theta) e^{-jkr} / (4 pi r).
    const double r = 100.0 * lambda;
    const FieldPoint far = hertzianDipoleField(Vec3(r, 0, 0), il, kF);
    EXPECT_NEAR(std::abs(far.e.z()), kEta0 * wavenumber(kF) * 1e-3 / (4 * kPi * r), 1e-4 * std::abs(far.e.z()));
    EXPECT_NEAR(std::abs(far.e.z()) / std::abs(far.h.y()), kEta0, 1e-3);
    // The small loop is its dual: same power with I l -> j omega mu I A... check the pattern shape sin(theta).
    const FieldPoint loop45 = smallLoopField(r * Vec3(std::sqrt(0.5), 0, std::sqrt(0.5)), Complex(1e-4, 0), kF);
    const FieldPoint loop90 = smallLoopField(r * Vec3(1, 0, 0), Complex(1e-4, 0), kF);
    EXPECT_NEAR(loop45.e.norm() / loop90.e.norm(), std::sqrt(0.5), 1e-4);
    EXPECT_TRUE(hertzianDipoleField(Vec3::Zero(), il, kF).singular);
}

TEST(EmAntennas, ThinDipoleImpedanceAndPatternMatchTheClassicalValues)
{
    const double lambda = wavelength(kF);
    // Half-wave: Z = (eta / 4 pi) (gamma + ln 2 pi - Ci 2 pi) + j (eta / 4 pi) Si 2 pi = 73.08 + j 42.51
    // (the textbook 73.13 + j 42.54 uses eta = 120 pi), independent of the radius.
    const double scale = kEta0 / (4 * kPi);
    const AntennaImpedance half = thinDipoleImpedance(0.5 * lambda, 1e-5 * lambda, kF);
    EXPECT_NEAR(half.input.real(), scale * (0.5772156649015329 + std::log(2 * kPi) + 0.0225606617463461), 1e-9);
    EXPECT_NEAR(half.input.imag(), scale * 1.41815157613263, 1e-9);
    EXPECT_NEAR(half.input.real(), 73.08, 0.01);
    EXPECT_NEAR(half.input.imag(), 42.51, 0.01);
    // Quarter-wave monopole: half of it.
    const AntennaImpedance mono = monopoleImpedance(0.25 * lambda, 1e-5 * lambda, kF);
    EXPECT_NEAR(mono.input.real(), 0.5 * half.input.real(), 1e-9);
    EXPECT_NEAR(mono.input.imag(), 0.5 * half.input.imag(), 1e-9);
    // Directivity 1.641 (2.15 dBi) for the half-wave dipole, 1.5 for a short one.
    const auto pattern = [&](double length) {
        return directivityOfAxisymmetric([&](double t) { return std::pow(thinDipolePattern(t, length, kF), 2); });
    };
    EXPECT_NEAR(pattern(0.5 * lambda), 1.6409, 1e-3);
    EXPECT_NEAR(pattern(0.01 * lambda), 1.5, 1e-3);
    EXPECT_NEAR(pattern(1.25 * lambda), 3.3, 0.05);  // 1.25 wavelength: the known maximum directivity of a centre-fed wire, about 3.3
    // Resonance slightly below half a wavelength, shorter for thicker wires.
    const double thin = resonantDipoleLength(1e-4 * lambda, kF), thick = resonantDipoleLength(1e-2 * lambda, kF);
    EXPECT_GT(thin, 0.46 * lambda);
    EXPECT_LT(thin, 0.5 * lambda);
    EXPECT_LT(thick, thin);
    EXPECT_NEAR(thinDipoleImpedance(thin, 1e-4 * lambda, kF).input.imag(), 0.0, 1e-6);
    // Near antiresonance the model says it is not valid.
    EXPECT_FALSE(thinDipoleImpedance(1.0 * lambda, 1e-4 * lambda, kF).valid);
    // A short thin dipole's far field is a Hertzian dipole of moment I L / 2 (triangular current).
    const double shortLength = 0.01 * lambda, r = 5 * lambda;
    const FieldPoint a = thinDipoleField(Vec3(r, 0, 0), shortLength, Complex(1, 0), kF);
    const FieldPoint b = hertzianDipoleField(Vec3(r, 0, 0), Complex(0.5 * shortLength, 0), kF);
    EXPECT_NEAR(std::abs(a.e.z()) / std::abs(b.e.z()), 1.0, 1e-3);
    // The exact field of the sinusoidal current approaches the pattern far away.
    const double rFar = 50 * lambda, t = radians(60);
    const FieldPoint farField = thinDipoleField(rFar * Vec3(std::sin(t), 0, std::cos(t)), 0.5 * lambda, Complex(1, 0), kF);
    EXPECT_NEAR(farField.e.norm() * rFar / (kEta0 / (2 * kPi)), std::abs(thinDipolePattern(t, 0.5 * lambda, kF)), 2e-3);
    // Small loop resistance 20 pi^2 (C / lambda)^4.
    const double b0 = 0.1 * lambda / (2 * kPi);
    EXPECT_NEAR(smallLoopImpedance(b0, b0 / 50, kF).input.real(), 20 * kPi * kPi * 1e-4, 1e-9);
}

TEST(EmAntennas, MatchingQuantities)
{
    const MatchQuantities matched = match(Complex(50, 0), 50);
    EXPECT_NEAR(std::abs(matched.reflection), 0.0, 1e-15);
    EXPECT_DOUBLE_EQ(matched.vswr, 1.0);
    EXPECT_TRUE(std::isinf(matched.returnLossDb));
    const MatchQuantities open = match(Complex(1e12, 0), 50);
    EXPECT_NEAR(std::abs(open.reflection), 1.0, 1e-9);
    const MatchQuantities dipole = match(Complex(73.13, 42.54), 50);
    EXPECT_NEAR(std::abs(dipole.reflection), std::abs(Complex(23.13, 42.54) / Complex(123.13, 42.54)), 1e-12);
    EXPECT_NEAR(dipole.vswr, (1 + std::abs(dipole.reflection)) / (1 - std::abs(dipole.reflection)), 1e-12);
    EXPECT_NEAR(dipole.mismatchEfficiency, 1 - std::norm(dipole.reflection), 1e-12);
}

// --- arrays ----------------------------------------------------------------------------------

namespace {

ArrayModel linear(int n, double spacingWavelengths, ElementPattern pattern = ElementPattern::CosineHemisphere)
{
    ArrayModel a;
    a.pattern = pattern;
    for (int i = 0; i < n; ++i) {
        ArrayElement e;
        e.position = Vec3((i - 0.5 * (n - 1)) * spacingWavelengths * wavelength(kF), 0, 0);
        a.elements.push_back(e);
    }
    return a;
}

ArrayModel planar(int n, double spacingWavelengths)
{
    ArrayModel a;
    a.pattern = ElementPattern::CosineHemisphere;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            ArrayElement e;
            e.position = spacingWavelengths * wavelength(kF) * Vec3(i - 0.5 * (n - 1), j - 0.5 * (n - 1), 0);
            a.elements.push_back(e);
        }
    return a;
}

// Largest sidelobe of |AF| along the xz cut, by fine scanning of the model itself.
double cutSidelobeDb(const ArrayModel& a)
{
    std::vector<double> v;
    for (int i = -9000; i <= 9000; ++i)
        v.push_back(std::abs(farFieldResponse(a, directionOf(std::abs(radians(i / 100.0)), i < 0 ? kPi : 0.0), kF)));
    const double peak = *std::max_element(v.begin(), v.end());
    double side = 0.0;
    for (size_t i = 1; i + 1 < v.size(); ++i)
        if (v[i] > v[i - 1] && v[i] >= v[i + 1] && v[i] < 0.99 * peak)
            side = std::max(side, v[i]);
    return 20 * std::log10(side / peak);
}

}  // namespace

TEST(EmArrays, UniformLinearArrayHasTheTextbookDirectivityBeamwidthAndSidelobes)
{
    // Isotropic elements along x at lambda/2: D = N exactly; HPBW about 0.886 lambda / (N d).
    const ArrayModel iso = linear(16, 0.5, ElementPattern::Isotropic);
    ArrayModel cosine = linear(16, 0.5);
    const PatternAnalysis a = analyzePattern(cosine, kF, 0.25);
    EXPECT_NEAR(degrees(a.mainTheta), 0.0, 0.3);
    EXPECT_NEAR(degrees(a.beamwidthTheta), degrees(0.886 * 2.0 / 16.0), 0.3);
    EXPECT_NEAR(a.sidelobeDb, cutSidelobeDb(cosine), 0.15);
    // The array factor's own first sidelobe for N = 16: -13.15 dB (-13.26 dB for large N).
    double side = 0.0;
    for (int i = 1; i < 20000; ++i) {
        const double psi = kPi * i / 20000.0;
        const double af = std::abs(std::sin(8 * psi) / (16 * std::sin(0.5 * psi)));
        if (psi > 2.0 * kPi / 16.0)
            side = std::max(side, af);
    }
    EXPECT_NEAR(20 * std::log10(side), -13.15, 0.05);
    // Directivity of the isotropic array, by integrating its pattern.
    const PatternAnalysis isotropic = analyzePattern(iso, kF, 0.25);
    EXPECT_NEAR(isotropic.directivity, 16.0, 0.1);
    EXPECT_GT(a.directivity, isotropic.directivity);  // the element pattern adds directivity
}

TEST(EmArrays, PhaseSteeringPointsTheBeamAndSquintsAtOtherFrequenciesTimeDelayDoesNot)
{
    ArrayModel p = planar(8, 0.5);
    steer(p, directionOf(radians(30), radians(45)), kF, SteeringMethod::Phase);
    const PatternAnalysis a = analyzePattern(p, kF, 0.25);
    EXPECT_NEAR(degrees(a.mainTheta), 30.0, 1.0);
    EXPECT_NEAR(degrees(a.mainPhi), 45.0, 1.0);
    // A linear array steered to 30 degrees in the xz plane, then used at 1.2 f.
    ArrayModel phase = linear(16, 0.5), delay = linear(16, 0.5);
    steer(phase, directionOf(radians(30), 0.0), kF, SteeringMethod::Phase);
    steer(delay, directionOf(radians(30), 0.0), kF, SteeringMethod::TimeDelay);
    const auto beamAt = [](const ArrayModel& m, double f) {
        double best = 0.0, bestTheta = 0.0;
        for (int i = 0; i <= 9000; ++i) {
            const double t = radians(i / 100.0), v = std::abs(farFieldResponse(m, directionOf(t, 0.0), f));
            if (v > best) {
                best = v;
                bestTheta = t;
            }
        }
        return degrees(bestTheta);
    };
    EXPECT_NEAR(beamAt(phase, kF), 30.0, 0.5);
    EXPECT_NEAR(beamAt(delay, kF), 30.0, 0.5);
    // The cosine element pattern pulls slightly towards broadside; the array factor alone gives asin(0.5 / 1.2).
    phase.pattern = delay.pattern = ElementPattern::Isotropic;
    EXPECT_NEAR(beamAt(phase, 1.2 * kF), degrees(std::asin(0.5 / 1.2)), 0.1);
    EXPECT_NEAR(beamAt(delay, 1.2 * kF), 30.0, 0.1);
}

TEST(EmArrays, GratingLobesArePredictedAndFound)
{
    ArrayModel wide = linear(12, 0.8, ElementPattern::Isotropic);
    const Vec3 steerTo = directionOf(radians(40), 0.0);
    steer(wide, steerTo, kF, SteeringMethod::Phase);
    const auto predicted = predictedGratingLobes({Vec3(0.8 * wavelength(kF), 0, 0)}, steerTo, kF);
    ASSERT_EQ(predicted.size(), 1u);
    // sin(theta) = sin 40 - 1 / 0.8 along x.
    EXPECT_NEAR(predicted[0].x(), std::sin(radians(40)) - 1.25, 1e-12);
    // The array factor there equals the main lobe.
    EXPECT_NEAR(std::abs(arrayFactor(wide, predicted[0], kF)), std::abs(arrayFactor(wide, steerTo, kF)), 1e-9 * 12);
    // Half-wavelength spacing at broadside: none. Planar 0.7 lambda steered to 45 degrees: some.
    EXPECT_TRUE(predictedGratingLobes({Vec3(0.5 * wavelength(kF), 0, 0)}, Vec3::UnitZ(), kF).empty());
    EXPECT_FALSE(predictedGratingLobes({Vec3(0.7 * wavelength(kF), 0, 0), Vec3(0, 0.7 * wavelength(kF), 0)}, directionOf(radians(45), 0.0), kF).empty());
    // The sampled analysis finds the equal maximum of the planar case.
    ArrayModel p = planar(8, 0.9);
    steer(p, directionOf(radians(40), 0.0), kF, SteeringMethod::Phase);
    p.pattern = ElementPattern::Isotropic;
    const PatternAnalysis a = analyzePattern(p, kF, 0.5);
    EXPECT_FALSE(a.equalMaxima.empty());
}

TEST(EmArrays, ChebyshevWeightsGiveEqualSidelobesAndSynthesisPlacesNulls)
{
    const std::vector<double> w = chebyshevWeights(10, 30.0);
    ASSERT_EQ(w.size(), 10u);
    EXPECT_NEAR(w.front(), w.back(), 1e-12);  // symmetric
    ArrayModel a = linear(10, 0.5, ElementPattern::Isotropic);
    for (size_t i = 0; i < w.size(); ++i)
        a.elements[i].weight = w[i];
    // Every sidelobe of the array factor at -30 dB.
    std::vector<double> v;
    for (int i = 0; i <= 18000; ++i)
        v.push_back(std::abs(arrayFactor(a, directionOf(radians(i / 100.0), 0.0), kF)));
    const double peak = *std::max_element(v.begin(), v.end());
    int lobes = 0;
    for (size_t i = 1; i + 1 < v.size(); ++i)
        if (v[i] > v[i - 1] && v[i] >= v[i + 1] && v[i] < 0.9 * peak) {
            EXPECT_NEAR(20 * std::log10(v[i] / peak), -30.0, 0.1);
            ++lobes;
        }
    EXPECT_GE(lobes, 6);
    // A main beam at broadside and nulls at +20 and -35 degrees.
    ArrayModel s = linear(16, 0.5, ElementPattern::Isotropic);
    const auto solved = synthesizeBeams(s, {{Vec3::UnitZ(), Complex(16.0, 0.0), 1.0}, {directionOf(radians(20), 0.0), Complex(0, 0), 10.0},
                                             {directionOf(radians(35), kPi), Complex(0, 0), 10.0}},
                                        kF, 1e-6);
    ASSERT_TRUE(solved.ok()) << solved.message();
    for (size_t i = 0; i < s.elements.size(); ++i)
        s.elements[i].weight = solved.value().weights[i];
    const double main = std::abs(arrayFactor(s, Vec3::UnitZ(), kF));
    EXPECT_NEAR(main, 16.0, 1e-3);
    EXPECT_LT(std::abs(arrayFactor(s, directionOf(radians(20), 0.0), kF)), 1e-4 * main);
    EXPECT_LT(std::abs(arrayFactor(s, directionOf(radians(35), kPi), kF)), 1e-4 * main);
    // An amplitude limit is honoured and its cost shows in the residual.
    const auto limited = synthesizeBeams(s, {{Vec3::UnitZ(), Complex(40.0, 0.0), 1.0}}, kF, 1e-6, 1.0);
    ASSERT_TRUE(limited.ok());
    EXPECT_TRUE(limited.value().amplitudeLimited);
    EXPECT_LE(limited.value().largestWeight, 1.0 + 1e-12);
    EXPECT_NEAR(limited.value().relativeResidual, 1.0 - 16.0 / 40.0, 1e-6);
}

// --- lines and guides ------------------------------------------------------------------------

TEST(EmLines, CharacteristicImpedancesAgainstReferences)
{
    // Coax: 50 ohm in air at b/a = exp(2 pi 50 / eta0) = 2.3026.
    EXPECT_NEAR(coaxialLine(1.0, std::exp(2 * kPi * 50 / kEta0), 1.0).characteristicImpedance, 50.0, 1e-9);
    EXPECT_NEAR(coaxialLine(0.45e-3, 1.47e-3, 2.25).characteristicImpedance, 47.3, 0.2);  // a PTFE-like coax
    // Microstrip, Pozar example 3.7: er = 2.2, h = 1.59 mm, W = 4.90 mm is a 50 ohm line with er_eff = 1.87.
    const LineParameters ms = microstripLine(4.90e-3, 1.59e-3, 2.2);
    EXPECT_NEAR(ms.characteristicImpedance, 50.0, 1.0);
    EXPECT_NEAR(ms.effectivePermittivity, 1.87, 0.02);
    // Stripline: exact against Pozar's approximate formula (within about 1 %) at W / b = 1, er = 1.
    EXPECT_NEAR(striplineLine(1.0, 1.0, 1.0).characteristicImpedance, 30 * kPi / (1.0 + 0.441), 0.015 * 65.4);
    // CPW: K(k) = K(k') at k = 1/sqrt(2), so Z0 = eta0 / (4 sqrt(er_eff)).
    const double s = 1.0, g = 0.5 * (std::sqrt(2.0) - 1.0);
    EXPECT_NEAR(coplanarLine(s, g, 9.8).characteristicImpedance, kEta0 / (4 * std::sqrt(5.4)), 1e-9);
    EXPECT_NEAR(ellipticK(0.0), kPi / 2, 1e-15);
    EXPECT_NEAR(ellipticK(std::sqrt(0.5)), 1.854074677301372, 1e-12);
}

TEST(EmLines, InputImpedanceTransformersAndMatchingNetworks)
{
    const double z0 = 50.0, beta = 2 * kPi;  // wavelength 1 m
    EXPECT_NEAR(std::abs(inputImpedance(Complex(100, 0), z0, beta, 0.25) - Complex(25, 0)), 0.0, 1e-9);   // quarter wave: Z0^2 / ZL
    EXPECT_NEAR(std::abs(inputImpedance(Complex(30, 20), z0, beta, 0.5) - Complex(30, 20)), 0.0, 1e-9);  // half wave: ZL
    EXPECT_NEAR(std::abs(inputImpedance(Complex(0, 0), z0, beta, 0.125) - Complex(0, 50)), 0.0, 1e-9);   // shorted lambda/8: j Z0
    EXPECT_NEAR(quarterWaveTransformer(50, 200), 100.0, 1e-12);
    // Both L-network solutions bring the load to the source resistance.
    for (const Complex load : {Complex(200, -100), Complex(10, 15)}) {
        const auto matches = lNetworkMatches(load, 50.0);
        ASSERT_EQ(matches.size(), 2u);
        for (const LMatch& m : matches) {
            Complex z;
            if (m.shuntAtLoad) {
                const Complex y = 1.0 / load + Complex(0, m.shuntSusceptance);
                z = 1.0 / y + Complex(0, m.seriesReactance);
            } else {
                const Complex zs = load + Complex(0, m.seriesReactance);
                z = 1.0 / (1.0 / zs + Complex(0, m.shuntSusceptance));
            }
            EXPECT_NEAR(std::abs(z - Complex(50, 0)), 0.0, 1e-9) << load;
        }
    }
    // Dielectric loss k tan(delta) / 2.
    EXPECT_NEAR(dielectricLoss(4.0, 0.01, kF), 0.5 * wavenumber(kF) * 2.0 * 0.01, 1e-12);
}

TEST(EmGuides, CutoffsModesImpedancesAndTheTE10Field)
{
    // WR-90: 22.86 x 10.16 mm.
    const auto modes = rectangularGuideModes(22.86e-3, 10.16e-3, 6);
    ASSERT_GE(modes.size(), 4u);
    EXPECT_EQ(modes[0].label(), "TE10");
    EXPECT_NEAR(modes[0].cutoffHz / 1e9, 6.557, 0.001);
    EXPECT_EQ(modes[1].label(), "TE20");
    EXPECT_NEAR(modes[1].cutoffHz / 1e9, 13.114, 0.002);
    EXPECT_EQ(modes[2].label(), "TE01");
    EXPECT_NEAR(modes[2].cutoffHz / 1e9, 14.754, 0.002);
    EXPECT_NEAR(modes[3].cutoffHz / 1e9, 16.145, 0.003);  // TE11 and TM11
    const ModePropagation at10 = propagate(modes[0], 10e9);
    ASSERT_TRUE(at10.propagating);
    const double root = std::sqrt(1 - std::pow(6.5571e9 / 10e9, 2));
    EXPECT_NEAR(at10.guideWavelength, wavelength(10e9) / root, 1e-5);
    EXPECT_NEAR(at10.waveImpedance, kEta0 / root, 0.1);
    EXPECT_NEAR(at10.phaseVelocity * at10.groupVelocity, kSpeedOfLight * kSpeedOfLight, 1e6);
    EXPECT_FALSE(propagate(modes[0], 5e9).propagating);
    EXPECT_GT(propagate(modes[0], 5e9).attenuation, 0.0);
    // Bessel zeros (Abramowitz and Stegun 9.5): j'_11 = 1.841184, j_01 = 2.404826, j'_01 = j_11 = 3.831706.
    EXPECT_NEAR(besselZero(1, 1, true), 1.841184, 1e-6);
    EXPECT_NEAR(besselZero(0, 1, false), 2.404826, 1e-6);
    EXPECT_NEAR(besselZero(0, 1, true), 3.831706, 1e-6);
    EXPECT_NEAR(besselZero(1, 1, false), 3.831706, 1e-6);
    const auto circular = circularGuideModes(0.01, 3);
    EXPECT_EQ(circular[0].label(), "TE11");
    EXPECT_NEAR(circular[0].cutoffHz, kSpeedOfLight * 1.8411838 / (2 * kPi * 0.01), 1e-7 * circular[0].cutoffHz);
    EXPECT_EQ(circular[1].label(), "TM01");
    // Cavity: the lowest mode of a 30 x 15 x 40 mm box is TE101 at (c/2) sqrt(1/a^2 + 1/d^2).
    const auto cavity = rectangularCavityModes(0.03, 0.015, 0.04, 3);
    EXPECT_NEAR(cavity[0].frequencyHz, 0.5 * kSpeedOfLight * std::hypot(1 / 0.03, 1 / 0.04), 1.0);
    // TE10: zero at the side walls, and the power formula equals the integrated Poynting flux.
    const double a = 22.86e-3, b = 10.16e-3;
    EXPECT_NEAR(std::abs(te10Field(a, 0.0, 0.0, 10e9).ey), 0.0, 1e-15);
    double flux = 0.0;
    for (int i = 0; i < 2000; ++i) {
        const double x = (i + 0.5) * a / 2000;
        const TE10Field f = te10Field(a, x, 0.0, 10e9, Complex(1000.0, 0.0));
        flux += -0.5 * (f.ey * std::conj(f.hx)).real() * b * a / 2000;
    }
    EXPECT_NEAR(flux, te10Power(a, b, 10e9, 1000.0), 1e-6 * flux);
    // Parallel plate: TEM first.
    EXPECT_EQ(parallelPlateModes(0.01, 3)[0].kind, ModeKind::TEM);
}

// --- full-wave (Yee) --------------------------------------------------------------------------

namespace {

YeeSpec grid2D(int n, int perWavelength, YeeBoundary boundary = YeeBoundary::Absorbing)
{
    YeeSpec s;
    s.nx = s.ny = n;
    s.nz = 1;
    s.cell = wavelength(kF) / perWavelength;
    s.boundary.fill(boundary);
    s.dt = 0.95 * yeeStabilityLimit(s.cell, kSpeedOfLight, 2);
    return s;
}

dsp::Waveform continuous()
{
    dsp::Waveform w;
    w.kind = dsp::WaveformKind::Continuous;
    w.frequencyHz = kF;
    return w;
}

YeeOutput mustRun(const YeeSpec& s, const std::vector<YeeSource>& sources, long steps, const YeeRecording& rec, const std::vector<YeeProbe>& probes = {})
{
    YeeOutput out;
    const YeeRunEnd end = runYee(s, sources, probes, steps, rec, out);
    EXPECT_EQ(end.kind, YeeRunEnd::Kind::Completed) << end.message;
    return out;
}

size_t node2D(const YeeSpec& s, int i, int j) { return static_cast<size_t>(i) + static_cast<size_t>(s.nx + 1) * static_cast<size_t>(j); }

// Exact Ez of a line current I at rho' = (d, 0) next to a PEC cylinder of radius a at the origin.
Complex cylinderField(double k, double a, double d, const Vec3& p, double currentFactor)
{
    const double rho = std::hypot(p.x(), p.y()), phi = std::atan2(p.y(), p.x());
    Complex total = -currentFactor * hankel2(0, k * (p - Vec3(d, 0, 0)).norm());
    const int terms = static_cast<int>(k * std::max(a, d) + 30);
    for (int n = 0; n <= terms; ++n) {
        const Complex term = std::cyl_bessel_j(static_cast<double>(n), k * a) / hankel2(n, k * a) * hankel2(n, k * d) * hankel2(n, k * rho);
        total += currentFactor * (n == 0 ? 1.0 : 2.0 * std::cos(n * phi)) * term;
    }
    return total;
}

}  // namespace

TEST(EmYee, StabilityLimitAndBoundaryRulesAreEnforced)
{
    YeeSpec s = grid2D(20, 20);
    EXPECT_TRUE(s.validate().ok());
    s.dt *= 1.06;
    EXPECT_NE(s.validate().message().find("stability limit"), std::string::npos);
    s = grid2D(20, 20);
    s.boundary[0] = YeeBoundary::Periodic;
    EXPECT_NE(s.validate().message().find("both faces"), std::string::npos);
    s = grid2D(20, 20);
    s.materials.push_back(YeeMaterial{"slow", 4.0, 1.0, 0.0, false});
    s.cellMaterial.assign(400, 0);
    s.cellMaterial[210] = 1;
    EXPECT_TRUE(s.validate().ok());  // a slower material does not lower the limit
    s.materials[1].relativePermittivity = 0.5;
    EXPECT_FALSE(s.validate().ok());
    YeeSpec three = s;
    three.cellMaterial.clear();
    three.nz = 10;
    three.dt = yeeStabilityLimit(three.cell, kSpeedOfLight, 2);
    EXPECT_FALSE(three.validate().ok());
}

TEST(EmYee, LineCurrentIn2DRadiatesTheHankelField)
{
    const int perWavelength = 20;
    const YeeSpec s = grid2D(120, perWavelength);
    const double k = wavenumber(kF), omega = 2 * kPi * kF;
    YeeRecording rec;
    rec.harmonicFrequencyHz = kF;
    rec.harmonicPeriods = 6;
    const long steps = std::lround(22.0 / kF / s.dt);
    const YeeOutput out = mustRun(s, {{60, 60, 0, 2, 1.0, 0.0, continuous()}}, steps, rec);
    // Ez = -(omega mu0 I / 4) H0(2)(k rho).
    for (const int cells : {20, 40}) {
        const double rho = cells * s.cell;
        const Complex expected = -(omega * kMu0 / 4.0) * hankel2(0, k * rho);
        const Complex got = out.harmonic[2][node2D(s, 60 + cells, 60)];
        EXPECT_NEAR(std::abs(got) / std::abs(expected), 1.0, 0.03) << cells;
        const Complex diagonal = out.harmonic[2][node2D(s, 60 + cells * 7 / 10, 60 + cells * 7 / 10)];
        const double rhoDiagonal = std::hypot(cells * 7 / 10, cells * 7 / 10) * s.cell;
        EXPECT_NEAR(std::abs(diagonal) / std::abs((omega * kMu0 / 4.0) * hankel2(0, k * rhoDiagonal)), 1.0, 0.03) << cells;
    }
    // Phase progression between 20 and 40 cells.
    const Complex ratio = out.harmonic[2][node2D(s, 80, 60)] / out.harmonic[2][node2D(s, 100, 60)];
    EXPECT_NEAR(std::arg(ratio / (hankel2(0, k * 20 * s.cell) / hankel2(0, k * 40 * s.cell))), 0.0, 0.1);
    // A z current excites TM only: Ex and Ey stay at rounding level.
    double te = 0.0;
    for (const auto& v : out.harmonic[0])
        te = std::max(te, std::abs(v));
    EXPECT_LT(te, 1e-9 * std::abs(out.harmonic[2][node2D(s, 80, 60)]));
}

TEST(EmYee, PECCylinderScatteringMatchesTheSeriesSolutionAndConvergesWithRefinement)
{
    const double lambda = wavelength(kF), k = wavenumber(kF), omega = 2 * kPi * kF;
    const double a = 0.5 * lambda, d = 1.5 * lambda;
    std::vector<double> errors;
    for (const int perWavelength : {10, 20, 30}) {
        const int n = 5 * perWavelength;  // a 5-wavelength square around the cylinder
        YeeSpec s = grid2D(n, perWavelength);
        s.materials.push_back(YeeMaterial{"pec", 1, 1, 0, true});
        s.cellMaterial.assign(static_cast<size_t>(n) * n, 0);
        const double centre = 0.5 * n * s.cell;
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
                if (std::hypot((i + 0.5) * s.cell - centre, (j + 0.5) * s.cell - centre) <= a)
                    s.cellMaterial[s.cellIndex(i, j, 0)] = 1;
        const int source = static_cast<int>(std::lround((centre + d) / s.cell));
        YeeRecording rec;
        rec.harmonicFrequencyHz = kF;
        const long steps = std::lround(30.0 / kF / s.dt);
        const YeeOutput out = mustRun(s, {{source, n / 2, 0, 2, 1.0, 0.0, continuous()}}, steps, rec);
        double sum = 0.0, reference = 0.0;
        for (int angle = 30; angle <= 330; angle += 30) {
            const double t = radians(angle), r = 1.0 * lambda;
            const int i = static_cast<int>(std::lround((centre + r * std::cos(t)) / s.cell));
            const int j = static_cast<int>(std::lround((centre + r * std::sin(t)) / s.cell));
            const Vec3 p(i * s.cell - centre, j * s.cell - centre, 0);
            const Complex exact = cylinderField(k, a, source * s.cell - centre, p, omega * kMu0 / 4.0);
            sum += std::norm(out.harmonic[2][node2D(s, i, j)] - exact);
            reference += std::norm(exact);
        }
        errors.push_back(std::sqrt(sum / reference));
    }
    EXPECT_LT(errors[1], errors[0]);
    EXPECT_LT(errors[2], errors[1]);
    EXPECT_LT(errors[2], 0.06);
}

TEST(EmYee, PlaneWaveReflectsFromADielectricWithTheFresnelCoefficientAndTheLayerAbsorbs)
{
    // Two cells across, periodic in y: plane waves along x from a sheet of current.
    const auto reflection = [](double er, int pml, int perWavelength = 40) {
        const int scale = perWavelength / 40;
        YeeSpec s;
        s.nx = 600 * scale;
        s.ny = 2;
        s.nz = 1;
        s.cell = wavelength(kF) / perWavelength;
        s.boundary = {YeeBoundary::Absorbing, YeeBoundary::Absorbing, YeeBoundary::Periodic, YeeBoundary::Periodic,
                      YeeBoundary::Absorbing, YeeBoundary::Absorbing};
        s.pmlCells = pml;
        s.materials.push_back(YeeMaterial{"dielectric", er, 1.0, 0.0, false});
        s.cellMaterial.assign(static_cast<size_t>(1200 * scale), 0);
        for (int i = 400 * scale; i < 600 * scale; ++i)
            for (int j = 0; j < 2; ++j)
                s.cellMaterial[s.cellIndex(i, j, 0)] = 1;
        s.dt = 0.95 * yeeStabilityLimit(s.cell, kSpeedOfLight, 2);
        dsp::Waveform pulse;
        pulse.kind = dsp::WaveformKind::GaussianPulse;
        pulse.frequencyHz = kF;
        pulse.fractionalBandwidth = 0.5;
        std::vector<YeeSource> sheet;
        for (int j = 0; j < 2; ++j)
            sheet.push_back({100 * scale, j, 0, 2, 1.0, 0.0, pulse});
        const long steps = std::lround(5.5 * 400 * scale * s.cell / kSpeedOfLight / s.dt);
        const YeeOutput out = mustRun(s, sheet, steps, YeeRecording{}, {{"p", 250 * scale, 0, 0, 2}});
        const auto& x = out.probes[0];
        // Halfway between the incident and the reflected arrivals at the probe.
        const auto split = static_cast<size_t>((150 + 2 * 150) * scale * s.cell / kSpeedOfLight / s.dt);
        double incident = 0.0, reflected = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            (i < split ? incident : reflected) = std::abs(x[i]) > std::abs(i < split ? incident : reflected) ? x[i] : (i < split ? incident : reflected);
        return reflected / incident;
    };
    EXPECT_NEAR(reflection(4.0, 12), -1.0 / 3.0, 0.01);
    // In er = 9 a pulse has only 13 cells per wavelength at 40 per vacuum wavelength:
    // the discrete medium's impedance is off and refinement brings it back.
    const double coarse = reflection(9.0, 12, 40), fine = reflection(9.0, 12, 80);
    EXPECT_LT(std::abs(fine + 0.5), std::abs(coarse + 0.5));
    EXPECT_NEAR(fine, -0.5, 0.01);
    EXPECT_LT(std::abs(reflection(1.0, 12)), 2e-3);  // no interface: only what the absorbing layer returns
}

TEST(EmYee, CavityResonatesAtTheAnalyticalFrequenciesAndConverges)
{
    // A closed PEC box 30 x 18 x 42 mm, at 3 mm and 1.5 mm cells.
    const double a = 0.03, b = 0.018, dz = 0.042;
    const auto modes = rectangularCavityModes(a, b, dz, 4);
    std::vector<double> errors;
    for (const int refine : {1, 2}) {
        YeeSpec s;
        s.cell = 3e-3 / refine;
        s.nx = 10 * refine;
        s.ny = 6 * refine;
        s.nz = 14 * refine;
        s.boundary.fill(YeeBoundary::PEC);
        s.dt = 0.95 * yeeStabilityLimit(s.cell, kSpeedOfLight, 3);
        dsp::Waveform pulse;
        pulse.kind = dsp::WaveformKind::Ricker;
        pulse.frequencyHz = 6e9;
        const long steps = std::lround(30e-9 / s.dt);
        const YeeOutput out = mustRun(s, {{2 * refine, 2 * refine, 3 * refine, 1, 1.0, 0.0, pulse}}, steps, YeeRecording{},
                                      {{"probe", 7 * refine, 3 * refine, 10 * refine, 1}});
        const dsp::Spectrum spectrum = dsp::computeSpectrum(out.probes[0], out.dt, dsp::Window::Hann, 8);
        const auto peaks = dsp::findPeaks(spectrum, 0.05, 1e9, 1.2e10);
        double nearest = 1e300;
        for (const auto& p : peaks)
            if (std::abs(p.frequencyHz - modes[0].frequencyHz) < std::abs(nearest - modes[0].frequencyHz))
                nearest = p.frequencyHz;
        errors.push_back(nearest / modes[0].frequencyHz - 1.0);
        // Closed and lossless: the energy stays constant after the pulse. The
        // estimate sums E and H from times half a step apart, so it ripples
        // with the field; compare averages over windows.
        const auto mean = [&](size_t from, size_t to) {
            double sum = 0.0;
            for (size_t i = from; i < to; ++i)
                sum += out.energy[i];
            return sum / static_cast<double>(to - from);
        };
        const size_t n = out.energy.size();
        EXPECT_NEAR(mean(3 * n / 4, n) / mean(n / 4, n / 2), 1.0, 0.02);
    }
    // Yee dispersion lowers the frequency; halving the cell reduces the error.
    EXPECT_LT(errors[0], 0.0);
    EXPECT_LT(std::abs(errors[1]), std::abs(errors[0]));
    EXPECT_LT(std::abs(errors[1]), 0.005);
}

TEST(EmYee, ShortCurrentElementIn3DRadiatesTheHertzianDipoleField)
{
    YeeSpec s;
    s.cell = wavelength(kF) / 16;
    s.nx = s.ny = s.nz = 32;
    s.pmlCells = 8;
    s.dt = 0.95 * yeeStabilityLimit(s.cell, kSpeedOfLight, 3);
    YeeRecording rec;
    rec.harmonicFrequencyHz = kF;
    rec.harmonicPeriods = 5;
    const long steps = std::lround(16.0 / kF / s.dt);
    const YeeOutput out = mustRun(s, {{16, 16, 16, 2, 1.0, 0.0, continuous()}}, steps, rec);
    // The element is one cell long (I l = I d) and sits at z = 16.5 cells.
    const auto at = [&](int i, int j, int k) {
        return out.harmonic[2][static_cast<size_t>(i) + 33 * (static_cast<size_t>(j) + 33 * static_cast<size_t>(k))];
    };
    for (const int cells : {8, 12}) {
        // In the plane of the element's centre the node average of the two Ez edges is taken at z = 16 and 17;
        // compare against the analytical field averaged the same way.
        const double r = cells * s.cell;
        const auto analytic = [&](double zOffset) {
            return hertzianDipoleField(Vec3(r, 0, zOffset), Complex(s.cell, 0), kF).e.z();
        };
        const Complex expected = 0.5 * (analytic(-0.5 * s.cell) + analytic(0.5 * s.cell));
        const Complex got = 0.5 * (at(16 + cells, 16, 16) + at(16 + cells, 16, 17));
        EXPECT_NEAR(std::abs(got) / std::abs(expected), 1.0, 0.06) << cells;
    }
}
