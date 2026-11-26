// Validation of wave-manipulator geometry, the analytical estimates and the
// reflector field model against closed-form references.
#include "manipulators/Analytics.h"
#include "manipulators/GeometryOptimization.h"
#include "manipulators/ReflectorModel.h"

#include "foundation/Units.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::manipulators;
using units::pi;

namespace {

const double kC = 340.0;
const double kF = 34000.0;       // wavelength exactly 0.01 m
const double kLambda = kC / kF;

Medium gas()
{
    Medium medium;
    medium.name = "Test gas";
    medium.soundSpeed = kC;
    medium.density = 1.2;
    return medium;
}

Geometry make(const char* kind, std::initializer_list<std::pair<const char*, double>> parameters = {})
{
    Geometry geometry = Geometry::create(kind).take();
    for (const auto& [key, value] : parameters)
        EXPECT_TRUE(geometry.setParameter(key, value).ok()) << kind << "." << key;
    return geometry;
}

PointSource sourceAt(const Vec3& position)
{
    PointSource source;
    source.position = position;
    source.frequencyHz = kF;
    source.radius = 1e-4;
    return source;
}

// Shortest distance from a point to the line through `origin` along `direction`.
double distanceToLine(const Vec3& point, const Vec3& origin, const Vec3& direction)
{
    return (point - origin).cross(direction.normalized()).norm();
}

// A scene holding one reflector manipulator at the origin, unrotated.
struct ReflectorScene {
    Scene scene;
    MaterialLibrary materials = createExampleMaterialLibrary();
    Id id;
    explicit ReflectorScene(const Geometry& geometry, BoundaryCondition boundary = BoundaryCondition::Rigid)
    {
        auto object = makeManipulator("Reflector", geometry.kindId()).take();
        object->state.geometry = geometry;
        ManipulatorSettings settings;
        settings.boundary = boundary;
        object->state.component = std::make_shared<ManipulatorComponent>(settings);
        id = scene.insert(std::move(object)).value()->id();
    }
    std::shared_ptr<const ReflectorModel> model(double elementsPerWavelength = 6.0)
    {
        ReflectorOptions options;
        options.elementsPerWavelength = elementsPerWavelength;
        return std::make_shared<const ReflectorModel>(PropagationOptions{},
                                                      collectReflectors(scene, materials, kLambda, options).surfaces);
    }
};

}  // namespace

// --- geometry ----------------------------------------------------------------------

TEST(ManipulatorGeometry, KindsAreRegisteredWithCategoriesAndValidDefaults)
{
    const std::pair<const char*, const char*> kinds[] = {
        {"reflector.parabolic", "reflector"}, {"reflector.elliptical", "reflector"}, {"reflector.spherical", "reflector"},
        {"reflector.cylindrical", "reflector"}, {"reflector.flat", "reflector"},     {"lens.spherical", "lens"},
        {"waveguide.circular", "waveguide"},  {"waveguide.rectangular", "waveguide"}, {"cavity.rectangular", "cavity"},
        {"cavity.cylindrical", "cavity"},     {"cavity.spherical", "cavity"},         {"diffraction.slit", "diffraction"}};
    for (const auto& [id, category] : kinds) {
        const GeometryKind* kind = GeometryRegistry::instance().find(id);
        ASSERT_NE(kind, nullptr) << id;
        EXPECT_EQ(kind->category, category);
        const Geometry geometry = make(id);
        EXPECT_TRUE(geometry.validate().ok()) << id;
        EXPECT_FALSE(geometry.tessellate().empty()) << id;
    }
    // The seven primitives are still there and still primitives.
    EXPECT_EQ(GeometryRegistry::instance().kindsInCategory("primitive").size(), 7u);
}

TEST(ManipulatorGeometry, ParabolicSurfaceSatisfiesItsEquationAndFocalProperty)
{
    const double f = 0.05, D = 0.2;
    const Geometry parabola = make("reflector.parabolic", {{"focalLength", f}, {"aperture", D}});
    const Vec3 focus(0, 0, f);
    const auto elements = sampleReflectorSurface(parabola, 0.004);
    ASSERT_GT(elements.size(), 1000u);
    double area = 0.0;
    for (const SurfaceElement& e : elements) {
        const double r = std::hypot(e.position.x(), e.position.y());
        EXPECT_NEAR(e.position.z(), r * r / (4.0 * f), 1e-15);          // z = r^2 / 4f
        EXPECT_LE(r, 0.5 * D);
        // Definition of a parabola: distance to the focus = distance to the directrix z = -f.
        EXPECT_NEAR((e.position - focus).norm(), e.position.z() + f, 1e-14);
        EXPECT_NEAR(e.normal.norm(), 1.0, 1e-14);
        EXPECT_GT(e.normal.z(), 0.0);                                   // towards the concave side
        // The normal bisects the directions to the focus and along the axis.
        const Vec3 toFocus = (focus - e.position).normalized();
        EXPECT_NEAR(e.normal.dot(toFocus), e.normal.dot(Vec3::UnitZ()), 1e-12);
        area += e.area;
    }
    // Area of a paraboloid: (8 pi f^2 / 3) [ (1 + (D / 4f)^2)^(3/2) - 1 ].
    const double exact = 8.0 * pi * f * f / 3.0 * (std::pow(1.0 + (D / (4.0 * f)) * (D / (4.0 * f)), 1.5) - 1.0);
    EXPECT_NEAR(reflectorArea(parabola), exact, 1e-9 * exact);
    EXPECT_NEAR(area, exact, 2e-3 * exact);
    EXPECT_EQ(estimateSurfaceElements(parabola, 0.004), elements.size());
    EXPECT_NEAR(parabolaDepth(f, D), 0.05, 1e-15);                      // D^2 / 16 f
    const auto points = characteristicPoints(parabola);
    ASSERT_EQ(points.size(), 1u);
    EXPECT_TRUE(points[0].position.isApprox(focus));
    // The display mesh reaches the rim depth and the back of the shell.
    const Aabb bounds = parabola.tessellate().bounds();
    EXPECT_NEAR(bounds.max.z(), 0.05, 1e-6);
    EXPECT_NEAR(bounds.min.z(), -0.003, 1e-6);
}

TEST(ManipulatorGeometry, EllipticalSurfaceKeepsTheSumOfFocalDistances)
{
    const double a = 0.15, b = 0.09;
    const Geometry ellipse = make("reflector.elliptical", {{"semiMajor", a}, {"semiMinor", b}, {"cut", 0.3}});
    const double c = ellipseFocalDistance(a, b);
    EXPECT_NEAR(c, 0.12, 1e-15);  // sqrt(0.15^2 - 0.09^2)
    const Vec3 f1(0, 0, -c), f2(0, 0, c);
    const auto elements = sampleReflectorSurface(ellipse, 0.005);
    ASSERT_GT(elements.size(), 1000u);
    for (const SurfaceElement& e : elements) {
        EXPECT_NEAR((e.position - f1).norm() + (e.position - f2).norm(), 2.0 * a, 1e-13);
        EXPECT_LE(e.position.z(), 0.3 * a + 1e-12);
        // The inward normal bisects the directions to the two foci.
        EXPECT_NEAR(e.normal.dot((f1 - e.position).normalized()), e.normal.dot((f2 - e.position).normalized()), 1e-12);
    }
    const auto points = characteristicPoints(ellipse);
    ASSERT_EQ(points.size(), 2u);
    EXPECT_TRUE(points[0].position.isApprox(f1));
    EXPECT_TRUE(points[1].position.isApprox(f2));
    // A "prolate" spheroid needs b < a.
    Geometry bad = ellipse;
    EXPECT_FALSE(bad.setParameter("semiMinor", 0.2).ok());
}

TEST(ManipulatorGeometry, SphericalFlatAndTroughSurfaces)
{
    const double R = 0.2, D = 0.2;
    const Geometry sphere = make("reflector.spherical", {{"curvatureRadius", R}, {"aperture", D}});
    const Vec3 centre(0, 0, R);
    double area = 0.0;
    for (const SurfaceElement& e : sampleReflectorSurface(sphere, 0.004)) {
        EXPECT_NEAR((e.position - centre).norm(), R, 1e-14);
        EXPECT_TRUE(e.normal.isApprox((centre - e.position).normalized(), 1e-12));
        area += e.area;
    }
    // Spherical cap: 2 pi R h, h = R - sqrt(R^2 - (D/2)^2).
    const double cap = 2.0 * pi * R * (R - std::sqrt(R * R - 0.25 * D * D));
    EXPECT_NEAR(reflectorArea(sphere), cap, 1e-9 * cap);
    EXPECT_NEAR(area, cap, 2e-3 * cap);
    Geometry tooWide = sphere;
    EXPECT_FALSE(tooWide.setParameter("aperture", 0.5).ok());

    const Geometry flat = make("reflector.flat", {{"width", 0.3}, {"height", 0.2}});
    area = 0.0;
    for (const SurfaceElement& e : sampleReflectorSurface(flat, 0.01)) {
        EXPECT_DOUBLE_EQ(e.position.z(), 0.0);
        EXPECT_TRUE(e.normal.isApprox(Vec3::UnitZ()));
        area += e.area;
    }
    EXPECT_NEAR(area, 0.06, 1e-12);

    const double f = 0.05;
    const Geometry trough = make("reflector.cylindrical", {{"focalLength", f}, {"width", 0.2}, {"length", 0.3}});
    area = 0.0;
    for (const SurfaceElement& e : sampleReflectorSurface(trough, 0.004)) {
        EXPECT_NEAR(e.position.z(), e.position.x() * e.position.x() / (4.0 * f), 1e-15);
        EXPECT_DOUBLE_EQ(e.normal.y(), 0.0);
        area += e.area;
    }
    // Arc of the parabola from -0.1 to 0.1 is 2 f (u sqrt(1 + u^2) + asinh u), u = 1.
    const double arc = 2.0 * f * (std::sqrt(2.0) + std::asinh(1.0));
    EXPECT_NEAR(reflectorArea(trough), arc * 0.3, 1e-12);
    EXPECT_NEAR(area, arc * 0.3, 2e-3 * arc * 0.3);
}

// --- rays (geometrical acoustics) -----------------------------------------------------

TEST(ManipulatorRays, AxisParallelRaysReflectThroughTheParabolasFocus)
{
    const double f = 0.05;
    const Geometry parabola = make("reflector.parabolic", {{"focalLength", f}, {"aperture", 0.2}});
    for (const Vec3& offset : {Vec3(0.01, 0, 0), Vec3(0.05, 0.03, 0), Vec3(-0.07, 0.06, 0), Vec3(0, -0.099, 0)}) {
        const auto hit = reflectRay(parabola, Ray{offset + Vec3(0, 0, 1.0), Vec3(0, 0, -1)});
        ASSERT_TRUE(hit.has_value());
        EXPECT_LT(distanceToLine(Vec3(0, 0, f), hit->point, hit->reflected), 1e-13);
        EXPECT_NEAR(hit->reflected.norm(), 1.0, 1e-14);
    }
    // And the reverse: rays from the focus leave parallel to the axis.
    const auto fromFocus = reflectRay(parabola, Ray{Vec3(0, 0, f), Vec3(0.6, 0.2, -0.5)});
    ASSERT_TRUE(fromFocus.has_value());
    EXPECT_TRUE(fromFocus->reflected.isApprox(Vec3::UnitZ(), 1e-12));
    // A ray outside the aperture misses.
    EXPECT_FALSE(reflectRay(parabola, Ray{Vec3(0.2, 0, 1.0), Vec3(0, 0, -1)}).has_value());
}

TEST(ManipulatorRays, RaysFromOneFocusOfAnEllipsePassThroughTheOther)
{
    const double a = 0.15, b = 0.09;
    const Geometry ellipse = make("reflector.elliptical", {{"semiMajor", a}, {"semiMinor", b}, {"cut", 0.0}});
    const double c = ellipseFocalDistance(a, b);
    for (const Vec3& direction : {Vec3(0.3, 0, -1), Vec3(1, 0.2, -0.1), Vec3(-0.5, 0.7, -0.4), Vec3(0.9, 0, -0.3)}) {
        const auto hit = reflectRay(ellipse, Ray{Vec3(0, 0, -c), direction});
        ASSERT_TRUE(hit.has_value());
        EXPECT_LT(distanceToLine(Vec3(0, 0, c), hit->point, hit->reflected), 1e-13);
        // The reflected ray travels towards the second focus, not away from it.
        EXPECT_GT(hit->reflected.dot(Vec3(0, 0, c) - hit->point), 0.0);
    }
}

TEST(ManipulatorRays, SphericalMirrorHasSphericalAberration)
{
    const double R = 0.2;
    const Geometry sphere = make("reflector.spherical", {{"curvatureRadius", R}, {"aperture", 0.3}});
    double previous = 0.5 * R + 1e-12;
    for (const double h : {0.001, 0.03, 0.06, 0.1, 0.14}) {
        const auto hit = reflectRay(sphere, Ray{Vec3(h, 0, 1.0), Vec3(0, 0, -1)});
        ASSERT_TRUE(hit.has_value());
        // Where the reflected ray crosses the axis (x = 0).
        const double t = -hit->point.x() / hit->reflected.x();
        const double crossing = hit->point.z() + t * hit->reflected.z();
        EXPECT_NEAR(crossing, sphericalMirrorAxisCrossing(R, h), 1e-12);
        EXPECT_LT(crossing, previous);  // marginal rays cross closer to the mirror
        previous = crossing;
    }
    EXPECT_NEAR(sphericalMirrorAxisCrossing(R, 1e-9), 0.5 * R, 1e-12);  // paraxial focus R / 2
}

TEST(ManipulatorRays, FlatReflectorMirrorsTheDirectionAndTraceRaysWorksInWorldSpace)
{
    const Geometry flat = make("reflector.flat", {{"width", 1.0}, {"height", 1.0}});
    const auto hit = reflectRay(flat, Ray{Vec3(0.1, 0.2, 0.5), Vec3(0.3, -0.1, -1.0)});
    ASSERT_TRUE(hit.has_value());
    EXPECT_TRUE(hit->reflected.isApprox(Vec3(0.3, -0.1, 1.0).normalized(), 1e-13));
    EXPECT_DOUBLE_EQ(hit->point.z(), 0.0);

    // A parabolic reflector moved and turned in the scene: rays from its
    // (world) focus still leave along its (world) axis.
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.2}}));
    ObjectState state = setup.scene.find(setup.id)->state;
    state.local.position = Vec3(1.0, 2.0, 3.0);
    state.local.rotation = Vec3(0.0, pi / 2.0, 0.0);  // local +Z -> world +X
    ASSERT_TRUE(setup.scene.setState(setup.id, state).ok());
    const Vec3 worldFocus(1.05, 2.0, 3.0);
    const auto paths = traceRays(setup.scene, *setup.scene.find(setup.id), worldFocus, 24, 0.5);
    ASSERT_GE(paths.size(), 12u);
    for (const RayPath& path : paths) {
        ASSERT_TRUE(path.hit);
        ASSERT_EQ(path.points.size(), 3u);
        EXPECT_TRUE((path.points[2] - path.points[1]).normalized().isApprox(Vec3::UnitX(), 1e-10));
    }
}

// --- analytical estimates ---------------------------------------------------------------

TEST(ManipulatorAnalytics, MirrorEquationAndInterfaces)
{
    EXPECT_NEAR(mirrorImageDistance(0.05, 2.0), 1.0 / (1.0 / 0.05 - 1.0 / 2.0), 1e-15);
    EXPECT_NEAR(mirrorImageDistance(0.05, 0.1), 0.1, 1e-15);       // source at 2f images at 2f
    EXPECT_TRUE(std::isinf(mirrorImageDistance(0.05, 0.05)));       // source at the focus: collimated

    EXPECT_DOUBLE_EQ(pressureReflectionCoefficient(400.0, 400.0), 0.0);
    EXPECT_DOUBLE_EQ(intensityTransmissionCoefficient(400.0, 400.0), 1.0);
    // Air (about 413 rayl) to water (about 1.48e6 rayl): almost total reflection.
    const double r = pressureReflectionCoefficient(413.0, 1.48e6);
    const double t = intensityTransmissionCoefficient(413.0, 1.48e6);
    EXPECT_NEAR(r, 0.99944, 1e-5);
    EXPECT_NEAR(t, 1.0 - r * r, 1e-12);                              // energy conservation
    EXPECT_NEAR(t, 0.001116, 1e-6);
    EXPECT_DOUBLE_EQ(pressureReflectionCoefficient(1.48e6, 413.0), -r);
}

TEST(ManipulatorAnalytics, LensEstimateFollowsTheLensmakersEquation)
{
    // Thin biconvex lens, R1 = +0.1, R2 = -0.1, of a material slower than the
    // medium (n = 1.5): 1/f = 0.5 * (10 + 10) -> f = 0.1.
    const LensEstimate thin = estimateLens(0.1, -0.1, 0.0, 1500.0, 1000.0, 1000.0, 1100.0);
    EXPECT_NEAR(thin.refractiveIndex, 1.5, 1e-15);
    EXPECT_NEAR(thin.focalLength, 0.1, 1e-12);
    // Thickness adds the (n - 1) d / (n R1 R2) term.
    const double d = 0.02;
    const LensEstimate thick = estimateLens(0.1, -0.1, d, 1500.0, 1000.0, 1000.0, 1100.0);
    EXPECT_NEAR(1.0 / thick.focalLength, 0.5 * (10.0 + 10.0 + 0.5 * d / (1.5 * 0.1 * -0.1)), 1e-9);
    // A plano-convex lens.
    EXPECT_NEAR(estimateLens(0.1, 0.0, 0.01, 1500.0, 1000.0, 1000.0, 1100.0).focalLength, 0.2, 1e-12);
    // A solid lens FASTER than the medium (acrylic in water, n = 0.54):
    // the same convex shape diverges, and the estimate says so.
    const LensEstimate fast = estimateLens(0.1, -0.1, 0.0, 1482.0, 998.0, 2730.0, 1180.0);
    EXPECT_LT(fast.refractiveIndex, 1.0);
    EXPECT_LT(fast.focalLength, 0.0);
    bool warned = false;
    for (const std::string& a : fast.assumptions)
        warned = warned || a.find("diverges") != std::string::npos;
    EXPECT_TRUE(warned);
    EXPECT_NEAR(fast.surfaceTransmission1, intensityTransmissionCoefficient(998.0 * 1482.0, 1180.0 * 2730.0), 1e-15);
    // Flat on both sides: no focusing power.
    EXPECT_TRUE(std::isinf(estimateLens(0.0, 0.0, 0.01, 1500.0, 1000.0, 1000.0, 1100.0).focalLength));

    // The lens geometry refuses surfaces that would intersect.
    Geometry lens = make("lens.spherical");
    EXPECT_TRUE(lens.validate().ok());
    EXPECT_FALSE(lens.setParameter("centerThickness", 0.001).ok());   // thinner than the two sags
    EXPECT_FALSE(lens.setParameter("radius1", 0.02).ok());            // radius smaller than half the aperture
}

TEST(ManipulatorAnalytics, TabulatedBesselZerosAreZerosOfTheDerivatives)
{
    // The duct and cylinder formulas rest on zeros of J_m'. Check the lowest
    // modes they produce against J_m'(x) = (J_{m-1}(x) - J_{m+1}(x)) / 2.
    const double a = 0.05;
    for (const Mode& mode : circularDuctCutoffs(a, kC, 13)) {
        const double x = 2.0 * pi * mode.frequencyHz * a / kC;
        const double derivative = mode.m == 0 ? -std::cyl_bessel_j(1.0, x)
                                              : 0.5 * (std::cyl_bessel_j(mode.m - 1.0, x) - std::cyl_bessel_j(mode.m + 1.0, x));
        EXPECT_NEAR(derivative, 0.0, 1e-6) << "m=" << mode.m << " n=" << mode.n;
    }
    // Spherical cavity: zeros of j_n'(x) = j_{n-1}(x) - (n + 1) j_n(x) / x.
    for (const Mode& mode : sphericalCavityModes(a, kC, 8)) {
        const double x = 2.0 * pi * mode.frequencyHz * a / kC;
        const unsigned n = static_cast<unsigned>(mode.m);
        const double derivative = n == 0 ? -std::sph_bessel(1, x) : std::sph_bessel(n - 1, x) - (n + 1.0) * std::sph_bessel(n, x) / x;
        EXPECT_NEAR(derivative, 0.0, 1e-6) << "n=" << n;
    }
}

TEST(ManipulatorAnalytics, DuctCutoffsAndCavityResonances)
{
    // Rectangular duct 40 mm x 20 mm: first cut-off c / (2 a) across the long side.
    const auto rectangular = rectangularDuctCutoffs(0.04, 0.02, kC, 4);
    ASSERT_EQ(rectangular.size(), 4u);
    EXPECT_NEAR(rectangular[0].frequencyHz, kC / (2.0 * 0.04), 1e-9);
    EXPECT_EQ(rectangular[0].m, 1);
    EXPECT_EQ(rectangular[0].n, 0);
    EXPECT_NEAR(rectangular[1].frequencyHz, kC / (2.0 * 0.02), 1e-9);  // (2,0) and (0,1) coincide
    EXPECT_NEAR(rectangular[3].frequencyHz, 0.5 * kC * std::hypot(1.0 / 0.04, 1.0 / 0.02), 1e-9);
    for (size_t i = 1; i < rectangular.size(); ++i)
        EXPECT_GE(rectangular[i].frequencyHz, rectangular[i - 1].frequencyHz);

    // Circular duct: the first mode is (1,0) at 1.8412 c / (2 pi a).
    const auto circular = circularDuctCutoffs(0.02, kC, 3);
    EXPECT_NEAR(circular[0].frequencyHz, 1.8411838 * kC / (2.0 * pi * 0.02), 1e-6);
    EXPECT_EQ(circular[0].m, 1);

    // Rectangular cavity: lowest mode along the longest side.
    const auto box = rectangularCavityModes(0.3, 0.2, 0.15, kC, 5);
    EXPECT_NEAR(box[0].frequencyHz, kC / (2.0 * 0.3), 1e-9);
    EXPECT_NEAR(box[1].frequencyHz, kC / (2.0 * 0.2), 1e-9);
    // Closed cylinder, long and thin: the first modes are axial, q c / (2 L).
    const auto cylinder = cylindricalCavityModes(0.01, 0.5, kC, 3);
    EXPECT_NEAR(cylinder[0].frequencyHz, kC / (2.0 * 0.5), 1e-9);
    EXPECT_NEAR(cylinder[1].frequencyHz, 2.0 * kC / (2.0 * 0.5), 1e-9);
    // Sphere: lowest mode 2.0816 c / (2 pi a).
    EXPECT_NEAR(sphericalCavityModes(0.1, kC, 1)[0].frequencyHz, 2.0815759 * kC / (2.0 * pi * 0.1), 1e-6);

    // Helmholtz resonator: 1 litre, neck radius 10 mm, length 50 mm.
    const double expected = kC / (2.0 * pi) * std::sqrt(pi * 1e-4 / (1e-3 * (0.05 + 0.017)));
    EXPECT_NEAR(helmholtzFrequency(0.01, 0.05, 1e-3, kC), expected, 1e-9);
    EXPECT_NEAR(quarterWaveFrequency(0.25, kC, 1), 340.0, 1e-12);
    EXPECT_NEAR(quarterWaveFrequency(0.25, kC, 2), 1020.0, 1e-12);
}

// --- the reflector field model --------------------------------------------------------------

TEST(ReflectorModel, LargeFlatRigidPlateActsAsAnImageSource)
{
    // A rigid plate 40 wavelengths square, source 5 wavelengths above its
    // centre. The scattered field must equal the field of the mirror image
    // of the source (exact for an infinite plane).
    ReflectorScene setup(make("reflector.flat", {{"width", 0.4}, {"height", 0.4}}));
    const auto model = setup.model();
    const PointSource source = sourceAt(Vec3(0, 0, 0.05));
    const Vec3 image(0, 0, -0.05);
    for (const Vec3& point : {Vec3(0.0, 0.0, 0.08), Vec3(0.03, -0.02, 0.06), Vec3(-0.04, 0.05, 0.1)}) {
        const double r = (point - image).norm();
        const Complex expected = std::polar(1.0 / r, -2.0 * pi * r / kLambda);
        const Complex scattered = model->scattered(source, point, gas());
        EXPECT_LT(std::abs(scattered - expected) / std::abs(expected), 0.04) << point.transpose();
    }
}

TEST(ReflectorModel, PressureReleasePlateActsAsANegativeImage)
{
    ReflectorScene setup(make("reflector.flat", {{"width", 0.4}, {"height", 0.4}}), BoundaryCondition::PressureRelease);
    const auto model = setup.model();
    const PointSource source = sourceAt(Vec3(0, 0, 0.05));
    const Vec3 point(0.02, 0.01, 0.07);
    const double r = (point - Vec3(0, 0, -0.05)).norm();
    const Complex expected = -std::polar(1.0 / r, -2.0 * pi * r / kLambda);
    EXPECT_LT(std::abs(model->scattered(source, point, gas()) - expected) / std::abs(expected), 0.05);
}

TEST(ReflectorModel, RigidPlateCastsAShadowAndImpedanceMatchedPlateReflectsNothing)
{
    const Geometry plate = make("reflector.flat", {{"width", 0.4}, {"height", 0.4}});
    const PointSource source = sourceAt(Vec3(0, 0, 0.05));
    const Vec3 behind(0.0, 0.0, -0.04);

    ReflectorScene rigid(plate);
    const Transfer total = rigid.model()->transfer(source, behind, gas());
    const double incident = 1.0 / 0.09;
    EXPECT_LT(std::abs(total.value), 0.08 * incident);  // deep in the shadow of a large plate

    // An impedance boundary whose impedance equals the medium's reflects
    // nothing at normal incidence; a very stiff one behaves like a rigid one.
    for (const auto& [impedance, expectedFraction] : {std::pair{gas().impedance(), 0.0}, {1e9, 1.0}}) {
        std::vector<ReflectingSurface> surfaces = collectReflectors(rigid.scene, rigid.materials, kLambda).surfaces;
        surfaces[0].boundary = BoundaryCondition::Impedance;
        surfaces[0].impedance = impedance;
        const ReflectorModel model(PropagationOptions{}, surfaces);
        const Vec3 above(0, 0, 0.08);
        const double image = 1.0 / 0.13;
        EXPECT_NEAR(std::abs(model.scattered(source, above, gas())) / image, expectedFraction, 0.06) << impedance;
    }
}

TEST(ReflectorModel, CombinedEvaluatorEqualsTheSumOverSources)
{
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.1}}));
    const auto model = setup.model(4.0);
    std::vector<PointSource> sources = {sourceAt(Vec3(0, 0, 0.05)), sourceAt(Vec3(0.02, 0, 0.08)), sourceAt(Vec3(0, 0, -0.05))};
    sources[1].amplitude = std::polar(2.0, 1.0);
    sources[2].amplitude = Complex(0.5, 0.0);  // behind the reflector: lights its back
    const ReflectorFieldEvaluator evaluator(sources, kF, gas(), model);
    EXPECT_EQ(evaluator.solverId(), "acoustic.reflector.kirchhoff");
    EXPECT_FALSE(evaluator.providesVelocity());
    EXPECT_GT(evaluator.surfaceElementCount(), 100u);
    for (const Vec3& point : {Vec3(0, 0, 0.2), Vec3(0.03, 0.01, 0.12), Vec3(0.0, 0.0, -0.1)}) {
        Complex expected(0.0, 0.0);
        for (const PointSource& source : sources)
            expected += source.amplitude * model->transfer(source, point, gas()).value;
        EXPECT_NEAR(std::abs(evaluator.pressure(point) - expected), 0.0, 1e-9 * std::abs(expected));
        EXPECT_NEAR(std::abs(evaluator.direct(point) + evaluator.scatteredField(point) - evaluator.pressure(point)), 0.0,
                    1e-9 * std::abs(expected));
    }
    // A point on the surface is flagged, not trusted.
    EXPECT_TRUE(evaluator.sample(Vec3(0.02, 0, 0.02 * 0.02 / 0.2)).nearSource);
    EXPECT_FALSE(evaluator.sample(Vec3(0, 0, 0.2)).nearSource);
}

TEST(ReflectorModel, ParabolicReflectorFocusesADistantSourceWhereTheMirrorEquationSays)
{
    // Source on the axis 2 m away; f = 50 mm, aperture 200 mm (20 wavelengths).
    // Geometrical acoustics puts the image at s' = 1 / (1/f - 1/s) = 51.28 mm.
    const double f = 0.05, s = 2.0;
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", f}, {"aperture", 0.2}}));
    const ReflectorFieldEvaluator evaluator({sourceAt(Vec3(0, 0, s))}, kF, gas(), setup.model());
    double bestZ = 0.0, best = 0.0;
    for (double z = 0.03; z <= 0.08; z += 0.00025) {
        const double value = std::abs(evaluator.pressure(Vec3(0, 0, z)));
        if (value > best) {
            best = value;
            bestZ = z;
        }
    }
    EXPECT_NEAR(bestZ, mirrorImageDistance(f, s), 0.5 * kLambda);
    // A real focus: far above the incident field there (1 / 1.95 Pa).
    EXPECT_GT(focalGain(evaluator, Vec3(0, 0, bestZ)), 20.0);
    // Diffraction-limited width: across the axis the focal spot is of the
    // order of a wavelength, not a point and not the aperture.
    const double half = 0.5 * best;
    double width = 0.0;
    for (double x = 0.0; x < 0.03; x += 0.0001) {
        if (std::abs(evaluator.pressure(Vec3(x, 0, bestZ))) < half) {
            width = 2.0 * x;
            break;
        }
    }
    EXPECT_GT(width, 0.3 * kLambda);
    EXPECT_LT(width, 2.0 * kLambda);
}

TEST(ReflectorModel, SourceAtTheFocusOfAParabolaGivesACollimatedBeam)
{
    const double f = 0.05, D = 0.2;
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", f}, {"aperture", D}}));
    const ReflectorFieldEvaluator evaluator({sourceAt(Vec3(0, 0, f))}, kF, gas(), setup.model());
    // Inside the beam's near field (z well below D^2 / (4 lambda) = 1 m) the
    // reflected beam does not spread: averaged over a wavelength of axial
    // ripple its amplitude at 0.6 m is about what it is at 0.3 m, while a
    // spherical wave would have halved.
    const auto meanReflected = [&](double z) {
        double sum = 0.0;
        for (int i = 0; i < 40; ++i)
            sum += std::abs(evaluator.scatteredField(Vec3(0.02, 0.0, z + i * kLambda / 40.0)));
        return sum / 40.0;
    };
    const double near = meanReflected(0.3);
    const double far = meanReflected(0.6);
    EXPECT_GT(far / near, 0.75);
    EXPECT_LT(far / near, 1.35);
    // Outside the aperture's shadow cylinder there is far less reflected sound.
    EXPECT_LT(std::abs(evaluator.scatteredField(Vec3(0.2, 0.0, 0.45))), 0.2 * near);
}

TEST(ReflectorModel, EllipticalReflectorCarriesSoundFromOneFocusToTheOther)
{
    const double a = 0.15, b = 0.09;
    ReflectorScene setup(make("reflector.elliptical", {{"semiMajor", a}, {"semiMinor", b}, {"cut", 0.0}}));
    const double c = ellipseFocalDistance(a, b);
    const ReflectorFieldEvaluator evaluator({sourceAt(Vec3(0, 0, -c))}, kF, gas(), setup.model());
    double bestZ = 0.0, best = 0.0;
    for (double z = 0.06; z <= 0.18; z += 0.00025) {
        const double value = std::abs(evaluator.scatteredField(Vec3(0, 0, z)));
        if (value > best) {
            best = value;
            bestZ = z;
        }
    }
    EXPECT_NEAR(bestZ, c, 0.75 * kLambda);
    // Without the reflector the source gives 1 / (2c) = 4.2 Pa at the second focus.
    EXPECT_GT(focalGain(evaluator, Vec3(0, 0, c)), 8.0);
}

TEST(ReflectorModel, ResultConvergesAsTheSurfaceResolutionIsRefined)
{
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.12}}));
    const PointSource source = sourceAt(Vec3(0, 0, 1.0));
    const Vec3 point(0.001, 0.0, 0.0525);
    std::vector<Complex> values;
    for (const double resolution : {2.0, 4.0, 8.0, 16.0})
        values.push_back(setup.model(resolution)->scattered(source, point, gas()));
    const double e1 = std::abs(values[0] - values[3]);
    const double e2 = std::abs(values[1] - values[3]);
    const double e3 = std::abs(values[2] - values[3]);
    EXPECT_LT(e2, e1);
    EXPECT_LT(e3, e2);
    EXPECT_LT(e3 / std::abs(values[3]), 0.01);  // 8 elements per wavelength is within 1 % of 16
    EXPECT_LT(std::abs(values[1] - values[3]) / std::abs(values[3]), 0.05);
}

TEST(ReflectorModel, CollectReportsWhatHasNoFieldModelAndUsesMaterialImpedance)
{
    ReflectorScene setup(make("reflector.flat", {{"width", 0.05}, {"height", 0.05}}), BoundaryCondition::Impedance);
    // No material assigned: treated as rigid, and said so.
    ReflectorCollection collection = collectReflectors(setup.scene, setup.materials, kLambda);
    ASSERT_EQ(collection.surfaces.size(), 1u);
    EXPECT_EQ(collection.surfaces[0].boundary, BoundaryCondition::Rigid);
    ASSERT_EQ(collection.notes.size(), 1u);
    EXPECT_NE(collection.notes[0].find("treated as rigid"), std::string::npos);

    // With a material: its rho c.
    ObjectState state = setup.scene.find(setup.id)->state;
    state.materialId = setup.materials.findByName("Acrylic")->id();
    ASSERT_TRUE(setup.scene.setState(setup.id, state).ok());
    collection = collectReflectors(setup.scene, setup.materials, kLambda);
    EXPECT_EQ(collection.surfaces[0].boundary, BoundaryCondition::Impedance);
    EXPECT_NEAR(collection.surfaces[0].impedance, 1180.0 * 2730.0, 1e-6);
    EXPECT_TRUE(collection.notes.empty());

    // A lens and a cavity are present but not simulated: reported.
    ASSERT_TRUE(setup.scene.insert(makeManipulator("Lens", "lens.spherical").take()).ok());
    ASSERT_TRUE(setup.scene.insert(makeManipulator("Box cavity", "cavity.rectangular").take()).ok());
    collection = collectReflectors(setup.scene, setup.materials, kLambda);
    EXPECT_EQ(collection.surfaces.size(), 1u);
    ASSERT_EQ(collection.notes.size(), 2u);
    EXPECT_NE(collection.notes[0].find("does NOT affect the calculated field"), std::string::npos);

    // The element count follows the wavelength, and the estimate matches it.
    const size_t coarse = collectReflectors(setup.scene, setup.materials, kLambda).elementCount;
    const size_t fine = collectReflectors(setup.scene, setup.materials, 0.5 * kLambda).elementCount;
    EXPECT_EQ(coarse, 30u * 30u);  // 50 mm / (10 mm / 6) = 30 per side
    EXPECT_EQ(fine, 60u * 60u);
    EXPECT_EQ(estimateReflectorElements(setup.scene, kLambda), coarse);
    // A scaled reflector has proportionally more surface.
    state = setup.scene.find(setup.id)->state;
    state.local.scale = Vec3(2.0, 2.0, 2.0);
    ASSERT_TRUE(setup.scene.setState(setup.id, state).ok());
    double area = 0.0;
    for (const SurfaceElement& e : collectReflectors(setup.scene, setup.materials, kLambda).surfaces[0].elements)
        area += e.area;
    EXPECT_NEAR(area, 0.01, 1e-12);  // (2 * 50 mm)^2
}

// --- hybrid focusing -----------------------------------------------------------------------------

TEST(HybridFocusing, ModelPhasesAlignDirectAndReflectedPathsAtTheTarget)
{
    // A small array facing a parabolic reflector; the target is in front of
    // the reflector where both the direct and the reflected sound arrive.
    ReflectorScene setup(make("reflector.parabolic", {{"focalLength", 0.06}, {"aperture", 0.16}}));
    ArraySettings array;
    array.countX = 8;
    array.spacingX = 0.005;
    array.element.frequencyHz = kF;
    auto object = makeArray("Array", array);
    object->state.local.position = Vec3(0, 0, 0.3);
    object->state.local.rotation = Vec3(pi, 0, 0);  // facing the reflector (-Z)
    const Id arrayId = setup.scene.insert(std::move(object)).value()->id();
    const Vec3 target(0.004, 0.0, 0.065);
    const auto model = setup.model();
    const std::vector<PointSource> sources = collectSources(setup.scene, {arrayId});

    const FocusSolution hybrid = computeModelFocus(sources, target, gas(), *model);
    ASSERT_EQ(hybrid.elements.size(), 8u);
    // With these phases every element's total response adds in phase:
    // |sum| equals the sum of magnitudes (the most these elements can give).
    Complex sum(0.0, 0.0);
    double bound = 0.0;
    for (size_t i = 0; i < sources.size(); ++i) {
        const Complex h = model->transfer(sources[i], target, gas()).value;
        sum += h * std::polar(1.0, hybrid.elements[i].phaseRad);
        bound += std::abs(h);
        EXPECT_GE(hybrid.elements[i].phaseRad, 0.0);
        EXPECT_LT(hybrid.elements[i].phaseRad, 2.0 * pi);
    }
    EXPECT_NEAR(std::abs(sum), bound, 1e-9 * bound);

    // Phases computed while ignoring the reflector do worse in the presence of it.
    const FreeFieldModel freeField;
    const FocusSolution naive = computeModelFocus(sources, target, gas(), freeField);
    Complex naiveSum(0.0, 0.0);
    for (size_t i = 0; i < sources.size(); ++i)
        naiveSum += model->transfer(sources[i], target, gas()).value * std::polar(1.0, naive.elements[i].phaseRad);
    EXPECT_LT(std::abs(naiveSum), 0.9 * bound);
    // With a free-field model the method reduces to the ordinary phase focus k r.
    for (size_t i = 0; i < sources.size(); ++i)
        EXPECT_NEAR(std::remainder(naive.elements[i].phaseRad - 2.0 * pi * naive.elements[i].distance / kLambda, 2.0 * pi), 0.0, 1e-9);
    // The reflector really takes part: the model's response differs from the free-field one.
    EXPECT_GT(std::abs(model->scattered(sources[0], target, gas())), 0.5 * std::abs(freeField.transfer(sources[0], target, gas()).value));
}

// --- focus metrics ----------------------------------------------------------------------------------

TEST(FocusMetrics, PeakWidthDisplacementAndSidelobeOfAKnownField)
{
    // A synthetic field: a Gaussian main peak of 10 Pa with standard
    // deviations 4 mm (U) and 8 mm (V), and a 3 Pa side peak along U.
    FieldResult result;
    result.frame.origin = Vec3(-0.05, 0.0, 0.0);
    result.frame.axisU = Vec3(0.1, 0, 0);
    result.frame.axisV = Vec3(0, 0, 0.1);
    result.frame.nu = result.frame.nv = 201;
    result.pressure.resize(result.frame.pointCount());
    result.nearSource.assign(result.frame.pointCount(), 0);
    const Vec3 centre(0.01, 0.0, 0.06);
    for (int iv = 0; iv < 201; ++iv)
        for (int iu = 0; iu < 201; ++iu) {
            const Vec3 p = result.frame.point(iu, iv, 0);
            const double main = 10.0 * std::exp(-0.5 * std::pow((p.x() - centre.x()) / 0.004, 2) - 0.5 * std::pow((p.z() - centre.z()) / 0.008, 2));
            const double side = 3.0 * std::exp(-0.5 * std::pow((p.x() + 0.02) / 0.003, 2) - 0.5 * std::pow((p.z() - centre.z()) / 0.008, 2));
            result.pressure[result.frame.index(iu, iv, 0)] = std::complex<float>(static_cast<float>(main + side), 0.0f);
        }

    const Vec3 intended(0.012, 0.0, 0.058);
    const FocusMetrics metrics = analyzeFocus(result, intended);
    ASSERT_TRUE(metrics.valid);
    EXPECT_TRUE(metrics.peakPoint.isApprox(centre, 1e-3));
    EXPECT_NEAR(metrics.peakMagnitude, 10.0, 1e-3);
    // Full width at half maximum of a Gaussian: 2 sqrt(2 ln 2) sigma.
    const double fwhm = 2.0 * std::sqrt(2.0 * std::log(2.0));
    EXPECT_NEAR(metrics.widthU, fwhm * 0.004, 2e-4);
    EXPECT_NEAR(metrics.widthV, fwhm * 0.008, 2e-4);
    EXPECT_DOUBLE_EQ(metrics.widthW, 0.0);  // a plane has no third axis
    ASSERT_TRUE(metrics.displacement.has_value());
    EXPECT_NEAR(*metrics.displacement, (centre - intended).norm(), 6e-4);
    ASSERT_TRUE(metrics.magnitudeAtIntended.has_value());
    EXPECT_NEAR(*metrics.magnitudeAtIntended, 10.0 * std::exp(-0.5 * 0.25 - 0.5 * 0.0625), 0.02);
    ASSERT_TRUE(metrics.sidelobeRatio.has_value());
    EXPECT_NEAR(*metrics.sidelobeRatio, 0.3, 0.01);

    // An intended point outside the sampled plane has no sampled value.
    EXPECT_FALSE(analyzeFocus(result, Vec3(0.0, 0.5, 0.06)).magnitudeAtIntended.has_value());
    // A peak cut off by the grid edge has no width, rather than a wrong one.
    FieldResult ramp = result;
    for (int iv = 0; iv < 201; ++iv)
        for (int iu = 0; iu < 201; ++iu)
            ramp.pressure[ramp.frame.index(iu, iv, 0)] = std::complex<float>(static_cast<float>(iu), 0.0f);
    EXPECT_DOUBLE_EQ(analyzeFocus(ramp).widthU, 0.0);
    EXPECT_FALSE(analyzeFocus(FieldResult{}).valid);
}

// --- constraints and optimization -------------------------------------------------------------------

TEST(ManufacturingConstraintsCheck, ReportsViolationsWithCorrectionsAndNeverEditsTheGeometry)
{
    const Geometry reflector = make("reflector.parabolic", {{"focalLength", 0.02}, {"aperture", 0.4}, {"thickness", 0.001}});
    const Geometry before = reflector;
    ManufacturingConstraints constraints;
    constraints.minimumWallThickness = 0.002;
    constraints.maximumDimension = 0.3;
    constraints.minimumCurvatureRadius = 0.05;
    const auto violations = checkConstraints(reflector, constraints);
    ASSERT_EQ(violations.size(), 3u);
    EXPECT_EQ(violations[0].constraint, "minimum wall thickness");
    EXPECT_DOUBLE_EQ(violations[0].actual, 0.001);
    EXPECT_DOUBLE_EQ(violations[0].limit, 0.002);
    EXPECT_FALSE(violations[0].correction.empty());
    EXPECT_EQ(violations[1].constraint, "maximum dimension");
    // The largest extent of this deep dish is its depth, D^2 / 16 f = 0.5 m, plus the shell.
    EXPECT_NEAR(violations[1].actual, 0.501, 1e-6);
    EXPECT_EQ(violations[2].constraint, "minimum curvature radius");
    EXPECT_DOUBLE_EQ(violations[2].actual, 0.04);  // 2 f at the vertex
    EXPECT_EQ(reflector, before);
    EXPECT_TRUE(checkConstraints(reflector, ManufacturingConstraints{}).empty());

    constraints = {};
    constraints.minimumFeatureSize = 0.02;
    EXPECT_EQ(checkConstraints(make("diffraction.slit"), constraints).size(), 1u);
}

TEST(GeometryOptimization, FindsTheFocalLengthThatFocusesAtTheTarget)
{
    // Source on the axis 0.5 m from the reflector; the pressure is wanted at
    // z = 70 mm. The mirror equation gives the focal length that images the
    // source there: f = 1 / (1/s + 1/s') = 61.4 mm. Start from 40 mm.
    const double s = 0.5, target = 0.07;
    const Geometry start = make("reflector.parabolic", {{"focalLength", 0.04}, {"aperture", 0.16}});
    const PointSource source = sourceAt(Vec3(0, 0, s));
    MaterialLibrary materials;
    int evaluated = 0;
    const GeometryObjective objective = [&](const Geometry& candidate) {
        ++evaluated;
        ReflectorScene setup(candidate);
        return std::abs(setup.model(5.0)->transfer(source, Vec3(0, 0, target), gas()).value);
    };
    OptimizationSettings settings;
    settings.maxEvaluations = 40;
    settings.tolerance = 5e-3;
    const OptimizationOutcome outcome = optimizeGeometry(start, {{"focalLength", 0.03, 0.12}}, objective, settings);

    ASSERT_TRUE(outcome.bestGeometry.has_value());
    EXPECT_TRUE(outcome.improved);
    EXPECT_GT(outcome.best.objective, 3.0 * outcome.initial.objective);  // a real, objective-driven improvement
    EXPECT_NEAR(outcome.bestGeometry->parameter("focalLength"), 1.0 / (1.0 / s + 1.0 / target), 0.004);
    EXPECT_DOUBLE_EQ(outcome.initial.values[0], 0.04);
    EXPECT_EQ(static_cast<int>(outcome.history.size()), evaluated);
    EXPECT_LE(evaluated, 40);
    EXPECT_NE(outcome.method.find("golden-section"), std::string::npos);
    // The starting geometry is untouched; other parameters are kept.
    EXPECT_DOUBLE_EQ(start.parameter("focalLength"), 0.04);
    EXPECT_DOUBLE_EQ(outcome.bestGeometry->parameter("aperture"), 0.16);
    // The history holds every candidate with its objective.
    for (const Candidate& candidate : outcome.history) {
        EXPECT_GE(candidate.values[0], 0.03);
        EXPECT_LE(candidate.values[0], 0.12);
        EXPECT_LE(candidate.objective, outcome.best.objective * (1.0 + 1e-12));
    }
}

TEST(GeometryOptimization, RespectsConstraintsAndRefusesBadProblems)
{
    const Geometry start = make("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.1}});
    // An objective that would like the largest aperture.
    const GeometryObjective bigger = [](const Geometry& g) { return g.parameter("aperture"); };
    OptimizationSettings settings;
    settings.constraints.maximumDimension = 0.25;
    const OptimizationOutcome outcome = optimizeGeometry(start, {{"aperture", 0.05, 0.6}}, bigger, settings);
    ASSERT_TRUE(outcome.bestGeometry.has_value());
    EXPECT_LE(outcome.bestGeometry->parameter("aperture"), 0.25 + 1e-9);
    EXPECT_GT(outcome.bestGeometry->parameter("aperture"), 0.2);
    bool infeasibleSeen = false;
    for (const Candidate& candidate : outcome.history)
        if (!candidate.feasible) {
            infeasibleSeen = true;
            EXPECT_NE(candidate.reason.find("maximum dimension"), std::string::npos);
        }
    EXPECT_TRUE(infeasibleSeen);

    EXPECT_FALSE(optimizeGeometry(start, {{"nonsense", 0.0, 1.0}}, bigger).bestGeometry.has_value());
    EXPECT_FALSE(optimizeGeometry(start, {{"aperture", 0.3, 0.1}}, bigger).bestGeometry.has_value());
    EXPECT_FALSE(optimizeGeometry(start, {}, bigger).bestGeometry.has_value());
}

// --- persistence and support levels -----------------------------------------------------------------

TEST(ManipulatorPersistence, RoundTripAndSupportLevels)
{
    auto project = Project::createNew("Manipulators");
    auto reflector = makeManipulator("Dish", "reflector.parabolic").take();
    ManipulatorSettings settings;
    settings.boundary = BoundaryCondition::Impedance;
    settings.intendedFocus = Vec3(0, 0, 0.05);
    settings.intendedSource = Vec3(0, 0, 2.0);
    settings.notes = "First design";
    reflector->state.component = std::make_shared<ManipulatorComponent>(settings);
    ASSERT_TRUE(reflector->state.geometry->setParameter("aperture", 0.3).ok());
    ASSERT_TRUE(project->scene.insert(std::move(reflector)).ok());
    for (const char* kind : {"lens.spherical", "waveguide.rectangular", "cavity.spherical", "diffraction.slit"})
        ASSERT_TRUE(project->scene.insert(makeManipulator(kind, kind).take()).ok());

    auto loaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    const auto& objects = loaded.value().project->scene.root().children();
    ASSERT_EQ(objects.size(), 5u);
    const auto* component = dynamic_cast<const ManipulatorComponent*>(objects[0]->state.component.get());
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->settings, settings);
    EXPECT_DOUBLE_EQ(objects[0]->state.geometry->parameter("aperture"), 0.3);
    EXPECT_EQ(serializeProject(*loaded.value().project), serializeProject(*project));

    // What is calculated for each kind is stated, not implied.
    const Category expected[] = {Category::Reflector, Category::Lens, Category::Waveguide, Category::ResonantStructure,
                                 Category::DiffractionStructure};
    const SupportLevel levels[] = {SupportLevel::ApproximateField, SupportLevel::AnalyticalEstimate,
                                   SupportLevel::AnalyticalEstimate, SupportLevel::AnalyticalEstimate, SupportLevel::GeometryOnly};
    for (size_t i = 0; i < 5; ++i) {
        const Geometry& geometry = *objects[i]->state.geometry;
        EXPECT_EQ(dynamic_cast<const ManipulatorComponent*>(objects[i]->state.component.get())->settings.category, expected[i]);
        EXPECT_EQ(supportOf(geometry), levels[i]);
        EXPECT_FALSE(supportDescription(geometry).empty());
    }
    EXPECT_NE(supportDescription(*objects[1]->state.geometry).find("NO field effect"), std::string::npos);
    EXPECT_NE(supportDescription(*objects[0]->state.geometry).find("Not a full-wave solution"), std::string::npos);
    EXPECT_FALSE(makeManipulator("x", "box").ok());  // a primitive is not a manipulator
}
