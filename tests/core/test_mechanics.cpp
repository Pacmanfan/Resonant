// Validation of the structural finite-element model against closed-form
// results. docs/mechanics/MECHANICAL_VALIDATION.md records the results and
// every tolerance stated here.
#include "dsp/Spectrum.h"
#include "mechanics/Analyses.h"
#include "mechanics/Meshing.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double pi = 3.14159265358979323846;

// Steel-like test values (exact numbers, not a material record).
ElasticMaterial steel()
{
    ElasticMaterial m;
    m.name = "test steel";
    m.density = 7800.0;
    m.youngsModulus = 2.0e11;
    m.poissonsRatio = 0.3;
    return m;
}

ElasticMaterial aluminium()
{
    ElasticMaterial m;
    m.name = "test aluminium";
    m.density = 2700.0;
    m.youngsModulus = 7.0e10;
    m.poissonsRatio = 0.33;
    return m;
}

enum class End { Free, Pinned, Clamped };

void support(StructuralModel& model, size_t node, End end)
{
    if (end == End::Pinned)
        fixDofs(model, node, {0, 1, 2, 3});  // translations and twist
    else if (end == End::Clamped)
        fixAll(model, node);
}

// A beam of length L along X, rectangular section b (along Y) x h (along Z).
StructuralModel beam(int elements, double L, double b, double h, End left, End right, const ElasticMaterial& m = steel())
{
    StructuralModel model = frameLine(Vec3::Zero(), Vec3(L, 0, 0), elements, rectangularSection(b, h), m, Vec3::UnitY());
    support(model, 0, left);
    support(model, model.nodes.size() - 1, right);
    return model;
}

// Frequencies of bending in the X-Z plane (the h direction), found among
// the modes by their participation along Z.
std::vector<double> bendingZ(const ModalSolution& modal)
{
    std::vector<double> f;
    for (const Mode& mode : modal.modes)
        if (mode.frequencyHz > 1e-3) {
            double z = 0.0, other = 0.0;
            for (Eigen::Index i = 0; i < mode.shape.size(); i += 6) {
                z += mode.shape(i + 2) * mode.shape(i + 2);
                other += mode.shape(i) * mode.shape(i) + mode.shape(i + 1) * mode.shape(i + 1) + mode.shape(i + 3) * mode.shape(i + 3);
            }
            if (z > 10.0 * other)
                f.push_back(mode.frequencyHz);
        }
    return f;
}

double beamFrequency(double betaL, double L, double b, double h, const ElasticMaterial& m)
{
    const double I = b * h * h * h / 12.0, A = b * h;
    return betaL * betaL / (2.0 * pi * L * L) * std::sqrt(m.youngsModulus * I / (m.density * A));
}

ForceInput force(const StructuralModel& model, size_t node, const Vec3& direction, double amplitude = 1.0)
{
    ForceInput f;
    f.label = "force";
    f.pattern = forcePattern(model, node, direction);
    f.amplitude = amplitude;
    f.position = model.nodes[node];
    f.direction = direction;
    return f;
}

}  // namespace

// --- material ---------------------------------------------------------------------------

TEST(MechanicalMaterial, DerivedConstantsFollowFromEAndNu)
{
    const ElasticMaterial m = steel();
    EXPECT_NEAR(m.shearModulus(), 2.0e11 / 2.6, 1.0);
    EXPECT_NEAR(m.bulkModulus(), 2.0e11 / 1.2, 1.0);
    EXPECT_NEAR(m.lameLambda(), 2.0e11 * 0.3 / (1.3 * 0.4), 1.0);
    // K = lambda + 2G/3
    EXPECT_NEAR(m.bulkModulus(), m.lameLambda() + 2.0 * m.shearModulus() / 3.0, 1e-3 * m.bulkModulus());
    EXPECT_NEAR(m.barSpeed(), std::sqrt(2.0e11 / 7800.0), 1e-9);
    EXPECT_GT(m.longitudinalSpeed(), m.barSpeed());
    EXPECT_NEAR(m.shearSpeed(), std::sqrt(m.shearModulus() / 7800.0), 1e-9);
    EXPECT_NEAR(m.plateBendingStiffness(0.002), 2.0e11 * 8e-9 / (12.0 * 0.91), 1e-6);

    ElasticMaterial bad = m;
    bad.poissonsRatio = 0.5;
    EXPECT_FALSE(bad.validate().ok());
    bad.poissonsRatio = -1.0;
    EXPECT_FALSE(bad.validate().ok());
    bad = m;
    bad.youngsModulus = 0.0;
    EXPECT_FALSE(bad.validate().ok());

    // From a material record: what is missing is named.
    Material record(Id("m"), "Half");
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Mechanical, "density", PropertyValue{1000.0}).ok());
    const auto missing = elasticMaterialOf(record);
    ASSERT_FALSE(missing.ok());
    EXPECT_NE(missing.message().find("Young's modulus, Poisson's ratio"), std::string::npos);
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Mechanical, "youngsModulus", PropertyValue{1e9}).ok());
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Mechanical, "poissonsRatio", PropertyValue{0.4}).ok());
    ASSERT_TRUE(record.setProperty(PhysicsDomain::Mechanical, "lossFactor", PropertyValue{0.05}).ok());
    const auto complete = elasticMaterialOf(record);
    ASSERT_TRUE(complete.ok()) << complete.message();
    EXPECT_TRUE(complete.value().lossFactorStated);
    EXPECT_DOUBLE_EQ(complete.value().lossFactor, 0.05);
}

TEST(MechanicalSections, PropertiesOfStandardSections)
{
    const Section r = rectangularSection(0.02, 0.01);
    EXPECT_DOUBLE_EQ(r.area, 2e-4);
    EXPECT_NEAR(r.iy, 0.02 * 1e-6 / 12.0, 1e-20);
    EXPECT_NEAR(r.iz, 0.01 * 8e-6 / 12.0, 1e-20);
    // Exact series value of the torsion constant for a 2:1 rectangle: 0.2287 b t^3.
    EXPECT_NEAR(r.torsion / (0.02 * 1e-6), 0.2287, 0.001);
    const Section c = circularSection(0.01);
    EXPECT_NEAR(c.torsion, pi * 1e-8 / 2.0, 1e-20);
    const Section t = tubeSection(0.01, 0.008);
    EXPECT_NEAR(t.area, pi * (1e-4 - 0.64e-4), 1e-15);
    EXPECT_TRUE(t.circular);
}

// --- frame elements ----------------------------------------------------------------------

TEST(MechanicalFrame, CantileverTipDeflectionIsExactForAnyMesh)
{
    const double L = 1.0, b = 0.02, h = 0.01, P = 50.0;
    const double I = b * h * h * h / 12.0;
    for (const int elements : {1, 4, 17}) {
        const StructuralModel model = beam(elements, L, b, h, End::Clamped, End::Free);
        const SystemMatrices system = assemble(model);
        const auto solved = solveStatic(model, system, {force(model, model.nodes.size() - 1, -Vec3::UnitZ(), P)});
        ASSERT_TRUE(solved.ok()) << solved.message();
        const double tip = model.translation(solved.value().displacement, model.nodes.size() - 1).z();
        EXPECT_NEAR(tip, -P * L * L * L / (3.0 * 2.0e11 * I), 1e-12) << elements;
        // Root stress M c / I = P L (h/2) / I.
        const ElementStress stress = elementStress(model, model.elements.front(), solved.value().displacement);
        ASSERT_TRUE(stress.available);
        EXPECT_NEAR(stress.maxNormal, P * L * (h / 2.0) / I, 1e-6 * P * L * (h / 2.0) / I);
    }
    // Axial and torsional stiffness.
    const StructuralModel rod = beam(5, L, b, h, End::Clamped, End::Free);
    const SystemMatrices system = assemble(rod);
    const auto axial = solveStatic(rod, system, {force(rod, rod.nodes.size() - 1, Vec3::UnitX(), 1000.0)});
    EXPECT_NEAR(rod.translation(axial.value().displacement, rod.nodes.size() - 1).x(), 1000.0 * L / (2.0e11 * b * h), 1e-15);
}

TEST(MechanicalFrame, AStructureWithoutSupportsIsRefusedForStatics)
{
    const StructuralModel model = beam(4, 1.0, 0.02, 0.01, End::Free, End::Free);
    const auto solved = solveStatic(model, assemble(model), {force(model, 2, Vec3::UnitZ())});
    ASSERT_FALSE(solved.ok());
    EXPECT_NE(solved.message().find("not held"), std::string::npos);
}

TEST(MechanicalFrame, BendingFrequenciesMatchEulerBernoulliForEverySupportCase)
{
    const double L = 1.0, b = 0.03, h = 0.01;
    struct Case {
        End left, right;
        std::vector<double> betaL;
    };
    const Case cases[] = {
        {End::Clamped, End::Free, {1.875104, 4.694091, 7.854757, 10.995541}},
        {End::Pinned, End::Pinned, {pi, 2 * pi, 3 * pi, 4 * pi}},
        {End::Clamped, End::Clamped, {4.730041, 7.853205, 10.995608, 14.137165}},
        {End::Free, End::Free, {4.730041, 7.853205, 10.995608, 14.137165}},
    };
    for (const Case& c : cases) {
        const StructuralModel model = beam(40, L, b, h, c.left, c.right);
        const auto modal = solveModal(model, assemble(model), 30);
        ASSERT_TRUE(modal.ok()) << modal.message();
        const std::vector<double> f = bendingZ(modal.value());
        ASSERT_GE(f.size(), 4u);
        for (size_t n = 0; n < 4; ++n)
            EXPECT_NEAR(f[n] / beamFrequency(c.betaL[n], L, b, h, steel()), 1.0, 1e-4) << int(c.left) << int(c.right) << " mode " << n;
        for (const Mode& mode : modal.value().modes)
            EXPECT_LT(mode.residual, 1e-8);
    }
    // A free-free beam has six rigid-body modes at zero frequency.
    const StructuralModel free = beam(20, L, b, h, End::Free, End::Free);
    const auto modal = solveModal(free, assemble(free), 8);
    for (int r = 0; r < 6; ++r)
        EXPECT_LT(modal.value().modes[static_cast<size_t>(r)].frequencyHz, 1e-3 * beamFrequency(4.730041, L, b, h, steel()));
    EXPECT_GT(modal.value().modes[6].frequencyHz, 0.5 * beamFrequency(4.730041, L, b, h, steel()));
}

TEST(MechanicalFrame, FrequenciesConvergeWithTheFourthPowerOfTheElementSize)
{
    // Hermite beam elements with consistent mass: the error falls by 2^4 per halving.
    const double L = 1.0, b = 0.03, h = 0.01;
    const double exact = beamFrequency(4.694091, L, b, h, steel());  // second cantilever mode
    std::vector<double> errors;
    for (const int elements : {4, 8, 16}) {
        const StructuralModel model = beam(elements, L, b, h, End::Clamped, End::Free);
        const auto modal = solveModal(model, assemble(model), 12);
        errors.push_back(bendingZ(modal.value())[1] / exact - 1.0);
    }
    EXPECT_GT(errors[0], 0.0);  // consistent mass: frequencies converge from above
    EXPECT_NEAR(std::log2(errors[0] / errors[1]), 4.0, 0.4);
    EXPECT_NEAR(std::log2(errors[1] / errors[2]), 4.0, 0.4);
}

TEST(MechanicalFrame, AxialAndTorsionalModesOfARod)
{
    const double L = 2.0, r = 0.02;
    const ElasticMaterial m = aluminium();
    StructuralModel model = frameLine(Vec3::Zero(), Vec3(0, 0, L), 60, circularSection(r), m, Vec3::UnitX());
    fixAll(model, 0);
    const auto modal = solveModal(model, assemble(model), 40);
    ASSERT_TRUE(modal.ok());
    // Classify by the dominant dof kind.
    std::vector<double> axial, torsion;
    for (const Mode& mode : modal.value().modes) {
        double uz = 0, rz = 0, total = 0;
        for (Eigen::Index i = 0; i < mode.shape.size(); i += 6) {
            uz += mode.shape(i + 2) * mode.shape(i + 2);
            rz += mode.shape(i + 5) * mode.shape(i + 5);
            total += mode.shape.segment(i, 6).squaredNorm();
        }
        if (uz > 0.99 * total)
            axial.push_back(mode.frequencyHz);
        if (rz > 0.99 * total)
            torsion.push_back(mode.frequencyHz);
    }
    ASSERT_GE(axial.size(), 2u);
    ASSERT_GE(torsion.size(), 2u);
    for (size_t n = 0; n < 2; ++n) {
        EXPECT_NEAR(axial[n], (2 * n + 1) * m.barSpeed() / (4 * L), 1e-3 * axial[n]);
        EXPECT_NEAR(torsion[n], (2 * n + 1) * m.shearSpeed() / (4 * L), 1e-3 * torsion[n]);
    }
    // Axial modes carry all the mass along Z between them, eventually.
    EXPECT_GT(modal.value().cumulativeMassFraction.z(), 0.9);
}

// --- plates ---------------------------------------------------------------------------------

namespace {

enum class Edge { Free, Simple, Clamped };

// Square or rectangular plate in the XY plane; edges in order x-, x+, y-, y+.
StructuralModel plate(double a, double b, double h, int n1, int n2, std::array<Edge, 4> edges, const ElasticMaterial& m = aluminium())
{
    StructuralModel model = rectangularGrid(ElementKind::Plate, Vec3::Zero(), Vec3::UnitX(), Vec3::UnitY(), a, b, n1, n2, h, 0.0, m);
    for (int j = 0; j <= n2; ++j)
        for (int i = 0; i <= n1; ++i) {
            const size_t node = static_cast<size_t>(i + (n1 + 1) * j);
            // dof 1 = rotation about axis1 (dw/dy), dof 2 = rotation about axis2 (-dw/dx)
            const auto apply = [&](Edge edge, bool alongY) {
                if (edge == Edge::Free)
                    return;
                model.fix(node, 0);
                model.fix(node, alongY ? 1 : 2);  // the slope along the edge vanishes with w
                if (edge == Edge::Clamped)
                    model.fix(node, alongY ? 2 : 1);
            };
            if (i == 0)
                apply(edges[0], true);
            if (i == n1)
                apply(edges[1], true);
            if (j == 0)
                apply(edges[2], false);
            if (j == n2)
                apply(edges[3], false);
        }
    return model;
}

double plateFrequency(double a, double b, double h, int m, int n, const ElasticMaterial& mat)
{
    const double D = mat.plateBendingStiffness(h);
    return pi / 2.0 * ((m / a) * (m / a) + (n / b) * (n / b)) * std::sqrt(D / (mat.density * h));
}

}  // namespace

TEST(MechanicalPlate, SimplySupportedPlateFrequenciesMatchNavier)
{
    const double a = 0.4, b = 0.3, h = 0.002;
    const StructuralModel model = plate(a, b, h, 24, 18, {Edge::Simple, Edge::Simple, Edge::Simple, Edge::Simple});
    const auto modal = solveModal(model, assemble(model), 6);
    ASSERT_TRUE(modal.ok()) << modal.message();
    std::vector<double> exact;
    for (int m = 1; m <= 4; ++m)
        for (int n = 1; n <= 4; ++n)
            exact.push_back(plateFrequency(a, b, h, m, n, aluminium()));
    std::sort(exact.begin(), exact.end());
    for (size_t r = 0; r < 6; ++r)
        EXPECT_NEAR(modal.value().modes[r].frequencyHz / exact[r], 1.0, 0.01) << r;
    // The (1,1) mode has no interior node line: its shape has one sign.
    const Eigen::VectorXd& shape = modal.value().modes[0].shape;
    double negative = 0.0;
    for (Eigen::Index i = 0; i < shape.size(); i += 3)
        negative = std::min(negative, shape(i));
    EXPECT_GE(negative, -1e-10);
}

TEST(MechanicalPlate, FrequencyConvergesWithRefinement)
{
    const double a = 0.3, h = 0.002;
    const double exact = plateFrequency(a, a, h, 1, 1, aluminium());
    std::vector<double> errors;
    for (const int n : {4, 8, 16}) {
        const StructuralModel model = plate(a, a, h, n, n, {Edge::Simple, Edge::Simple, Edge::Simple, Edge::Simple});
        errors.push_back(std::abs(solveModal(model, assemble(model), 1).value().modes[0].frequencyHz / exact - 1.0));
    }
    EXPECT_LT(errors[1], errors[0]);
    EXPECT_LT(errors[2], errors[1]);
    EXPECT_NEAR(std::log2(errors[1] / errors[2]), 2.0, 0.5);  // second order
    EXPECT_LT(errors[2], 0.003);
}

TEST(MechanicalPlate, ClampedPlateAndCentralLoadMatchClassicalCoefficients)
{
    const double a = 0.3, h = 0.002;
    const ElasticMaterial m = aluminium();
    const double D = m.plateBendingStiffness(h);
    // Clamped square plate: lambda^2 = 35.99 (Leissa).
    const StructuralModel clamped = plate(a, a, h, 20, 20, {Edge::Clamped, Edge::Clamped, Edge::Clamped, Edge::Clamped});
    const double f11 = solveModal(clamped, assemble(clamped), 1).value().modes[0].frequencyHz;
    EXPECT_NEAR(f11, 35.99 / (2 * pi * a * a) * std::sqrt(D / (m.density * h)), 0.01 * f11);
    // Simply supported square plate, central point load: w = 0.01160 P a^2 / D (Timoshenko, nu = 0.3).
    ElasticMaterial nu03 = m;
    nu03.poissonsRatio = 0.3;
    const StructuralModel simple = plate(a, a, h, 20, 20, {Edge::Simple, Edge::Simple, Edge::Simple, Edge::Simple}, nu03);
    const size_t centre = 10 + 21 * 10;
    const auto solved = solveStatic(simple, assemble(simple), {force(simple, centre, -Vec3::UnitZ(), 10.0)});
    ASSERT_TRUE(solved.ok());
    const double w = -simple.translation(solved.value().displacement, centre).z();
    EXPECT_NEAR(w, 0.01160 * 10.0 * a * a / nu03.plateBendingStiffness(h), 0.01 * w);
}

// --- membrane -------------------------------------------------------------------------------

TEST(MechanicalMembrane, RectangularMembraneFrequencies)
{
    const double a = 0.5, b = 0.35, h = 1e-4, tension = 200.0;
    ElasticMaterial film = aluminium();
    film.density = 1400.0;
    StructuralModel model = rectangularGrid(ElementKind::Membrane, Vec3::Zero(), Vec3::UnitX(), Vec3::UnitY(), a, b, 40, 28, h, tension, film);
    for (int j = 0; j <= 28; ++j)
        for (int i = 0; i <= 40; ++i)
            if (i == 0 || i == 40 || j == 0 || j == 28)
                model.fix(static_cast<size_t>(i + 41 * j), 0);
    const auto modal = solveModal(model, assemble(model), 5);
    ASSERT_TRUE(modal.ok());
    const double c = std::sqrt(tension / (film.density * h));
    std::vector<double> exact;
    for (int m = 1; m <= 4; ++m)
        for (int n = 1; n <= 4; ++n)
            exact.push_back(0.5 * c * std::hypot(m / a, n / b));
    std::sort(exact.begin(), exact.end());
    for (size_t r = 0; r < 5; ++r)
        EXPECT_NEAR(modal.value().modes[r].frequencyHz / exact[r], 1.0, 0.01) << r;
    EXPECT_FALSE(hasStress(ElementKind::Membrane));
    EXPECT_FALSE(elementStress(model, model.elements[0], modal.value().modes[0].shape).available);
}

// --- solids ---------------------------------------------------------------------------------

TEST(MechanicalSolid, PatchTestAndAxialBar)
{
    // A uniform strain field is reproduced exactly: the element passes the patch test.
    ElasticMaterial m = steel();
    m.poissonsRatio = 0.0;  // no lateral contraction: the fixed face does not stiffen the bar
    const double L = 0.2, s = 0.02, P = 4000.0;
    StructuralModel bar = hexBox(Vec3(L / 2, 0, 0), Vec3::UnitX(), Vec3::UnitY(), Vec3::UnitZ(), Vec3(L, s, s), 10, 2, 2, m);
    std::vector<ForceInput> loads;
    for (size_t node = 0; node < bar.nodes.size(); ++node) {
        if (std::abs(bar.nodes[node].x()) < 1e-12)
            fixAll(bar, node);
    }
    // Consistent nodal loads of a uniform traction on the end face: corners 1/16, edges 1/8, centre 1/4 of P on a 2x2 face mesh.
    for (size_t node = 0; node < bar.nodes.size(); ++node)
        if (std::abs(bar.nodes[node].x() - L) < 1e-12) {
            const int corner = (std::abs(std::abs(bar.nodes[node].y()) - s / 2) < 1e-12) + (std::abs(std::abs(bar.nodes[node].z()) - s / 2) < 1e-12);
            loads.push_back(force(bar, node, Vec3::UnitX(), P * (corner == 2 ? 1.0 / 16 : corner == 1 ? 1.0 / 8 : 1.0 / 4)));
        }
    const auto solved = solveStatic(bar, assemble(bar), loads);
    ASSERT_TRUE(solved.ok());
    for (size_t node = 0; node < bar.nodes.size(); ++node) {
        const double x = bar.nodes[node].x();
        EXPECT_NEAR(bar.translation(solved.value().displacement, node).x(), P * x / (m.youngsModulus * s * s), 1e-13);
    }
    for (const Element& e : bar.elements)
        EXPECT_NEAR(elementStress(bar, e, solved.value().displacement).maxNormal, P / (s * s), 1e-3);
    EXPECT_NEAR(bar.totalMass(), m.density * L * s * s, 1e-12);
}

TEST(MechanicalSolid, LongitudinalModesAndTheStiffnessOfTrilinearElementsInBending)
{
    ElasticMaterial m = steel();
    m.poissonsRatio = 0.0;
    const double L = 0.5, s = 0.01;
    StructuralModel bar = hexBox(Vec3(L / 2, 0, 0), Vec3::UnitX(), Vec3::UnitY(), Vec3::UnitZ(), Vec3(L, s, s), 50, 1, 1, m);
    for (size_t node = 0; node < bar.nodes.size(); ++node)
        if (std::abs(bar.nodes[node].x()) < 1e-12)
            fixAll(bar, node);
    const auto modal = solveModal(bar, assemble(bar), 12);
    ASSERT_TRUE(modal.ok());
    // The first mode whose motion is along X.
    double firstAxial = 0.0;
    for (const Mode& mode : modal.value().modes)
        if (mode.effectiveMassFraction.x() > 0.5) {
            firstAxial = mode.frequencyHz;
            break;
        }
    EXPECT_NEAR(firstAxial, m.barSpeed() / (4 * L), 1e-3 * firstAxial);

    // Bending: a cantilever of hexahedra is too stiff (shear locking of the
    // trilinear element) and approaches the beam result only slowly. This
    // is recorded, not hidden.
    const double P = 10.0, h = 0.02, Lb = 0.2;
    const double exact = P * Lb * Lb * Lb / (3 * m.youngsModulus * h * h * h * h / 12.0);
    std::vector<double> ratio;
    for (const int n : {5, 10, 20}) {
        StructuralModel c = hexBox(Vec3(Lb / 2, 0, 0), Vec3::UnitX(), Vec3::UnitY(), Vec3::UnitZ(), Vec3(Lb, h, h), n, 2, 2, m);
        std::vector<ForceInput> loads;
        for (size_t node = 0; node < c.nodes.size(); ++node) {
            if (std::abs(c.nodes[node].x()) < 1e-12)
                fixAll(c, node);
            if (std::abs(c.nodes[node].x() - Lb) < 1e-12)
                loads.push_back(force(c, node, -Vec3::UnitZ(), P / 9.0));
        }
        const auto solved = solveStatic(c, assemble(c), loads);
        double tip = 0.0;
        for (size_t node = 0; node < c.nodes.size(); ++node)
            if (std::abs(c.nodes[node].x() - Lb) < 1e-12)
                tip += -c.translation(solved.value().displacement, node).z() / 9.0;
        ratio.push_back(tip / exact);
    }
    EXPECT_LT(ratio[0], ratio[1]);
    EXPECT_LT(ratio[1], ratio[2]);
    EXPECT_LT(ratio[2], 1.0);
    EXPECT_GT(ratio[2], 0.7);
}

// --- eigensolver ------------------------------------------------------------------------------

TEST(MechanicalEigensolver, LanczosAgreesWithTheDenseSolver)
{
    const StructuralModel model = plate(0.4, 0.3, 0.002, 14, 10, {Edge::Simple, Edge::Free, Edge::Clamped, Edge::Simple});
    const SystemMatrices system = assemble(model);
    const auto dense = solveModal(model, system, 10, nullptr, false);
    const auto lanczos = solveModal(model, system, 10, nullptr, true);
    ASSERT_TRUE(dense.ok());
    ASSERT_TRUE(lanczos.ok());
    EXPECT_NE(dense.value().method.find("dense"), std::string::npos);
    EXPECT_NE(lanczos.value().method.find("Lanczos"), std::string::npos);
    for (size_t r = 0; r < 10; ++r) {
        EXPECT_NEAR(lanczos.value().modes[r].frequencyHz, dense.value().modes[r].frequencyHz, 1e-7 * dense.value().modes[r].frequencyHz);
        EXPECT_LT(lanczos.value().modes[r].residual, 1e-8);
        // Same shape up to the sign convention (largest component positive).
        EXPECT_NEAR(std::abs(lanczos.value().modes[r].shape.dot(dense.value().modes[r].shape))
                        / (lanczos.value().modes[r].shape.norm() * dense.value().modes[r].shape.norm()),
                    1.0, 1e-6);
    }
    // Free structure through Lanczos: rigid modes handled by the shift.
    const StructuralModel free = beam(300, 1.0, 0.03, 0.01, End::Free, End::Free);
    const auto modal = solveModal(free, assemble(free), 10, nullptr, true);
    ASSERT_TRUE(modal.ok());
    EXPECT_LT(modal.value().modes[5].frequencyHz, 1e-3 * modal.value().modes[6].frequencyHz);
    EXPECT_NEAR(bendingZ(modal.value())[0], beamFrequency(4.730041, 1.0, 0.03, 0.01, steel()), 1e-4 * bendingZ(modal.value())[0]);
}

// --- harmonic -----------------------------------------------------------------------------------

namespace {

StructuralModel cantilever(int elements = 20)
{
    return beam(elements, 0.5, 0.03, 0.005, End::Clamped, End::Free);
}

}  // namespace

TEST(MechanicalHarmonic, LowFrequencyResponseIsTheStaticComplianceAndDirectEqualsModalWithAllModes)
{
    const StructuralModel model = cantilever(8);
    const SystemMatrices system = assemble(model);
    const size_t tip = model.nodes.size() - 1;
    const std::vector<ForceInput> forces = {force(model, tip, Vec3::UnitZ())};
    const std::vector<ResponsePoint> points = {responseAt(model, tip, Vec3::UnitZ(), "tip")};
    HarmonicSettings settings;
    settings.frequenciesHz = {0.01, 3.0, 17.0, 120.0, 500.0};
    settings.damping.kind = DampingKind::Rayleigh;
    settings.damping.alpha = 2.0;
    settings.damping.beta = 1e-5;
    const auto direct = solveHarmonic(model, system, forces, points, settings);
    ASSERT_TRUE(direct.ok()) << direct.message();
    const double I = 0.03 * std::pow(0.005, 3) / 12.0;
    EXPECT_NEAR(direct.value().responses[0][0].real(), std::pow(0.5, 3) / (3 * 2.0e11 * I), 1e-6 * std::pow(0.5, 3) / (3 * 2.0e11 * I));
    settings.method = HarmonicMethod::Modal;
    settings.modeCount = static_cast<int>(system.map.freeCount());  // every mode
    const auto modal = solveHarmonic(model, system, forces, points, settings);
    ASSERT_TRUE(modal.ok());
    for (size_t i = 0; i < settings.frequenciesHz.size(); ++i)
        EXPECT_NEAR(std::abs(modal.value().responses[0][i] - direct.value().responses[0][i]), 0.0, 1e-8 * std::abs(direct.value().responses[0][i]));
    EXPECT_EQ(direct.value().fields.size(), settings.frequenciesHz.size());

    // Loss factor, direct against modal with all modes.
    settings.damping = Damping{};
    settings.damping.kind = DampingKind::LossFactor;
    settings.damping.eta = 0.02;
    settings.damping.materialLossFactors = false;
    settings.method = HarmonicMethod::Direct;
    const auto lossDirect = solveHarmonic(model, system, forces, points, settings);
    settings.method = HarmonicMethod::Modal;
    const auto lossModal = solveHarmonic(model, system, forces, points, settings);
    for (size_t i = 0; i < settings.frequenciesHz.size(); ++i)
        EXPECT_NEAR(std::abs(lossModal.value().responses[0][i] - lossDirect.value().responses[0][i]), 0.0,
                    1e-8 * std::abs(lossDirect.value().responses[0][i]));
    // A modal ratio has no direct form, and is refused there.
    settings.damping.kind = DampingKind::ModalRatio;
    settings.method = HarmonicMethod::Direct;
    EXPECT_FALSE(solveHarmonic(model, system, forces, points, settings).ok());
}

TEST(MechanicalHarmonic, ResonancePeakAndHalfPowerBandwidthGiveTheDampingRatio)
{
    const StructuralModel model = cantilever();
    const SystemMatrices system = assemble(model);
    const size_t tip = model.nodes.size() - 1;
    const double f1 = bendingZ(solveModal(model, system, 6).value())[0];
    HarmonicSettings settings;
    for (int i = 0; i <= 2000; ++i)
        settings.frequenciesHz.push_back(f1 * (0.9 + 0.2 * i / 2000.0));
    settings.damping.kind = DampingKind::ModalRatio;
    settings.damping.zeta = 0.01;
    settings.method = HarmonicMethod::Modal;
    settings.modeCount = 20;
    settings.keepFields = false;
    const auto solved = solveHarmonic(model, system, {force(model, tip, Vec3::UnitZ())}, {responseAt(model, tip, Vec3::UnitZ())}, settings);
    ASSERT_TRUE(solved.ok());
    EXPECT_TRUE(solved.value().fields.empty());
    const auto& r = solved.value().responses[0];
    size_t peak = 0;
    for (size_t i = 0; i < r.size(); ++i)
        if (std::abs(r[i]) > std::abs(r[peak]))
            peak = i;
    EXPECT_NEAR(settings.frequenciesHz[peak], f1 * std::sqrt(1 - 2 * 0.01 * 0.01), 1e-4 * f1);
    // Half-power points.
    const double level = std::abs(r[peak]) / std::sqrt(2.0);
    size_t lo = peak, hi = peak;
    while (lo > 0 && std::abs(r[lo]) > level)
        --lo;
    while (hi + 1 < r.size() && std::abs(r[hi]) > level)
        ++hi;
    const double bandwidth = settings.frequenciesHz[hi] - settings.frequenciesHz[lo];
    EXPECT_NEAR(bandwidth / f1, 2 * 0.01, 0.0005);
    // At resonance the response lags the force by 90 degrees.
    EXPECT_NEAR(std::abs(std::arg(r[peak])), pi / 2, 0.05);
}

// --- transient ----------------------------------------------------------------------------------

TEST(MechanicalTransient, EnergyIsConservedWithoutDampingAndStepLoadDoublesTheStaticDeflection)
{
    const StructuralModel model = cantilever(10);
    const SystemMatrices system = assemble(model);
    const size_t tip = model.nodes.size() - 1;
    const double f1 = bendingZ(solveModal(model, system, 6).value())[0];
    // A short half-sine pulse, then free vibration.
    ForceInput pulse = force(model, tip, Vec3::UnitZ(), 5.0);
    pulse.waveform.kind = dsp::WaveformKind::Sampled;
    pulse.waveform.sampleRateHz = 1e5;
    for (int i = 0; i <= 100; ++i)
        pulse.waveform.samples.push_back(std::sin(pi * i / 100.0));
    TransientSettings settings;
    settings.duration = 10.0 / f1;
    settings.timeStep = 1.0 / (200.0 * f1);
    const auto solved = solveTransient(model, system, {pulse}, {responseAt(model, tip, Vec3::UnitZ())}, settings);
    ASSERT_TRUE(solved.ok()) << solved.message();
    const TransientSolution& s = solved.value();
    // After the pulse: kinetic + strain energy equals the work done, to rounding.
    for (size_t i = 0; i < s.energyTimes.size(); ++i) {
        const double total = s.kinetic[i] + s.strain[i];
        EXPECT_NEAR(total, s.externalWork[i], 1e-9 * std::max(1e-30, s.externalWork.back()));
        if (s.energyTimes[i] > 2e-3)
            EXPECT_NEAR(total, s.kinetic.back() + s.strain.back(), 1e-9 * total);
    }

    // A suddenly applied constant force: peak deflection twice the static one
    // for a single mode; the cantilever tip is dominated by its first mode.
    ForceInput step = force(model, tip, Vec3::UnitZ(), 2.0);
    step.waveform.kind = dsp::WaveformKind::Sampled;
    step.waveform.sampleRateHz = 1.0;
    step.waveform.samples = {1.0, 1.0, 1.0};
    settings.duration = 1.5 / f1;
    const auto stepped = solveTransient(model, system, {step}, {responseAt(model, tip, Vec3::UnitZ())}, settings);
    const double staticTip = 2.0 * std::pow(0.5, 3) / (3 * 2.0e11 * 0.03 * std::pow(0.005, 3) / 12.0);
    double peak = 0.0;
    for (const double d : stepped.value().displacement[0])
        peak = std::max(peak, d);
    EXPECT_NEAR(peak / staticTip, 2.0, 0.06);
}

TEST(MechanicalTransient, FreeVibrationPeriodElongationFollowsTheNewmarkRelationAndRayleighDampingDecays)
{
    const StructuralModel model = cantilever(16);
    const SystemMatrices system = assemble(model);
    const size_t tip = model.nodes.size() - 1;
    const auto modal = solveModal(model, system, 6);
    const Mode* first = nullptr;
    for (const Mode& mode : modal.value().modes)
        if (std::abs(mode.shape(static_cast<Eigen::Index>(model.dof(tip, 2)))) > 1e-6) {
            first = &mode;
            break;
        }
    ASSERT_NE(first, nullptr);
    const double period = 1.0 / first->frequencyHz;
    // Released from the first mode shape: a pure cosine at the mode's frequency.
    const auto measuredPeriod = [&](double dt, const Damping& damping, std::vector<double>* history) {
        TransientSettings settings;
        settings.duration = 20.5 * period;
        settings.timeStep = dt;
        settings.damping = damping;
        settings.snapshotCount = 0;
        const auto solved = solveTransient(model, system, {}, {responseAt(model, tip, Vec3::UnitZ())}, settings, nullptr, &first->shape);
        const auto& x = solved.value().displacement[0];
        if (history)
            *history = x;
        // Upward zero crossings, interpolated.
        std::vector<double> crossings;
        for (size_t i = 1; i < x.size(); ++i)
            if (x[i - 1] < 0.0 && x[i] >= 0.0)
                crossings.push_back(dt * (static_cast<double>(i - 1) + x[i - 1] / (x[i - 1] - x[i])));
        return (crossings.back() - crossings.front()) / static_cast<double>(crossings.size() - 1);
    };
    const Damping none;
    for (const double stepsPerPeriod : {10.0, 20.0, 40.0}) {
        const double dt = period / stepsPerPeriod;
        const double error = measuredPeriod(dt, none, nullptr) / period - 1.0;
        EXPECT_NEAR(error, newmarkPeriodError(first->omega, dt), 1e-4) << stepsPerPeriod;
    }
    // Second-order: 4x smaller error per halving.
    EXPECT_NEAR(newmarkPeriodError(first->omega, period / 20) / newmarkPeriodError(first->omega, period / 40), 4.0, 0.1);
    EXPECT_NEAR(newmarkPeriodError(1.0, 0.1), 0.1 * 0.1 / 12.0, 1e-5);

    // Rayleigh damping (mass-proportional) of ratio zeta = alpha / (2 omega):
    // the amplitude decays by exp(-2 pi zeta / sqrt(1 - zeta^2)) per cycle.
    Damping rayleigh;
    rayleigh.kind = DampingKind::Rayleigh;
    rayleigh.alpha = 2.0 * 0.02 * first->omega;
    std::vector<double> x;
    measuredPeriod(period / 100, rayleigh, &x);
    const auto peakNear = [&](double t) {
        const size_t centre = static_cast<size_t>(t / (period / 100));
        double best = 0.0;
        for (size_t i = centre - 50; i < centre + 50; ++i)
            best = std::max(best, x[i]);
        return best;
    };
    const double decay = peakNear(10 * period) / peakNear(5 * period);
    EXPECT_NEAR(decay, std::exp(-5 * 2 * pi * 0.02 / std::sqrt(1 - 0.0004)), 0.01);
    // Loss factors have no time-domain form here.
    TransientSettings bad;
    bad.damping.kind = DampingKind::LossFactor;
    EXPECT_FALSE(solveTransient(model, system, {}, {}, bad).ok());
}

TEST(MechanicalTransient, LongitudinalPulseTravelsAtTheBarSpeedAndReflects)
{
    // A long rod along X, free at both ends; a force pulse at the left end.
    const ElasticMaterial m = steel();
    const double L = 2.0;
    StructuralModel rod = frameLine(Vec3::Zero(), Vec3(L, 0, 0), 400, circularSection(0.005), m, Vec3::UnitY());
    const size_t a = 100, b = 300;  // sensors at 0.5 m and 1.5 m
    ForceInput pulse = force(rod, 0, Vec3::UnitX(), 100.0);
    pulse.waveform.kind = dsp::WaveformKind::GaussianPulse;
    pulse.waveform.frequencyHz = 20000.0;
    pulse.waveform.fractionalBandwidth = 0.8;
    TransientSettings settings;
    settings.timeStep = 2e-7;
    settings.duration = 1.0e-3;
    settings.snapshotCount = 0;
    const auto solved = solveTransient(rod, assemble(rod), {pulse}, {responseAt(rod, a, Vec3::UnitX()), responseAt(rod, b, Vec3::UnitX())}, settings);
    ASSERT_TRUE(solved.ok());
    const auto& s = solved.value();
    // Arrival: the velocity pulse's largest excursion before reflections return.
    const auto arrival = [&](const std::vector<double>& v, double before) {
        size_t best = 0;
        for (size_t i = 0; i < v.size() && s.time[i] < before; ++i)
            if (std::abs(v[i]) > std::abs(v[best]))
                best = i;
        return s.time[best];
    };
    const double c = m.barSpeed();
    const double t1 = arrival(s.velocity[0], 2.4 / c), t2 = arrival(s.velocity[1], 2.4 / c);
    EXPECT_NEAR(1.0 / (t2 - t1), c, 0.01 * c);
    // At the free right end the velocity doubles and the wave returns: the
    // sensor at 1.5 m sees the reflected pulse with the same sign of velocity.
    double incident = 0.0, reflected = 0.0;
    for (size_t i = 0; i < s.time.size(); ++i) {
        if (s.time[i] < 2.2 / c)
            incident = std::abs(s.velocity[1][i]) > std::abs(incident) ? s.velocity[1][i] : incident;
        else if (s.time[i] < 3.2 / c)
            reflected = std::abs(s.velocity[1][i]) > std::abs(reflected) ? s.velocity[1][i] : reflected;
    }
    EXPECT_NEAR(reflected / incident, 1.0, 0.05);
}

TEST(MechanicalTransient, FlexuralWavesAreDispersiveGroupVelocityIsTwiceThePhaseVelocity)
{
    // A tone burst of bending waves on a long beam: unlike sound in air, the
    // speed depends on frequency. Euler-Bernoulli: c_phase = sqrt(omega) (EI / rho A)^(1/4), c_group = 2 c_phase.
    const ElasticMaterial m = steel();
    const double L = 4.0, b = 0.02, h = 0.004;
    StructuralModel bar = frameLine(Vec3::Zero(), Vec3(L, 0, 0), 800, rectangularSection(b, h), m, Vec3::UnitY());
    const double I = b * h * h * h / 12.0, A = b * h;
    for (const double f : {2000.0, 5000.0}) {
        const double omega = 2 * pi * f;
        const double phase = std::sqrt(omega) * std::pow(m.youngsModulus * I / (m.density * A), 0.25);
        ForceInput burst = force(bar, 0, Vec3::UnitZ(), 1.0);
        burst.waveform.kind = dsp::WaveformKind::ToneBurst;
        burst.waveform.frequencyHz = f;
        burst.waveform.cycles = 6.0;
        TransientSettings settings;
        settings.timeStep = 1.0 / (80.0 * f);
        // Long enough for the burst to pass the far sensor; the reflection from the
        // beam's far end arrives later.
        settings.duration = 2.5 / (2.0 * phase) + 1.5 * burst.waveform.length();
        settings.snapshotCount = 0;
        const size_t s1 = 200, s2 = 500;  // 1.0 m and 2.5 m
        const auto solved = solveTransient(bar, assemble(bar), {burst}, {responseAt(bar, s1, Vec3::UnitZ()), responseAt(bar, s2, Vec3::UnitZ())}, settings);
        ASSERT_TRUE(solved.ok());
        // Envelope arrival: centroid of the squared signal.
        const auto centroid = [&](const std::vector<double>& x) {
            double w = 0, t = 0;
            for (size_t i = 0; i < x.size(); ++i) {
                w += x[i] * x[i];
                t += x[i] * x[i] * solved.value().time[i];
            }
            return t / w;
        };
        const double group = 1.5 / (centroid(solved.value().displacement[1]) - centroid(solved.value().displacement[0]));
        EXPECT_NEAR(group, 2.0 * phase, 0.06 * 2.0 * phase) << f;
    }
}
