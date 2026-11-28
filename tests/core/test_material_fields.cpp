// Tensor-valued material fields (REQ-CHAT-20261128-006, RES-FAM-021, ADR-000234): frames, blends,
// interpolation without extrapolation, rotation into the object's axes, refusals, and their use
// by the three-dimensional layered solids.

#include "materials/MaterialField.h"
#include "mechanics/LayeredSolid.h"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

using namespace resonant;
using namespace resonant::materials;
using namespace resonant::mechanics;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<double> isotropicStiffness(double e, double nu)
{
    const double l = e * nu / ((1 + nu) * (1 - 2 * nu)), g = e / (2 * (1 + nu));
    std::vector<double> v;
    for (int i = 0; i < 6; ++i)
        for (int j = i; j < 6; ++j) {
            double c = 0;
            if (i < 3 && j < 3)
                c = i == j ? l + 2 * g : l;
            if (i >= 3 && i == j)
                c = g;
            v.push_back(c);
        }
    return v;
}

BodyLayer layer(double e, double nu, double rho, double t, int through)
{
    BodyLayer l;
    l.name = "layer";
    l.material.base.name = "m";
    l.material.base.youngsModulus = e;
    l.material.base.poissonsRatio = nu;
    l.material.base.density = rho;
    l.thickness.start = t;
    l.elementsThrough = through;
    return l;
}

double tipOf(const LayeredBody& body, double length)
{
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    LayeredLoads l;
    l.edgeForces = {{BodyEdge::U1, Vec3(0, 0, -10)}};
    auto r = solveLayeredStatic(body, s, l);
    EXPECT_TRUE(r.ok()) << r.message();
    if (!r.ok())
        return 0.0;
    double sum = 0;
    int n = 0;
    for (size_t i = 0; i < r.value().mesh.structure.nodes.size(); ++i)
        if (std::abs(r.value().mesh.structure.nodes[i].x() - length) < 1e-12) {
            sum += -r.value().mesh.structure.translation(r.value().displacement, i).z();
            ++n;
        }
    return sum / n;
}

}  // namespace

TEST(MaterialFields, FramesGiveCoordinatesAndOrthonormalBases)
{
    FieldFrame c;
    c.kind = FieldFrame::Kind::Cylindrical;
    const Vec3 q = c.coordinates(Vec3(0, 2, 3));
    EXPECT_NEAR(q.x(), 2.0, 1e-15);
    EXPECT_NEAR(q.y(), kPi / 2, 1e-15);
    EXPECT_NEAR(q.z(), 3.0, 1e-15);
    const Eigen::Matrix3d b = c.basis(Vec3(0, 2, 3));
    EXPECT_TRUE((b.transpose() * b).isApprox(Eigen::Matrix3d::Identity(), 1e-14));
    EXPECT_NEAR(b.determinant(), 1.0, 1e-14);   // right-handed
    EXPECT_TRUE(b.col(0).isApprox(Vec3(0, 1, 0), 1e-14));  // e_r along +y there
    FieldFrame s;
    s.kind = FieldFrame::Kind::Spherical;
    const Vec3 p(1, 1, std::sqrt(2.0));
    const Vec3 qs = s.coordinates(p);
    EXPECT_NEAR(qs.x(), 2.0, 1e-14);
    EXPECT_NEAR(qs.y(), kPi / 4, 1e-14);
    EXPECT_NEAR(qs.z(), kPi / 4, 1e-14);
    const Eigen::Matrix3d bs = s.basis(p);
    EXPECT_TRUE((bs.transpose() * bs).isApprox(Eigen::Matrix3d::Identity(), 1e-14));
    EXPECT_NEAR(bs.determinant(), 1.0, 1e-14);
    EXPECT_TRUE(bs.col(0).isApprox(p.normalized(), 1e-14));
}

TEST(MaterialFields, AnAnalyticBlendFollowsItsProfileAndIsNotExtrapolated)
{
    MaterialField f;
    f.id = "k";
    f.property = "thermal.conductivity";
    f.rank = FieldRank::Scalar;
    f.value0 = {10.0};
    f.value1 = {30.0};
    f.coordinate = 2;
    f.q0 = 0.0;
    f.q1 = 0.01;
    ASSERT_TRUE(f.validate().ok()) << f.validate().message();
    EXPECT_NEAR(f.componentsAt(Vec3(0, 0, 0.005)).value()(0), 20.0, 1e-12);
    f.profile = "power-law";
    f.exponent = 2.0;
    EXPECT_NEAR(f.componentsAt(Vec3(0, 0, 0.005)).value()(0), 15.0, 1e-12);
    EXPECT_FALSE(f.componentsAt(Vec3(0, 0, 0.02)).ok());  // beyond q1: refused, not extrapolated
    // The JSON form round-trips.
    auto back = MaterialField::fromJson(f.toJson());
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_NEAR(back.value().componentsAt(Vec3(0, 0, 0.0025)).value()(0), f.componentsAt(Vec3(0, 0, 0.0025)).value()(0), 1e-15);
    // A stiffness that is not positive definite is refused, never repaired.
    MaterialField bad;
    bad.id = "bad";
    bad.property = "mechanical.stiffness";
    bad.rank = FieldRank::Stiffness6;
    bad.profile = "constant";
    bad.value0 = isotropicStiffness(100e9, 0.3);
    bad.value0[0] = -1e9;
    const Status s = bad.validate();
    ASSERT_FALSE(s.ok());
    EXPECT_NE(s.message().find("not positive definite"), std::string::npos) << s.message();
    // A rank the property does not take is refused.
    MaterialField wrong = f;
    wrong.property = "mechanical.density";
    wrong.rank = FieldRank::Symmetric3;
    EXPECT_FALSE(wrong.validate().ok());
}

TEST(MaterialFields, SamplesAreInterpolatedWithinTheirSupportOnly)
{
    MaterialField f;
    f.id = "rho";
    f.property = "mechanical.density";
    f.form = "sampled";
    f.source = "measured";
    f.provenance = "test samples";
    f.interpolation = "linear-1d";
    f.coordinate = 0;
    f.samples = {{Vec3(0, 0, 0), {1000.0}, {5.0}}, {Vec3(1, 0, 0), {2000.0}, {5.0}}, {Vec3(2, 0, 0), {4000.0}, {5.0}}};
    ASSERT_TRUE(f.validate().ok()) << f.validate().message();
    EXPECT_NEAR(f.componentsAt(Vec3(0.5, 7, 7)).value()(0), 1500.0, 1e-12);  // along x only
    EXPECT_NEAR(f.componentsAt(Vec3(1.5, 0, 0)).value()(0), 3000.0, 1e-12);
    EXPECT_FALSE(f.componentsAt(Vec3(2.5, 0, 0)).ok());
    // Scattered data: inverse-distance weights within the support; nothing beyond it.
    f.interpolation = "idw";
    EXPECT_FALSE(f.validate().ok());  // no support given
    f.support = 0.8;
    ASSERT_TRUE(f.validate().ok());
    EXPECT_NEAR(f.componentsAt(Vec3(1, 0, 0)).value()(0), 2000.0, 1e-9);
    const double v = f.componentsAt(Vec3(0.5, 0, 0)).value()(0);
    EXPECT_NEAR(v, 1500.0, 1e-9);  // equal weights of the two nearest samples
    EXPECT_FALSE(f.componentsAt(Vec3(5, 0, 0)).ok());
    f.interpolation = "nearest";
    EXPECT_NEAR(f.componentsAt(Vec3(1.9, 0, 0)).value()(0), 4000.0, 1e-12);
}

TEST(MaterialFields, ComponentsInACylindricalBasisRotateIntoTheObjectsAxes)
{
    // A radially orthotropic conductivity (k_rr, k_tt, k_zz) given in (e_r, e_theta, e_z).
    MaterialField f;
    f.id = "k";
    f.property = "thermal.conductivity";
    f.rank = FieldRank::Symmetric3;
    f.frame.kind = FieldFrame::Kind::Cylindrical;
    f.frameBasis = true;
    f.profile = "constant";
    f.value0 = {5.0, 1.0, 2.0, 0.0, 0.0, 0.0};
    ASSERT_TRUE(f.validate().ok()) << f.validate().message();
    const Eigen::MatrixXd onY = f.tensorAt(Vec3(0, 3, 0)).value();  // e_r = +y, e_theta = -x
    EXPECT_NEAR(onY(0, 0), 1.0, 1e-14);
    EXPECT_NEAR(onY(1, 1), 5.0, 1e-14);
    EXPECT_NEAR(onY(2, 2), 2.0, 1e-14);
    const Eigen::MatrixXd diagonal = f.tensorAt(Vec3(1, 1, 0)).value();  // at 45 degrees
    EXPECT_NEAR(diagonal(0, 1), 2.0, 1e-13);                              // (k_rr - k_tt) / 2
    EXPECT_NEAR(diagonal.trace(), 8.0, 1e-13);                             // invariant
}

TEST(MaterialFields, AStiffnessFieldIsTheSameBodyAsItsMaterial)
{
    const double length = 0.1, e = 70e9, nu = 0.3;
    LayeredBody plain;
    plain.surface.length = length;
    plain.surface.width = 0.01;
    plain.layers = {layer(e, nu, 2700, 0.004, 2)};
    plain.elementsU = 10;
    plain.elementsV = 1;
    LayeredBody field = plain;
    MaterialField c;
    c.id = "c";
    c.property = "mechanical.stiffness";
    c.rank = FieldRank::Stiffness6;
    c.profile = "constant";
    c.value0 = isotropicStiffness(e, nu);
    field.layers[0].material.fields = {c};
    EXPECT_NEAR(tipOf(field, length), tipOf(plain, length), 1e-12 * tipOf(plain, length));
    // A graded field through the thickness equals the graded layer (both element by element).
    LayeredBody graded = plain;
    graded.layers = {layer(70e9, 0.0, 2700, 0.004, 6)};
    ElasticMaterial steel;
    steel.name = "steel";
    steel.youngsModulus = 200e9;
    steel.poissonsRatio = 0.0;
    steel.density = 7800;
    graded.layers[0].material.graded = steel;
    LayeredBody fieldGraded = plain;
    fieldGraded.layers = {layer(70e9, 0.0, 2700, 0.004, 6)};
    MaterialField g;
    g.id = "g";
    g.property = "mechanical.stiffness";
    g.rank = FieldRank::Stiffness6;
    g.value0 = isotropicStiffness(70e9, 0.0);
    g.value1 = isotropicStiffness(200e9, 0.0);
    g.coordinate = 2;
    g.q0 = 0.0;
    g.q1 = 0.004;
    fieldGraded.layers[0].material.fields = {g};
    EXPECT_NEAR(tipOf(fieldGraded, length), tipOf(graded, length), 1e-8 * tipOf(graded, length));
    // A stiffness field together with an orthotropic or graded description is refused.
    graded.layers[0].material.fields = {g};
    EXPECT_FALSE(graded.validate().ok());
}

TEST(MaterialFields, AnAnisotropicConductivityConductsAlongItsOwnAxis)
{
    LayeredBody body;
    body.surface.length = 0.05;
    body.surface.width = 0.05;
    BodyLayer l = layer(70e9, 0.3, 2700, 0.01, 3);
    MaterialField k;
    k.id = "k";
    k.property = "thermal.conductivity";
    k.rank = FieldRank::Symmetric3;
    k.profile = "constant";
    k.value0 = {100.0, 50.0, 2.0, 0.0, 0.0, 0.0};  // k_xx, k_yy, k_zz
    l.material.fields = {k};
    body.layers = {l};
    body.elementsU = 2;
    body.elementsV = 2;
    LayeredThermalBoundary bc;
    bc.innerTemperature = 400;
    bc.outerTemperature = 300;
    auto r = solveLayeredConduction(body, bc);
    ASSERT_TRUE(r.ok()) << r.message();
    EXPECT_NEAR(r.value().heatIn, 2.0 * 100 / 0.01 * 0.05 * 0.05, 1e-9 * 2.0 * 100 / 0.01 * 0.05 * 0.05);  // k_zz only
}
