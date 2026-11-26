// Numerical validation of the analytical acoustic engine. Expected values
// come from closed-form expressions written out here, not from a second
// call into the code under test.
#include "acoustics/FieldGrid.h"
#include "acoustics/Focusing.h"
#include "acoustics/InverseDesign.h"
#include "acoustics/Settings.h"
#include "foundation/Units.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::acoustics;
using units::pi;

namespace {

const double kC = 340.0;      // m/s, a round number so wavelengths are exact
const double kRho = 1.2;      // kg/m^3
const double kF = 34000.0;    // Hz -> wavelength exactly 0.01 m
const double kLambda = kC / kF;

Medium testMedium()
{
    Medium medium;
    medium.name = "Test gas";
    medium.soundSpeed = kC;
    medium.density = kRho;
    return medium;
}

PointSource sourceAt(const Vec3& position, Complex amplitude = Complex(1.0, 0.0))
{
    PointSource source;
    source.position = position;
    source.frequencyHz = kF;
    source.amplitude = amplitude;
    source.radius = 1e-4;
    return source;
}

FieldEvaluator evaluatorFor(const std::vector<PointSource>& sources, PropagationOptions options = {})
{
    return FieldEvaluator(sources, kF, testMedium(), std::make_shared<FreeFieldModel>(options));
}

double wrap(double angle)
{
    return std::remainder(angle, 2.0 * pi);
}

}  // namespace

// --- medium -----------------------------------------------------------------------

TEST(AcousticMedium, IdealGasAirMatchesTheFormula)
{
    const Medium air = airAtTemperature(293.15);
    EXPECT_NEAR(air.soundSpeed, std::sqrt(1.4 * 287.05 * 293.15), 1e-9);  // 343.2 m/s
    EXPECT_NEAR(air.soundSpeed, 343.2, 0.1);
    EXPECT_NEAR(air.density, 101325.0 / (287.05 * 293.15), 1e-9);         // 1.204 kg/m^3
    EXPECT_GT(airAtTemperature(313.15).soundSpeed, air.soundSpeed);
    EXPECT_NE(air.basis.find("Theoretical"), std::string::npos);
    EXPECT_NE(waterNominal().basis.find("not authoritative"), std::string::npos);
}

TEST(AcousticMedium, WavenumberWavelengthImpedanceAndValidation)
{
    const Medium medium = testMedium();
    EXPECT_DOUBLE_EQ(medium.wavelength(kF), 0.01);
    EXPECT_NEAR(medium.wavenumber(kF), 2.0 * pi / 0.01, 1e-9);
    EXPECT_DOUBLE_EQ(medium.impedance(), kRho * kC);
    Medium bad = medium;
    bad.soundSpeed = 0.0;
    EXPECT_FALSE(bad.validate().ok());
    bad = medium;
    bad.attenuation = -1.0;
    EXPECT_FALSE(bad.validate().ok());
}

// --- single source ------------------------------------------------------------------

TEST(AcousticPropagation, SingleSourceSphericalSpreading)
{
    const FieldEvaluator field = evaluatorFor({sourceAt(Vec3::Zero(), Complex(2.5, 0.0))});
    // |P| = A * (1 m) / r in every direction.
    for (const Vec3& direction : {Vec3(1, 0, 0), Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(1, 2, -2).normalized()}) {
        for (const double r : {0.1, 0.5, 1.0, 3.0}) {
            EXPECT_NEAR(std::abs(field.pressure(r * direction)), 2.5 / r, 1e-12 / r);
        }
    }
    // Doubling the distance halves the amplitude (6.02 dB).
    const double near = std::abs(field.pressure(Vec3(0.7, 0, 0)));
    const double far = std::abs(field.pressure(Vec3(1.4, 0, 0)));
    EXPECT_NEAR(near / far, 2.0, 1e-12);
    // At the 1 m reference distance the amplitude is the source amplitude.
    EXPECT_NEAR(std::abs(field.pressure(Vec3(0, 1, 0))), 2.5, 1e-12);
}

TEST(AcousticPropagation, PhaseProgressesAsMinusKr)
{
    const FieldEvaluator field = evaluatorFor({sourceAt(Vec3::Zero())});
    // exp(-j k r): the phase at r is -2 pi r / lambda.
    EXPECT_NEAR(wrap(std::arg(field.pressure(Vec3(0.5, 0, 0)))), wrap(-2.0 * pi * 0.5 / kLambda), 1e-9);
    // A quarter wavelength farther the phase lags by a quarter cycle.
    const double a = std::arg(field.pressure(Vec3(0.2, 0, 0)));
    const double b = std::arg(field.pressure(Vec3(0.2 + kLambda / 4.0, 0, 0)));
    EXPECT_NEAR(wrap(b - a), -pi / 2.0, 1e-9);
    // A whole wavelength farther it is back in phase.
    const double c = std::arg(field.pressure(Vec3(0.2 + kLambda, 0, 0)));
    EXPECT_NEAR(wrap(c - a), 0.0, 1e-9);
}

TEST(AcousticPropagation, SourcePhaseLeadsAndDelayLags)
{
    EmitterSettings settings;
    settings.frequencyHz = kF;
    settings.amplitudePa = 3.0;
    settings.phaseRad = 0.4;
    EXPECT_NEAR(std::abs(complexAmplitude(settings)), 3.0, 1e-12);
    EXPECT_NEAR(std::arg(complexAmplitude(settings)), 0.4, 1e-12);
    // A delay of a quarter period is a phase lag of pi/2 at this frequency.
    settings.phaseRad = 0.0;
    settings.delaySec = 0.25 / kF;
    EXPECT_NEAR(std::arg(complexAmplitude(settings)), -pi / 2.0, 1e-9);
    // The same delay is a different phase at another frequency: not a fixed phase shift.
    settings.frequencyHz = 2.0 * kF;
    EXPECT_NEAR(std::abs(wrap(std::arg(complexAmplitude(settings)))), pi, 1e-9);
}

TEST(AcousticPropagation, AttenuationIsExponentialAndOptional)
{
    Medium lossy = testMedium();
    lossy.attenuation = 0.5;  // Np/m
    const std::vector<PointSource> sources = {sourceAt(Vec3::Zero())};
    const FieldEvaluator with(sources, kF, lossy, std::make_shared<FreeFieldModel>());
    PropagationOptions noLoss;
    noLoss.includeAttenuation = false;
    const FieldEvaluator without(sources, kF, lossy, std::make_shared<FreeFieldModel>(noLoss));
    EXPECT_NEAR(std::abs(with.pressure(Vec3(2, 0, 0))), std::exp(-0.5 * 2.0) / 2.0, 1e-12);
    EXPECT_NEAR(std::abs(without.pressure(Vec3(2, 0, 0))), 0.5, 1e-12);
}

TEST(AcousticPropagation, NoSingularityAtOrNearASource)
{
    PointSource source = sourceAt(Vec3(0.1, 0.2, 0.3));
    source.radius = 0.004;
    const FieldEvaluator field = evaluatorFor({source});
    // Exactly at the source, and anywhere inside its radius: finite, flagged,
    // and equal in magnitude to the value on the limiting sphere.
    for (const Vec3& offset : {Vec3(0, 0, 0), Vec3(1e-9, 0, 0), Vec3(0, 0.002, 0), Vec3(0, 0, -0.0039)}) {
        const FieldSample sample = field.sample(source.position + offset);
        EXPECT_TRUE(std::isfinite(sample.pressure.real()) && std::isfinite(sample.pressure.imag()));
        EXPECT_TRUE(sample.nearSource);
        EXPECT_NEAR(std::abs(sample.pressure), 1.0 / 0.004, 1e-6);
    }
    EXPECT_FALSE(field.sample(source.position + Vec3(0.0041, 0, 0)).nearSource);
    EXPECT_TRUE(field.particleVelocity(source.position).allFinite());
}

// --- interference ---------------------------------------------------------------------

TEST(AcousticInterference, TwoSourcesAddOnTheBisector)
{
    const Vec3 a(-0.05, 0, 0), b(0.05, 0, 0);
    const FieldEvaluator both = evaluatorFor({sourceAt(a), sourceAt(b)});
    // Any point of the plane x = 0 is equidistant: the two waves add exactly.
    for (const Vec3& point : {Vec3(0, 0.3, 0), Vec3(0, 0, 1.0), Vec3(0, 0.2, -0.4)}) {
        const double r = (point - a).norm();
        EXPECT_NEAR(std::abs(both.pressure(point)), 2.0 / r, 1e-10);
    }
}

TEST(AcousticInterference, HalfWavelengthPathDifferenceCancels)
{
    // On the line through both sources, outside them, the path difference is
    // the separation. With a separation of 2.5 wavelengths the waves arrive
    // in antiphase and the amplitude is the difference of the two.
    const double d = 2.5 * kLambda;
    const FieldEvaluator both = evaluatorFor({sourceAt(Vec3(0, 0, 0)), sourceAt(Vec3(d, 0, 0))});
    const double x = 0.4;
    const double r1 = x, r2 = x - d;
    EXPECT_NEAR(std::abs(both.pressure(Vec3(x, 0, 0))), std::abs(1.0 / r1 - 1.0 / r2), 1e-9);
    // With a separation of 3 wavelengths they arrive in phase there.
    const FieldEvaluator inPhase = evaluatorFor({sourceAt(Vec3(0, 0, 0)), sourceAt(Vec3(3.0 * kLambda, 0, 0))});
    EXPECT_NEAR(std::abs(inPhase.pressure(Vec3(x, 0, 0))), 1.0 / x + 1.0 / (x - 3.0 * kLambda), 1e-9);
    // An antiphase source pair cancels exactly on the bisector.
    const FieldEvaluator opposed = evaluatorFor({sourceAt(Vec3(-0.05, 0, 0)), sourceAt(Vec3(0.05, 0, 0), Complex(-1.0, 0.0))});
    EXPECT_NEAR(std::abs(opposed.pressure(Vec3(0, 0.37, 0.1))), 0.0, 1e-12);
}

TEST(AcousticInterference, SourceCountDoesNotRescaleAmplitudes)
{
    // The strength belongs to each source: a second source far away must not
    // change what the first one means.
    const FieldEvaluator one = evaluatorFor({sourceAt(Vec3::Zero())});
    const FieldEvaluator two = evaluatorFor({sourceAt(Vec3::Zero()), sourceAt(Vec3(1e6, 0, 0))});
    EXPECT_NEAR(std::abs(one.pressure(Vec3(0, 0.5, 0))), 2.0, 1e-12);
    EXPECT_NEAR(std::abs(two.pressure(Vec3(0, 0.5, 0))), 2.0, 1e-5);
}

TEST(AcousticInterference, SourcesAtAnotherFrequencyAreLeftOutAndCounted)
{
    PointSource other = sourceAt(Vec3(0.1, 0, 0));
    other.frequencyHz = 2.0 * kF;
    const FieldEvaluator field = evaluatorFor({sourceAt(Vec3::Zero()), other});
    EXPECT_EQ(field.sources().size(), 1u);
    EXPECT_EQ(field.excludedSourceCount(), 1u);
    EXPECT_NEAR(std::abs(field.pressure(Vec3(0, 0, 1))), 1.0, 1e-12);
}

// --- velocity and intensity -------------------------------------------------------------

TEST(AcousticIntensity, SphericalWaveIntensityIsPSquaredOverTwoRhoC)
{
    // For one outgoing spherical wave the active intensity is exactly
    // |P|^2 / (2 rho c), radially outward, at every distance.
    const FieldEvaluator field = evaluatorFor({sourceAt(Vec3::Zero(), Complex(4.0, 0.0))});
    for (const double r : {0.02, 0.3, 2.0}) {
        const Vec3 point = r * Vec3(1, 2, 2).normalized();
        const Vec3 intensity = field.activeIntensity(point);
        const double expected = (4.0 / r) * (4.0 / r) / (2.0 * kRho * kC);
        EXPECT_NEAR(intensity.dot(point.normalized()), expected, 1e-9 * expected);
        EXPECT_NEAR(intensity.cross(point.normalized()).norm(), 0.0, 1e-9 * expected);
        EXPECT_NEAR(planeWaveIntensity(field.pressure(point), testMedium()), expected, 1e-9 * expected);
    }
    EXPECT_TRUE(field.velocityIsExact());
}

TEST(AcousticIntensity, PlaneWaveFormulaIsWrongInAStandingWave)
{
    // Midway between two equal sources the pressure is doubled but the
    // particle velocities cancel: no net energy flows, although the
    // plane-wave formula reports four times one source's intensity.
    const FieldEvaluator field = evaluatorFor({sourceAt(Vec3(-0.2, 0, 0)), sourceAt(Vec3(0.2, 0, 0))});
    const Vec3 midpoint = Vec3::Zero();
    EXPECT_NEAR(field.activeIntensity(midpoint).norm(), 0.0, 1e-12);
    EXPECT_GT(planeWaveIntensity(field.pressure(midpoint), testMedium()), 0.01);
}

TEST(AcousticIntensity, DerivedQuantitiesFollowTheirDefinitions)
{
    const Complex p(3.0, 4.0);  // peak amplitude 5 Pa
    EXPECT_NEAR(rmsPressure(p), 5.0 / std::sqrt(2.0), 1e-12);
    // 5 Pa peak -> 3.5355 Pa rms -> 20 log10(3.5355 / 20e-6) = 104.95 dB re 20 uPa.
    EXPECT_NEAR(soundPressureLevel(p, 20e-6), 20.0 * std::log10(5.0 / std::sqrt(2.0) / 20e-6), 1e-9);
    EXPECT_NEAR(soundPressureLevel(p, 20e-6), 104.95, 0.01);
    // Instantaneous pressure: Re{P exp(j wt)}; at wt = -arg P it peaks.
    EXPECT_NEAR(instantaneousPressure(p, -std::arg(p)), 5.0, 1e-12);
    EXPECT_NEAR(instantaneousPressure(p, 0.0), 3.0, 1e-12);
}

// --- directivity -------------------------------------------------------------------------

TEST(AcousticDirectivity, ModelsMatchTheirFormulas)
{
    Directivity omni;
    EXPECT_DOUBLE_EQ(omni.evaluate(-1.0, 100.0), 1.0);

    Directivity cardioid;
    cardioid.model = DirectivityModel::Cardioid;
    EXPECT_DOUBLE_EQ(cardioid.evaluate(1.0, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(cardioid.evaluate(0.0, 0.0), 0.5);
    EXPECT_DOUBLE_EQ(cardioid.evaluate(-1.0, 0.0), 0.0);
    cardioid.order = 2.0;
    EXPECT_DOUBLE_EQ(cardioid.evaluate(0.0, 0.0), 0.25);

    Directivity piston;
    piston.model = DirectivityModel::BaffledPiston;
    piston.pistonRadius = 0.01;
    const double k = 2.0 * pi / kLambda;
    EXPECT_NEAR(piston.evaluate(1.0, k), 1.0, 1e-12);
    // First null of 2 J1(x)/x at x = 3.8317: sin(theta) = 3.8317 / (k a).
    const double sinNull = 3.8317059702075125 / (k * 0.01);
    EXPECT_NEAR(piston.evaluate(std::sqrt(1.0 - sinNull * sinNull), k), 0.0, 1e-9);
    EXPECT_DOUBLE_EQ(piston.evaluate(-0.2, k), 0.0);  // nothing behind the baffle
}

TEST(AcousticDirectivity, EmitterOrientationSteersTheDirectionalPattern)
{
    Scene scene;
    EmitterSettings settings;
    settings.frequencyHz = kF;
    settings.directivity.model = DirectivityModel::Cardioid;
    auto emitter = makeEmitter("E", settings);
    // Pitch +90 degrees about Y turns the local +Z axis to world +X.
    emitter->state.local.rotation = Vec3(0.0, pi / 2.0, 0.0);
    ASSERT_TRUE(scene.insert(std::move(emitter)).ok());
    const auto sources = collectSources(scene);
    ASSERT_EQ(sources.size(), 1u);
    EXPECT_TRUE(sources[0].forward.isApprox(Vec3::UnitX(), 1e-12));
    const FieldEvaluator field = evaluatorFor(sources);
    EXPECT_NEAR(std::abs(field.pressure(Vec3(1, 0, 0))), 1.0, 1e-12);   // on axis
    EXPECT_NEAR(std::abs(field.pressure(Vec3(-1, 0, 0))), 0.0, 1e-12);  // behind
    EXPECT_NEAR(std::abs(field.pressure(Vec3(0, 0, 1))), 0.5, 1e-12);   // broadside
    EXPECT_FALSE(field.velocityIsExact());
    // With directivity switched off the same emitter is omnidirectional.
    PropagationOptions options;
    options.includeDirectivity = false;
    EXPECT_NEAR(std::abs(evaluatorFor(sources, options).pressure(Vec3(-1, 0, 0))), 1.0, 1e-12);
}

// --- arrays --------------------------------------------------------------------------------

TEST(AcousticArray, LayoutsHaveTheRightCountSpacingAndShape)
{
    ArraySettings s;
    s.layout = ArrayLayout::Linear;
    s.countX = 5;
    s.spacingX = 0.01;
    auto elements = generateElements(s);
    ASSERT_EQ(elements.size(), 5u);
    EXPECT_TRUE(elements[0].localPosition.isApprox(Vec3(-0.02, 0, 0)));
    EXPECT_TRUE(elements[4].localPosition.isApprox(Vec3(0.02, 0, 0)));
    EXPECT_NEAR((elements[1].localPosition - elements[0].localPosition).norm(), 0.01, 1e-15);

    s.layout = ArrayLayout::RectangularPlanar;
    s.countX = 4;
    s.countY = 3;
    s.spacingX = 0.01;
    s.spacingY = 0.02;  // independent horizontal and vertical spacing
    elements = generateElements(s);
    ASSERT_EQ(elements.size(), 12u);
    EXPECT_EQ(s.elementCount(), 12);
    EXPECT_TRUE(elements[0].localPosition.isApprox(Vec3(-0.015, -0.02, 0)));
    EXPECT_TRUE(elements[11].localPosition.isApprox(Vec3(0.015, 0.02, 0)));
    EXPECT_TRUE(elements[5].localPosition.isApprox(Vec3(-0.005, 0.0, 0)));  // index = ix + countX * iy
    Vec3 centroid = Vec3::Zero();
    for (const auto& e : elements)
        centroid += e.localPosition;
    EXPECT_NEAR(centroid.norm(), 0.0, 1e-12);

    s.layout = ArrayLayout::Circular;
    s.countX = 12;
    s.radius = 0.07;
    elements = generateElements(s);
    ASSERT_EQ(elements.size(), 12u);
    for (const auto& e : elements) {
        EXPECT_NEAR(e.localPosition.norm(), 0.07, 1e-12);
        EXPECT_DOUBLE_EQ(e.localPosition.z(), 0.0);
    }

    s.layout = ArrayLayout::Cylindrical;
    s.countY = 3;
    s.spacingY = 0.02;
    s.facing = ElementFacing::Outward;
    elements = generateElements(s);
    ASSERT_EQ(elements.size(), 36u);
    EXPECT_NEAR(elements[0].localPosition.z(), -0.02, 1e-12);
    EXPECT_NEAR(elements[35].localPosition.z(), 0.02, 1e-12);
    EXPECT_TRUE(elements[0].localForward.isApprox(Vec3::UnitX()));  // facing away from the axis

    s.layout = ArrayLayout::Spherical;
    s.countX = 200;
    s.facing = ElementFacing::Inward;
    elements = generateElements(s);
    ASSERT_EQ(elements.size(), 200u);
    centroid = Vec3::Zero();
    for (const auto& e : elements) {
        EXPECT_NEAR(e.localPosition.norm(), 0.07, 1e-12);
        EXPECT_TRUE(e.localForward.isApprox(-e.localPosition.normalized(), 1e-12));
        centroid += e.localPosition;
    }
    EXPECT_LT((centroid / 200.0).norm(), 0.07 * 0.02);  // evenly spread

    s.layout = ArrayLayout::Custom;
    s.customPositions = {Vec3(1, 2, 3), Vec3(-1, 0, 0)};
    elements = generateElements(s);
    ASSERT_EQ(elements.size(), 2u);
    EXPECT_TRUE(elements[0].localPosition.isApprox(Vec3(1, 2, 3)));
}

TEST(AcousticArray, PerElementOverridesSurviveLayoutChanges)
{
    ArraySettings s;
    s.countX = 8;
    s.element.amplitudePa = 2.0;
    s.overrides[2].phaseRad = 1.0;
    s.overrides[6].amplitudePa = 0.5;
    s.overrides[6].active = false;

    auto elements = generateElements(s);
    EXPECT_DOUBLE_EQ(elements[2].settings.phaseRad, 1.0);
    EXPECT_DOUBLE_EQ(elements[2].settings.amplitudePa, 2.0);  // not overridden: follows the default
    EXPECT_DOUBLE_EQ(elements[6].settings.amplitudePa, 0.5);
    EXPECT_FALSE(elements[6].settings.active);

    // Changing an array parameter keeps the overrides and updates positions.
    s.spacingX = 0.02;
    s.element.amplitudePa = 3.0;
    elements = generateElements(s);
    EXPECT_DOUBLE_EQ(elements[2].settings.phaseRad, 1.0);
    EXPECT_DOUBLE_EQ(elements[2].settings.amplitudePa, 3.0);
    EXPECT_DOUBLE_EQ(elements[6].settings.amplitudePa, 0.5);
    EXPECT_NEAR(elements[7].localPosition.x() - elements[6].localPosition.x(), 0.02, 1e-15);

    // Shrinking hides the override of element 6 without destroying it ...
    s.countX = 4;
    EXPECT_EQ(generateElements(s).size(), 4u);
    EXPECT_EQ(s.overrides.count(6), 1u);
    // ... and growing back brings it into effect again.
    s.countX = 8;
    EXPECT_FALSE(generateElements(s)[6].settings.active);
}

TEST(AcousticArray, InactiveElementsAreNotSourcesAndWorldTransformApplies)
{
    Scene scene;
    ArraySettings s;
    s.countX = 3;
    s.spacingX = 0.1;
    s.element.frequencyHz = kF;
    s.overrides[1].active = false;
    auto array = makeArray("Array", s);
    array->state.local.position = Vec3(0, 0, 2.0);
    array->state.local.rotation = Vec3(0, 0, pi / 2.0);  // yaw: the array line turns from X to Y
    const Id id = scene.insert(std::move(array)).value()->id();

    const auto sources = collectSources(scene);
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(sources[0].elementIndex, 0);
    EXPECT_EQ(sources[1].elementIndex, 2);
    EXPECT_TRUE(sources[0].position.isApprox(Vec3(0, -0.1, 2.0), 1e-12));
    EXPECT_TRUE(sources[1].position.isApprox(Vec3(0, 0.1, 2.0), 1e-12));
    EXPECT_EQ(sources[0].objectId, id);
    EXPECT_TRUE(collectSources(scene, {Id("someone-else")}).empty());
}

TEST(AcousticArray, SymmetricArrayHasASymmetricField)
{
    ArraySettings s;
    s.layout = ArrayLayout::RectangularPlanar;
    s.countX = 6;
    s.countY = 4;
    s.spacingX = 0.006;
    s.spacingY = 0.008;
    s.element.frequencyHz = kF;
    Scene scene;
    ASSERT_TRUE(scene.insert(makeArray("A", s)).ok());
    const FieldEvaluator field = evaluatorFor(collectSources(scene));
    for (const Vec3& p : {Vec3(0.03, 0.01, 0.1), Vec3(0.02, -0.04, 0.25)}) {
        const Complex reference = field.pressure(p);
        EXPECT_NEAR(std::abs(field.pressure(Vec3(-p.x(), p.y(), p.z())) - reference), 0.0, 1e-9);
        EXPECT_NEAR(std::abs(field.pressure(Vec3(p.x(), -p.y(), p.z())) - reference), 0.0, 1e-9);
        EXPECT_NEAR(std::abs(field.pressure(Vec3(p.x(), p.y(), -p.z())) - reference), 0.0, 1e-9);  // omnidirectional elements
    }
}

// --- focusing ---------------------------------------------------------------------------------

namespace {
// Three elements on the X axis at -0.03, 0, +0.03 and a target on the Z axis:
// the distances follow from Pythagoras.
ArraySettings threeElementArray()
{
    ArraySettings s;
    s.countX = 3;
    s.spacingX = 0.03;
    s.element.frequencyHz = kF;
    return s;
}
}  // namespace

TEST(AcousticFocus, TimeDelaysAreTheDifferencesInTravelTime)
{
    const auto elements = generateElements(threeElementArray());
    const Vec3 target(0, 0, 0.04);
    const FocusSolution focus =
        computeFocus(elements, Mat4::Identity(), target, testMedium(), kF, FocusControl::TimeDelay);
    ASSERT_EQ(focus.elements.size(), 3u);
    // Outer elements are 0.05 m away (3-4-5 triangle), the centre one 0.04 m.
    EXPECT_NEAR(focus.elements[0].distance, 0.05, 1e-15);
    EXPECT_NEAR(focus.elements[1].distance, 0.04, 1e-15);
    // The farthest elements fire first; the centre one waits 0.01 m / c.
    EXPECT_NEAR(focus.elements[0].delaySec, 0.0, 1e-18);
    EXPECT_NEAR(focus.elements[1].delaySec, 0.01 / kC, 1e-15);
    EXPECT_NEAR(focus.elements[2].delaySec, 0.0, 1e-18);
    for (const auto& e : focus.elements) {
        EXPECT_GE(e.delaySec, 0.0);
        EXPECT_DOUBLE_EQ(e.phaseRad, 0.0);
    }
}

TEST(AcousticFocus, PhasesCancelThePropagationPhase)
{
    const auto elements = generateElements(threeElementArray());
    const Vec3 target(0, 0, 0.04);
    const FocusSolution focus = computeFocus(elements, Mat4::Identity(), target, testMedium(), kF, FocusControl::Phase);
    // phi = k r mod 2 pi, with lambda = 0.01 m: 0.05 m is 5 wavelengths
    // (phase 0) and 0.04 m is 4 wavelengths (phase 0).
    for (const auto& e : focus.elements) {
        EXPECT_NEAR(wrap(e.phaseRad), 0.0, 1e-9);
        EXPECT_DOUBLE_EQ(e.delaySec, 0.0);
    }
    // A target a quarter wavelength closer to the centre element.
    const FocusSolution shifted = computeFocus(elements, Mat4::Identity(), Vec3(0, 0, 0.04 - kLambda / 4.0),
                                               testMedium(), kF, FocusControl::Phase);
    EXPECT_NEAR(wrap(shifted.elements[1].phaseRad), wrap(2.0 * pi * (0.04 - kLambda / 4.0) / kLambda), 1e-9);
    EXPECT_GE(shifted.elements[1].phaseRad, 0.0);
    EXPECT_LT(shifted.elements[1].phaseRad, 2.0 * pi);
}

TEST(AcousticFocus, AppliedPhaseFocusMakesEveryElementArriveInPhase)
{
    ArraySettings s;
    s.layout = ArrayLayout::RectangularPlanar;
    s.countX = 8;
    s.countY = 8;
    s.spacingX = s.spacingY = 0.0043;
    s.element.frequencyHz = kF;
    const Vec3 target(0.012, -0.007, 0.08);  // off axis
    Scene scene;
    const Id id = scene.insert(makeArray("A", s)).value()->id();

    const double unfocused = std::abs(evaluatorFor(collectSources(scene)).pressure(target));
    const FocusSolution focus =
        computeFocus(generateElements(s), Mat4::Identity(), target, testMedium(), kF, FocusControl::Phase);
    const ArraySettings focused = applyFocus(s, focus, Taper::Uniform);
    ObjectState state = scene.find(id)->state;
    state.component = std::make_shared<ArrayComponent>(focused);
    ASSERT_TRUE(scene.setState(id, state).ok());

    // In phase, the amplitudes simply add: sum of A / r_i.
    double expected = 0.0;
    for (const auto& e : focus.elements)
        expected += 1.0 / e.distance;
    const FieldEvaluator field = evaluatorFor(collectSources(scene));
    EXPECT_NEAR(std::abs(field.pressure(target)), expected, 1e-9 * expected);
    // That sum is the largest magnitude these sources can produce at the
    // target (triangle inequality), so focusing can only have raised it.
    EXPECT_GT(std::abs(field.pressure(target)), 1.5 * unfocused);
    // The target is a local maximum along the array axis direction.
    EXPECT_GT(std::abs(field.pressure(target)), std::abs(field.pressure(target + Vec3(0.004, 0, 0))));
    EXPECT_GT(std::abs(field.pressure(target)), std::abs(field.pressure(target - Vec3(0, 0.004, 0))));
}

TEST(AcousticFocus, TimeDelayFocusIsBroadbandAndPhaseFocusIsNot)
{
    ArraySettings s;
    s.countX = 16;
    s.spacingX = 0.005;
    const Vec3 target(0.01, 0, 0.06);
    const auto coherence = [&](const ArraySettings& settings, double frequency) {
        // |sum of unit phasors| / N at the target: 1 means perfectly in phase.
        ArraySettings atFrequency = settings;
        atFrequency.element.frequencyHz = frequency;
        Complex sum(0.0, 0.0);
        const auto elements = generateElements(atFrequency);
        for (const auto& e : elements) {
            const double r = (target - e.localPosition).norm();
            sum += std::polar(1.0, std::arg(complexAmplitude(e.settings)) - 2.0 * pi * frequency * r / kC);
        }
        return std::abs(sum) / static_cast<double>(elements.size());
    };

    s.element.frequencyHz = kF;
    const auto elements = generateElements(s);
    const ArraySettings byDelay = applyFocus(
        s, computeFocus(elements, Mat4::Identity(), target, testMedium(), kF, FocusControl::TimeDelay), Taper::Uniform);
    const ArraySettings byPhase = applyFocus(
        s, computeFocus(elements, Mat4::Identity(), target, testMedium(), kF, FocusControl::Phase), Taper::Uniform);

    EXPECT_NEAR(coherence(byDelay, kF), 1.0, 1e-9);
    EXPECT_NEAR(coherence(byPhase, kF), 1.0, 1e-9);
    // At another frequency the delays still align the arrivals; the fixed phases do not.
    EXPECT_NEAR(coherence(byDelay, 0.6 * kF), 1.0, 1e-9);
    EXPECT_NEAR(coherence(byDelay, 1.7 * kF), 1.0, 1e-9);
    EXPECT_LT(coherence(byPhase, 0.6 * kF), 0.8);
    // The two controls write different settings.
    EXPECT_TRUE(byDelay.overrides.at(0).delaySec.has_value());
    EXPECT_FALSE(byDelay.overrides.at(0).phaseRad.has_value());
    EXPECT_TRUE(byPhase.overrides.at(0).phaseRad.has_value());
    EXPECT_FALSE(byPhase.overrides.at(0).delaySec.has_value());
}

TEST(AcousticFocus, TaperWeightsFallTowardsTheEdgesAndSetAmplitudes)
{
    ArraySettings s;
    s.countX = 9;
    s.spacingX = 0.005;
    s.element.amplitudePa = 2.0;
    const auto elements = generateElements(s);
    const FocusSolution hann =
        computeFocus(elements, Mat4::Identity(), Vec3(0, 0, 0.1), testMedium(), kF, FocusControl::Phase, Taper::Hann);
    EXPECT_NEAR(hann.elements[4].weight, 1.0, 1e-12);  // centre
    for (int i = 0; i < 4; ++i) {
        EXPECT_GT(hann.elements[static_cast<size_t>(i + 1)].weight, hann.elements[static_cast<size_t>(i)].weight);
        EXPECT_NEAR(hann.elements[static_cast<size_t>(i)].weight, hann.elements[static_cast<size_t>(8 - i)].weight, 1e-12);
    }
    EXPECT_GT(hann.elements[0].weight, 0.0);
    const ArraySettings tapered = applyFocus(s, hann, Taper::Hann);
    EXPECT_NEAR(*tapered.overrides.at(4).amplitudePa, 2.0, 1e-12);
    EXPECT_LT(*tapered.overrides.at(0).amplitudePa, 1.0);
    // A uniform taper leaves amplitudes alone.
    const ArraySettings plain = applyFocus(
        s, computeFocus(elements, Mat4::Identity(), Vec3(0, 0, 0.1), testMedium(), kF, FocusControl::Phase), Taper::Uniform);
    EXPECT_FALSE(plain.overrides.at(0).amplitudePa.has_value());
}

// --- inverse design ------------------------------------------------------------------------------

namespace {
std::vector<PointSource> lineOfSources(int count, double spacing)
{
    std::vector<PointSource> sources;
    for (int i = 0; i < count; ++i)
        sources.push_back(sourceAt(Vec3((i - 0.5 * (count - 1)) * spacing, 0, 0)));
    return sources;
}

TargetPoint targetAt(const Vec3& position, Complex desired, double weight = 1.0)
{
    TargetPoint target;
    target.position = position;
    target.desired = desired;
    target.weight = weight;
    return target;
}
}  // namespace

TEST(AcousticInverse, PropagationMatrixHoldsTheUnitSourceResponses)
{
    const auto sources = lineOfSources(3, 0.02);
    const std::vector<TargetPoint> targets = {targetAt(Vec3(0, 0, 0.1), 1.0), targetAt(Vec3(0.03, 0, 0.08), 1.0)};
    const FreeFieldModel model;
    const Eigen::MatrixXcd H = propagationMatrix(sources, targets, testMedium(), model);
    ASSERT_EQ(H.rows(), 2);
    ASSERT_EQ(H.cols(), 3);
    for (int m = 0; m < 2; ++m) {
        for (int i = 0; i < 3; ++i) {
            const double r = (targets[static_cast<size_t>(m)].position - sources[static_cast<size_t>(i)].position).norm();
            const Complex expected = std::polar(1.0 / r, -2.0 * pi * r / kLambda);
            EXPECT_NEAR(std::abs(H(m, i) - expected), 0.0, 1e-10);
        }
    }
}

TEST(AcousticInverse, MoreSourcesThanTargetsMeetsEveryTargetWithTheMinimumNormSolution)
{
    const auto sources = lineOfSources(16, 0.005);
    const std::vector<TargetPoint> targets = {targetAt(Vec3(-0.02, 0, 0.08), Complex(30.0, 0.0)),
                                              targetAt(Vec3(0.025, 0, 0.09), std::polar(20.0, 1.0)),
                                              targetAt(Vec3(0.0, 0, 0.07), Complex(0.0, 0.0))};  // a null
    const FreeFieldModel model;
    InverseOptions options;
    options.regularization = 0.0;
    const InverseSolution solution = solveInverse(sources, targets, testMedium(), model, options);
    ASSERT_TRUE(solution.solved) << solution.message;
    EXPECT_EQ(solution.rank, 3);
    EXPECT_TRUE(solution.accurate);
    EXPECT_LT(solution.relativeResidual, 1e-10);
    for (size_t m = 0; m < targets.size(); ++m)
        EXPECT_NEAR(std::abs(solution.achieved(static_cast<Eigen::Index>(m)) - targets[m].desired), 0.0, 1e-8);

    // Independent check 1: the field of the solved strengths, evaluated by
    // the field evaluator, hits the targets.
    std::vector<PointSource> driven = sources;
    for (size_t i = 0; i < driven.size(); ++i)
        driven[i].amplitude = solution.weights(static_cast<Eigen::Index>(i));
    const FieldEvaluator field = evaluatorFor(driven);
    EXPECT_NEAR(std::abs(field.pressure(targets[0].position)), 30.0, 1e-7);
    EXPECT_NEAR(std::abs(field.pressure(targets[2].position)), 0.0, 1e-7);
    // Independent check 2: the minimum-norm solution lies in the row space,
    // w = H^H y, so it is unchanged by projecting onto that space.
    const Eigen::MatrixXcd H = propagationMatrix(sources, targets, testMedium(), model);
    const Eigen::VectorXcd y = (H * H.adjoint()).ldlt().solve(Eigen::VectorXcd(H * solution.weights));
    EXPECT_NEAR((H.adjoint() * y - solution.weights).norm(), 0.0, 1e-8 * solution.weights.norm());
}

TEST(AcousticInverse, MoreTargetsThanSourcesGivesTheLeastSquaresSolution)
{
    const auto sources = lineOfSources(2, 0.01);
    std::vector<TargetPoint> targets;
    for (int m = 0; m < 6; ++m)
        targets.push_back(targetAt(Vec3(-0.03 + 0.012 * m, 0, 0.05), Complex(10.0, 0.0)));
    const FreeFieldModel model;
    InverseOptions options;
    options.regularization = 0.0;
    const InverseSolution solution = solveInverse(sources, targets, testMedium(), model, options);
    ASSERT_TRUE(solution.solved);
    EXPECT_FALSE(solution.accurate);  // six targets cannot be met with two sources
    EXPECT_GT(solution.relativeResidual, 0.05);
    bool told = false;
    for (const std::string& note : solution.notes)
        told = told || note.find("NOT achieved accurately") != std::string::npos;
    EXPECT_TRUE(told);
    // The least-squares residual is orthogonal to the columns of H.
    const Eigen::MatrixXcd H = propagationMatrix(sources, targets, testMedium(), model);
    Eigen::VectorXcd b(6);
    for (int m = 0; m < 6; ++m)
        b(m) = targets[static_cast<size_t>(m)].desired;
    EXPECT_NEAR((H.adjoint() * (H * solution.weights - b)).norm(), 0.0, 1e-7 * (H.adjoint() * b).norm());
}

TEST(AcousticInverse, RegularizationTradesSourceStrengthForAccuracy)
{
    // Two targets a tenth of a wavelength apart with very different wanted
    // values: an ill-conditioned request that needs large strengths.
    const auto sources = lineOfSources(8, 0.005);
    const std::vector<TargetPoint> targets = {targetAt(Vec3(0, 0, 0.08), Complex(10.0, 0.0)),
                                              targetAt(Vec3(kLambda / 10.0, 0, 0.08), Complex(-10.0, 0.0))};
    const FreeFieldModel model;
    double previousNorm = std::numeric_limits<double>::infinity();
    double previousResidual = -1.0;
    for (const double lambda : {0.0, 1e-6, 1e-4, 1e-2, 1.0}) {
        InverseOptions options;
        options.regularization = lambda;
        const InverseSolution solution = solveInverse(sources, targets, testMedium(), model, options);
        ASSERT_TRUE(solution.solved);
        EXPECT_LE(solution.weights.norm(), previousNorm * (1.0 + 1e-9)) << lambda;
        EXPECT_GE(solution.residualNorm, previousResidual - 1e-9) << lambda;
        previousNorm = solution.weights.norm();
        previousResidual = solution.residualNorm;
    }
    InverseOptions none;
    none.regularization = 0.0;
    EXPECT_GT(solveInverse(sources, targets, testMedium(), model, none).conditionNumber, 10.0);
}

TEST(AcousticInverse, AmplitudeLimitClipsAndSaysSo)
{
    const auto sources = lineOfSources(4, 0.005);
    const std::vector<TargetPoint> targets = {targetAt(Vec3(0, 0, 0.1), Complex(100.0, 0.0))};
    const FreeFieldModel model;
    InverseOptions options;
    const InverseSolution free = solveInverse(sources, targets, testMedium(), model, options);
    ASSERT_TRUE(free.solved);
    EXPECT_FALSE(free.amplitudeLimited);
    options.maxAmplitudePa = 0.5 * free.weights.cwiseAbs().maxCoeff();
    const InverseSolution limited = solveInverse(sources, targets, testMedium(), model, options);
    EXPECT_TRUE(limited.amplitudeLimited);
    EXPECT_LE(limited.weights.cwiseAbs().maxCoeff(), options.maxAmplitudePa * (1.0 + 1e-12));
    EXPECT_GT(limited.relativeResidual, free.relativeResidual);
    EXPECT_FALSE(limited.notes.empty());
}

TEST(AcousticInverse, TargetWeightsFavourTheHeavierTarget)
{
    const auto sources = lineOfSources(1, 0.01);  // one source cannot meet two different targets
    const FreeFieldModel model;
    const auto solve = [&](double w0, double w1) {
        const std::vector<TargetPoint> targets = {targetAt(Vec3(0, 0, 0.1), Complex(10.0, 0.0), w0),
                                                  targetAt(Vec3(0, 0, 0.1 + kLambda / 2.0), Complex(10.0, 0.0), w1)};
        InverseOptions options;
        options.regularization = 0.0;
        return solveInverse(sources, targets, testMedium(), model, options);
    };
    const InverseSolution favourFirst = solve(100.0, 1.0);
    const InverseSolution favourSecond = solve(1.0, 100.0);
    EXPECT_LT(std::abs(favourFirst.achieved(0) - Complex(10.0, 0.0)), std::abs(favourFirst.achieved(1) - Complex(10.0, 0.0)));
    EXPECT_GT(std::abs(favourSecond.achieved(0) - Complex(10.0, 0.0)), std::abs(favourSecond.achieved(1) - Complex(10.0, 0.0)));
}

TEST(AcousticInverse, RefusesProblemsItCannotSolve)
{
    const FreeFieldModel model;
    const std::vector<TargetPoint> targets = {targetAt(Vec3(0, 0, 0.1), 1.0)};
    EXPECT_FALSE(solveInverse({}, targets, testMedium(), model).solved);
    EXPECT_FALSE(solveInverse(lineOfSources(2, 0.01), {}, testMedium(), model).solved);
    auto mixed = lineOfSources(2, 0.01);
    mixed[1].frequencyHz = 2.0 * kF;
    const InverseSolution solution = solveInverse(mixed, targets, testMedium(), model);
    EXPECT_FALSE(solution.solved);
    EXPECT_NE(solution.message.find("different frequencies"), std::string::npos);
}

TEST(AcousticInverse, SceneTargetsAreCollectedAndUnsupportedKindsReported)
{
    Scene scene;
    TargetSettings point;
    point.desiredAmplitudePa = 5.0;
    point.desiredPhaseRad = pi / 2.0;
    point.weight = 2.0;
    auto a = makeTarget("Focus", point);
    a->state.local.position = Vec3(0.1, 0.2, 0.3);
    ASSERT_TRUE(scene.insert(std::move(a)).ok());
    TargetSettings null;
    null.suppress = true;
    ASSERT_TRUE(scene.insert(makeTarget("Quiet", null)).ok());
    TargetSettings plane;
    plane.kind = TargetKind::Plane;
    ASSERT_TRUE(scene.insert(makeTarget("Sheet", plane)).ok());

    const TargetCollection collection = collectTargets(scene);
    ASSERT_EQ(collection.points.size(), 2u);
    EXPECT_TRUE(collection.points[0].position.isApprox(Vec3(0.1, 0.2, 0.3)));
    EXPECT_NEAR(std::abs(collection.points[0].desired - Complex(0.0, 5.0)), 0.0, 1e-12);
    EXPECT_DOUBLE_EQ(collection.points[0].weight, 2.0);
    EXPECT_EQ(collection.points[1].desired, Complex(0.0, 0.0));
    ASSERT_EQ(collection.unsupported.size(), 1u);
    EXPECT_EQ(collection.unsupported[0], "Sheet");
}

TEST(AcousticInverse, SolvedStrengthsAreAppliedToTheArrayAndReproduceTheTargets)
{
    Scene scene;
    ArraySettings s;
    s.layout = ArrayLayout::RectangularPlanar;
    s.countX = 6;
    s.countY = 6;
    s.spacingX = s.spacingY = 0.005;
    s.element.frequencyHz = kF;
    const Id arrayId = scene.insert(makeArray("Array", s)).value()->id();
    const std::vector<TargetPoint> targets = {targetAt(Vec3(-0.015, 0.0, 0.07), Complex(40.0, 0.0)),
                                              targetAt(Vec3(0.02, 0.01, 0.09), std::polar(25.0, -0.7))};
    const FreeFieldModel model;
    const auto sources = collectSources(scene);
    const InverseSolution solution = solveInverse(sources, targets, testMedium(), model);
    ASSERT_TRUE(solution.solved);

    ObjectState state = scene.find(arrayId)->state;
    state.component = std::make_shared<ArrayComponent>(applyWeights(s, arrayId, sources, solution.weights));
    ASSERT_TRUE(scene.setState(arrayId, state).ok());
    const FieldEvaluator field = evaluatorFor(collectSources(scene));
    EXPECT_NEAR(std::abs(field.pressure(targets[0].position) - targets[0].desired), 0.0, 1e-3);
    EXPECT_NEAR(std::abs(field.pressure(targets[1].position) - targets[1].desired), 0.0, 1e-3);
    const auto* applied = componentOf<ArrayComponent>(*scene.find(arrayId));
    EXPECT_GE(*applied->settings.overrides.at(0).phaseRad, 0.0);
    EXPECT_LT(*applied->settings.overrides.at(0).phaseRad, 2.0 * pi);
}

// --- regions, grids, jobs ---------------------------------------------------------------------------

TEST(AcousticRegion, GridDimensionsAndPositions)
{
    RegionSettings region;
    region.kind = RegionKind::PlaneXZ;
    region.size = Vec3(0.2, 0.5, 0.1);
    region.countU = 21;
    region.countV = 11;
    EXPECT_EQ(region.pointCount(), 231u);
    EXPECT_TRUE(region.localPoint(0, 0, 0).isApprox(Vec3(-0.1, 0, -0.05)));
    EXPECT_TRUE(region.localPoint(20, 10, 0).isApprox(Vec3(0.1, 0, 0.05)));
    EXPECT_TRUE(region.localPoint(10, 5, 0).isApprox(Vec3::Zero()));

    region.kind = RegionKind::PlaneXY;
    EXPECT_TRUE(region.localPoint(20, 10, 0).isApprox(Vec3(0.1, 0.25, 0)));
    region.kind = RegionKind::PlaneYZ;
    EXPECT_TRUE(region.localPoint(20, 10, 0).isApprox(Vec3(0, 0.25, 0.05)));

    region.kind = RegionKind::Volume;
    region.countW = 5;
    EXPECT_EQ(region.pointCount(), 21u * 11u * 5u);
    EXPECT_TRUE(region.localPoint(20, 10, 4).isApprox(Vec3(0.1, 0.25, 0.05)));

    // A translated, rotated region object gives a grid in world space.
    Mat4 world = Transform{Vec3(1, 2, 3), Vec3(0, 0, pi / 2.0), Vec3::Ones()}.matrix();
    const GridFrame frame = gridOf(region, world);
    EXPECT_EQ(frame.nu, 21);
    EXPECT_EQ(frame.nw, 5);
    EXPECT_EQ(frame.pointCount(), region.pointCount());
    EXPECT_TRUE(frame.point(10, 5, 2).isApprox(Vec3(1, 2, 3), 1e-12));               // the centre
    EXPECT_TRUE(frame.point(20, 5, 2).isApprox(Vec3(1, 2.1, 3), 1e-12));              // local +X is world +Y
    EXPECT_NEAR(frame.maxSpacing(), 0.5 / 10.0, 1e-12);

    RegionSettings bad = region;
    bad.countU = 1;
    EXPECT_FALSE(bad.validate().ok());
    bad = region;
    bad.size = Vec3(0.1, 0, 0.1);
    EXPECT_FALSE(bad.validate().ok());
}

TEST(AcousticRegion, PreflightEstimatesAndRefusesExcessiveGrids)
{
    GridFrame frame;
    frame.axisU = Vec3(0.1, 0, 0);
    frame.axisV = Vec3(0, 0.1, 0);
    frame.nu = frame.nv = 101;
    const FieldEstimate fine = estimateField(frame, 64, testMedium(), kF);
    EXPECT_TRUE(fine.allowed);
    EXPECT_EQ(fine.points, 10201u);
    EXPECT_EQ(fine.bytes, 10201u * 9u);
    EXPECT_DOUBLE_EQ(fine.evaluations, 10201.0 * 64.0);
    EXPECT_NEAR(fine.samplesPerWavelength, 0.01 / 0.001, 1e-9);
    EXPECT_TRUE(fine.warnings.empty());

    // A grid far too coarse for the wavelength is allowed but warned about.
    frame.nu = frame.nv = 6;
    const FieldEstimate coarse = estimateField(frame, 64, testMedium(), kF);
    EXPECT_TRUE(coarse.allowed);
    ASSERT_FALSE(coarse.warnings.empty());
    EXPECT_NE(coarse.warnings[0].find("aliased"), std::string::npos);

    // An enormous volume is refused before anything is allocated.
    frame.axisW = Vec3(0, 0, 0.1);
    frame.nu = frame.nv = frame.nw = 2000;
    const FieldEstimate huge = estimateField(frame, 64, testMedium(), kF);
    EXPECT_FALSE(huge.allowed);
    EXPECT_NE(huge.refusal.find("Reduce the resolution"), std::string::npos);
    // The limit is configurable.
    frame.nu = frame.nv = frame.nw = 50;
    EXPECT_FALSE(estimateField(frame, 1, testMedium(), kF, 1024 * 1024).allowed);
    EXPECT_FALSE(estimateField(frame, 0, testMedium(), kF).warnings.empty());  // no sources
}

TEST(AcousticField, ComputedGridMatchesPointEvaluationAndHasStatistics)
{
    const std::vector<PointSource> sources = {sourceAt(Vec3(0, 0, 0)), sourceAt(Vec3(0.02, 0, 0))};
    const FieldEvaluator field = evaluatorFor(sources);
    GridFrame frame;
    frame.origin = Vec3(-0.05, 0, 0.01);
    frame.axisU = Vec3(0.1, 0, 0);
    frame.axisV = Vec3(0, 0, 0.1);
    frame.nu = 41;
    frame.nv = 31;
    const auto result = computeField("test", field, frame);
    ASSERT_EQ(result->pressure.size(), 41u * 31u);
    EXPECT_EQ(result->kind(), "acoustic.field");
    for (const auto& [iu, iv] : {std::pair{0, 0}, {40, 30}, {17, 9}}) {
        const Complex expected = field.pressure(frame.point(iu, iv, 0));
        const std::complex<float> stored = result->pressure[frame.index(iu, iv, 0)];
        EXPECT_NEAR(stored.real(), expected.real(), 1e-4 * std::abs(expected) + 1e-4);
        EXPECT_NEAR(stored.imag(), expected.imag(), 1e-4 * std::abs(expected) + 1e-4);
    }
    // Statistics: the maximum is the largest magnitude on the grid.
    float largest = 0.0f;
    for (const auto& p : result->pressure)
        largest = std::max(largest, std::abs(p));
    EXPECT_NEAR(result->statistics.maxMagnitude, largest, 1e-4 * largest);
    EXPECT_GT(result->statistics.meanMagnitude, 0.0);
    EXPECT_LT(result->statistics.meanMagnitude, result->statistics.maxMagnitude);
    // Reproducibility metadata.
    EXPECT_EQ(result->metadata.solverId, "acoustic.freefield.analytical");
    EXPECT_EQ(result->metadata.domain, PhysicsDomain::Acoustic);
    EXPECT_FALSE(result->metadata.softwareVersion.empty());
    EXPECT_FALSE(result->metadata.startedUtc.empty());
    EXPECT_FALSE(result->assumptions.empty());
    EXPECT_NE(result->assumptions[1].find("do NOT influence"), std::string::npos);
    EXPECT_EQ(result->sources.size(), 2u);
    EXPECT_DOUBLE_EQ(result->frequencyHz, kF);
}

TEST(AcousticField, SamplesInsideASourceAreFlaggedAndKeptOutOfTheStatistics)
{
    PointSource source = sourceAt(Vec3::Zero());
    source.radius = 0.01;
    const FieldEvaluator field = evaluatorFor({source});
    GridFrame frame;
    frame.origin = Vec3(-0.05, -0.05, 0);
    frame.axisU = Vec3(0.1, 0, 0);
    frame.axisV = Vec3(0, 0.1, 0);
    frame.nu = frame.nv = 21;  // 5 mm spacing: the centre sample is at the source
    const auto result = computeField("near", field, frame);
    EXPECT_EQ(result->nearSource[frame.index(10, 10, 0)], 1);
    EXPECT_GT(result->statistics.nearSourceSamples, 0u);
    // The reported maximum is outside the source: at most A / radius.
    EXPECT_LE(result->statistics.maxMagnitude, 1.0 / 0.01 + 1e-6);
    EXPECT_GE((result->statistics.maxPoint - source.position).norm(), 0.01 - 1e-9);
}

TEST(AcousticField, JobRunsInTheBackgroundReportsProgressAndGivesTheSameField)
{
    const auto evaluator = std::make_shared<const FieldEvaluator>(evaluatorFor(lineOfSources(8, 0.005)));
    GridFrame frame;
    frame.origin = Vec3(-0.04, 0, 0.005);
    frame.axisU = Vec3(0.08, 0, 0);
    frame.axisV = Vec3(0, 0, 0.1);
    frame.axisW = Vec3(0, 0.02, 0);
    frame.nu = 33;
    frame.nv = 41;
    frame.nw = 5;
    auto job = std::make_shared<FieldJob>("volume", evaluator, frame, ResultMetadata{}, 4);
    JobManager manager;
    double progress = 0.0;
    std::mutex mutex;
    manager.addListener([&](const JobInfo& info) {
        std::lock_guard lock(mutex);
        progress = std::max(progress, info.progress);
    });
    EXPECT_EQ(job->result(), nullptr);
    const Id id = manager.submit(job);
    const auto info = manager.wait(id, std::chrono::seconds(30));
    ASSERT_TRUE(info.has_value());
    ASSERT_EQ(info->state, JobState::Completed) << info->message;
    {
        std::lock_guard lock(mutex);
        EXPECT_DOUBLE_EQ(progress, 1.0);
    }
    const auto threaded = job->result();
    ASSERT_NE(threaded, nullptr);
    const auto direct = computeField("volume", *evaluator, frame);
    ASSERT_EQ(threaded->pressure.size(), direct->pressure.size());
    EXPECT_EQ(threaded->pressure, direct->pressure);  // threading does not change a single value
    EXPECT_GT(threaded->metadata.elapsedSeconds, 0.0);
}

TEST(AcousticField, JobCanBeCancelledAndThenHasNoResult)
{
    // A grid large enough to still be running when the cancellation arrives.
    const auto evaluator = std::make_shared<const FieldEvaluator>(evaluatorFor(lineOfSources(256, 0.001)));
    GridFrame frame;
    frame.axisU = Vec3(0.1, 0, 0);
    frame.axisV = Vec3(0, 0, 0.1);
    frame.nu = 1500;
    frame.nv = 1500;
    auto job = std::make_shared<FieldJob>("big", evaluator, frame, ResultMetadata{}, 2);
    JobManager manager;
    const Id id = manager.submit(job);
    while (manager.info(id)->state == JobState::Queued)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(manager.cancel(id));
    const auto info = manager.wait(id, std::chrono::seconds(30));
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->state, JobState::Cancelled);
    EXPECT_EQ(job->result(), nullptr);
}

TEST(AcousticField, AResultIsASnapshotThatLaterSceneEditsDoNotChange)
{
    Scene scene;
    EmitterSettings settings;
    settings.frequencyHz = kF;
    const Id id = scene.insert(makeEmitter("E", settings)).value()->id();
    GridFrame frame;
    frame.origin = Vec3(0.1, 0, 0);
    frame.axisU = Vec3(0.1, 0, 0);
    frame.axisV = Vec3(0, 0.1, 0);
    frame.nu = frame.nv = 5;
    const auto before = computeField("snapshot", evaluatorFor(collectSources(scene)), frame);
    const auto copy = before->pressure;

    ObjectState state = scene.find(id)->state;
    settings.amplitudePa = 50.0;
    state.component = std::make_shared<EmitterComponent>(settings);
    state.local.position = Vec3(5, 5, 5);
    ASSERT_TRUE(scene.setState(id, state).ok());

    EXPECT_EQ(before->pressure, copy);
    EXPECT_NEAR(std::abs(before->sources[0].amplitude), 1.0, 1e-12);
    EXPECT_TRUE(before->sources[0].position.isApprox(Vec3::Zero()));
}

// --- probes and exports -------------------------------------------------------------------------------

TEST(AcousticProbe, ReportsFieldQuantitiesAndExports)
{
    Scene scene;
    EmitterSettings settings;
    settings.frequencyHz = kF;
    settings.amplitudePa = 2.0;
    ASSERT_TRUE(scene.insert(makeEmitter("E", settings)).ok());
    auto probe = makeProbe("Probe, A");
    probe->state.local.position = Vec3(0, 0, 0.5);
    ASSERT_TRUE(scene.insert(std::move(probe)).ok());
    TargetSettings wanted;
    wanted.desiredAmplitudePa = 4.0;
    auto target = makeTarget("T", wanted);
    target->state.local.position = Vec3(0.25, 0, 0);
    ASSERT_TRUE(scene.insert(std::move(target)).ok());

    const FieldEvaluator field = evaluatorFor(collectSources(scene));
    EXPECT_EQ(reportPoints(scene, field, false).size(), 1u);
    const auto reports = reportPoints(scene, field, true);
    ASSERT_EQ(reports.size(), 2u);
    EXPECT_NEAR(std::abs(reports[0].pressure), 4.0, 1e-12);                 // 2 Pa at 1 m -> 4 Pa at 0.5 m
    EXPECT_NEAR(reports[0].rms, 4.0 / std::sqrt(2.0), 1e-12);
    EXPECT_DOUBLE_EQ(reports[0].referencePressure, 20e-6);
    EXPECT_NEAR(reports[0].activeIntensity.z(), 16.0 / (2.0 * kRho * kC), 1e-9);
    EXPECT_FALSE(reports[0].desired.has_value());
    EXPECT_NEAR(std::abs(reports[1].pressure), 8.0, 1e-12);
    ASSERT_TRUE(reports[1].desired.has_value());
    EXPECT_NEAR(std::abs(*reports[1].desired), 4.0, 1e-12);

    const std::string csv = pointReportsToCsv(reports);
    EXPECT_EQ(csv.substr(0, csv.find('\n')).find("name,x_m,y_m,z_m,pressure_real_Pa"), 0u);
    EXPECT_EQ(std::count(csv.begin(), csv.end(), '\n'), 3);
    EXPECT_NE(csv.find("Probe  A,0,0,0.5,"), std::string::npos);  // the comma in the name cannot break a column

    const nlohmann::json document = pointReportsToJson(reports, field);
    EXPECT_EQ(document["format"], "resonant-point-report");
    EXPECT_EQ(document["points"].size(), 2u);
    EXPECT_DOUBLE_EQ(document["frequencyHz"].get<double>(), kF);
    EXPECT_NEAR(document["points"][0]["magnitudePa"].get<double>(), 4.0, 1e-12);
    EXPECT_EQ(document["amplitudes"], "peak");
    EXPECT_TRUE(document["points"][1].contains("desiredPressure"));
}

// --- persistence -------------------------------------------------------------------------------------------

TEST(AcousticPersistence, ComponentsAndSettingsRoundTripThroughTheProjectFile)
{
    auto project = Project::createNew("Acoustic");
    EmitterSettings emitter;
    emitter.frequencyHz = 25000.0;
    emitter.amplitudePa = 3.5;
    emitter.phaseRad = 0.25;
    emitter.delaySec = 1e-5;
    emitter.directivity.model = DirectivityModel::BaffledPiston;
    emitter.directivity.pistonRadius = 0.008;
    emitter.physicalTransducer = true;
    emitter.transducerProfile = "data sheet XYZ";
    ASSERT_TRUE(project->scene.insert(makeEmitter("Emitter", emitter)).ok());

    ArraySettings array;
    array.layout = ArrayLayout::Cylindrical;
    array.countX = 10;
    array.countY = 3;
    array.radius = 0.04;
    array.facing = ElementFacing::Inward;
    array.customPositions = {Vec3(1, 2, 3)};
    array.overrides[4].phaseRad = 1.5;
    array.overrides[4].active = false;
    array.overrides[7].amplitudePa = 0.25;
    array.overrides[7].delaySec = 2e-6;
    array.overrides[7].frequencyHz = 30000.0;
    ASSERT_TRUE(project->scene.insert(makeArray("Array", array)).ok());

    TargetSettings target;
    target.desiredAmplitudePa = 12.0;
    target.desiredPhaseRad = -0.5;
    target.weight = 3.0;
    target.suppress = true;
    ASSERT_TRUE(project->scene.insert(makeTarget("Target", target)).ok());
    ASSERT_TRUE(project->scene.insert(makeProbe("Probe")).ok());
    RegionSettings region;
    region.kind = RegionKind::Volume;
    region.size = Vec3(0.1, 0.2, 0.3);
    region.countU = 11;
    region.countV = 21;
    region.countW = 31;
    ASSERT_TRUE(project->scene.insert(makeRegion("Region", region)).ok());

    AcousticSettings settings;
    settings.medium = waterNominal();
    settings.medium.attenuation = 0.02;
    settings.frequencyHz = 25000.0;
    settings.propagation.includeDirectivity = false;
    settings.propagation.minimumDistance = 2e-3;
    settings.memoryLimitBytes = 64ull * 1024 * 1024;
    storeSettings(*project, settings);

    auto loaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    const Project& copy = *loaded.value().project;
    const auto& objects = copy.scene.root().children();
    ASSERT_EQ(objects.size(), 5u);
    EXPECT_EQ(objects[0]->type(), "acoustic.emitter");
    EXPECT_EQ(componentOf<EmitterComponent>(*objects[0])->settings, emitter);
    EXPECT_EQ(componentOf<ArrayComponent>(*objects[1])->settings, array);
    EXPECT_EQ(componentOf<TargetComponent>(*objects[2])->settings, target);
    EXPECT_NE(componentOf<ProbeComponent>(*objects[3]), nullptr);
    EXPECT_EQ(componentOf<RegionComponent>(*objects[4])->settings, region);
    EXPECT_EQ(settingsOf(copy), settings);
    EXPECT_EQ(serializeProject(copy), serializeProject(*project));

    // A project without acoustic settings gets the defaults (air at 20 C).
    EXPECT_EQ(settingsOf(*Project::createNew()).medium.name, "Air");
}

TEST(AcousticPersistence, InvalidComponentDataIsRejectedWhenLoading)
{
    auto project = Project::createNew();
    ASSERT_TRUE(project->scene.insert(makeEmitter("E")).ok());
    ASSERT_TRUE(project->scene.insert(makeRegion("R")).ok());
    const nlohmann::json good = serializeProject(*project);

    nlohmann::json bad = good;
    bad["scene"]["objects"][0]["component"]["data"]["frequencyHz"] = -5.0;
    auto loaded = deserializeProject(bad);
    EXPECT_FALSE(loaded.ok());
    EXPECT_NE(loaded.message().find("frequency"), std::string::npos);

    bad = good;
    bad["scene"]["objects"][0]["component"]["data"]["directivity"]["model"] = "laser";
    EXPECT_FALSE(deserializeProject(bad).ok());

    bad = good;
    bad["scene"]["objects"][1]["component"]["data"]["countU"] = 1;
    EXPECT_FALSE(deserializeProject(bad).ok());

    bad = good;
    bad["scene"]["objects"][0]["component"]["data"].erase("amplitudePa");
    EXPECT_FALSE(deserializeProject(bad).ok());
}

TEST(AcousticPersistence, SettingsValidationAndRunRecords)
{
    AcousticSettings settings;
    EXPECT_TRUE(settings.validate().ok());
    settings.frequencyHz = 0.0;
    EXPECT_FALSE(settings.validate().ok());
    EXPECT_FALSE(settingsFromJson(nlohmann::json::array()).ok());
    EXPECT_FALSE(settingsFromJson({{"medium", {{"name", "x"}, {"density", -1.0}, {"soundSpeed", 300.0}}}}).ok());

    GridFrame frame;
    frame.origin = Vec3(0.1, 0, 0);
    const auto result = computeField("run 1", evaluatorFor({sourceAt(Vec3::Zero())}), frame);
    const nlohmann::json record = runRecord(*result);
    EXPECT_EQ(record["name"], "run 1");
    EXPECT_EQ(record["kind"], "acoustic.field");
    EXPECT_EQ(record["metadata"]["solver"], "acoustic.freefield.analytical");
    EXPECT_FALSE(record["metadata"]["assumptions"].empty());

    // Storing settings keeps other data under the same key (the run history).
    auto project = Project::createNew();
    project->extensions[kSettingsKey] = {{"runs", nlohmann::json::array({record})}};
    storeSettings(*project, AcousticSettings());
    EXPECT_EQ(project->extensions[kSettingsKey]["runs"].size(), 1u);
    EXPECT_TRUE(project->extensions[kSettingsKey].contains("medium"));
}
