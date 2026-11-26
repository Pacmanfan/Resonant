#include "geometry/Geometry.h"

#include "foundation/Units.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {
Geometry make(const char* kind)
{
    auto created = Geometry::create(kind);
    EXPECT_TRUE(created.ok()) << created.message();
    return created.take();
}

double meshArea(const TriangleMesh& mesh)
{
    double area = 0.0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vec3 a = mesh.positions[mesh.indices[i]].cast<double>();
        const Vec3 b = mesh.positions[mesh.indices[i + 1]].cast<double>();
        const Vec3 c = mesh.positions[mesh.indices[i + 2]].cast<double>();
        area += 0.5 * (b - a).cross(c - a).norm();
    }
    return area;
}
}  // namespace

TEST(Geometry, AllSevenPrimitivesAreRegistered)
{
    for (const char* kind : {"box", "sphere", "cylinder", "cone", "plane", "disc", "ellipsoid"}) {
        const GeometryKind* found = GeometryRegistry::instance().find(kind);
        ASSERT_NE(found, nullptr) << kind;
        EXPECT_EQ(found->category, "primitive");
        EXPECT_FALSE(found->parameters.empty());
    }
    EXPECT_EQ(GeometryRegistry::instance().kindsInCategory("primitive").size(), 7u);
}

TEST(Geometry, UnknownKindIsRejected)
{
    const auto created = Geometry::create("teapot");
    EXPECT_FALSE(created.ok());
    EXPECT_NE(created.message().find("teapot"), std::string::npos);
}

TEST(Geometry, DefaultsAreValid)
{
    for (const GeometryKind* kind : GeometryRegistry::instance().kinds()) {
        const Geometry geometry = make(kind->id.c_str());
        EXPECT_TRUE(geometry.validate().ok()) << kind->id;
        EXPECT_FALSE(geometry.tessellate().empty()) << kind->id;
    }
}

TEST(Geometry, ParameterValidationRejectsBadValuesAndKeepsTheOldOne)
{
    Geometry box = make("box");
    EXPECT_TRUE(box.setParameter("width", 2.5).ok());
    EXPECT_DOUBLE_EQ(box.parameter("width"), 2.5);

    EXPECT_FALSE(box.setParameter("width", 0.0).ok());   // must be > 0
    EXPECT_FALSE(box.setParameter("width", -1.0).ok());
    EXPECT_FALSE(box.setParameter("width", std::nan("")).ok());
    EXPECT_FALSE(box.setParameter("width", std::numeric_limits<double>::infinity()).ok());
    EXPECT_FALSE(box.setParameter("radius", 1.0).ok());  // a box has no radius
    EXPECT_DOUBLE_EQ(box.parameter("width"), 2.5);
}

TEST(Geometry, BoxMeshMatchesItsDimensions)
{
    Geometry box = make("box");
    ASSERT_TRUE(box.setParameter("width", 2.0).ok());
    ASSERT_TRUE(box.setParameter("depth", 4.0).ok());
    ASSERT_TRUE(box.setParameter("height", 6.0).ok());
    const TriangleMesh mesh = box.tessellate();
    const Aabb bounds = mesh.bounds();
    EXPECT_TRUE(bounds.min.isApprox(Vec3(-1.0, -2.0, -3.0), 1e-6));
    EXPECT_TRUE(bounds.max.isApprox(Vec3(1.0, 2.0, 3.0), 1e-6));
    EXPECT_EQ(mesh.triangleCount(), 12u);
    EXPECT_NEAR(meshArea(mesh), 2.0 * (2 * 4 + 2 * 6 + 4 * 6), 1e-4);
}

TEST(Geometry, SphereMeshLiesOnTheSphereWithOutwardNormals)
{
    Geometry sphere = make("sphere");
    ASSERT_TRUE(sphere.setParameter("radius", 1.5).ok());
    TessellationOptions options;
    options.segments = 64;
    const TriangleMesh mesh = sphere.tessellate(options);
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        EXPECT_NEAR(mesh.positions[i].norm(), 1.5f, 1e-5f);
        EXPECT_GT(mesh.positions[i].normalized().dot(mesh.normals[i]), 0.999f);
    }
    // The tessellated area converges to 4 pi r^2 from below.
    const double exact = 4.0 * units::pi * 1.5 * 1.5;
    EXPECT_LT(meshArea(mesh), exact);
    EXPECT_NEAR(meshArea(mesh), exact, 0.01 * exact);
}

TEST(Geometry, TriangleWindingAgreesWithVertexNormals)
{
    for (const GeometryKind* kind : GeometryRegistry::instance().kinds()) {
        const TriangleMesh mesh = make(kind->id.c_str()).tessellate();
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
            const Eigen::Vector3f face = (mesh.positions[b] - mesh.positions[a]).cross(mesh.positions[c] - mesh.positions[a]);
            const Eigen::Vector3f normal = mesh.normals[a] + mesh.normals[b] + mesh.normals[c];
            ASSERT_GE(face.dot(normal), 0.0f) << kind->id << " triangle " << i / 3;
        }
    }
}

TEST(Geometry, CylinderAndConeAndDiscAreas)
{
    TessellationOptions fine;
    fine.segments = 256;

    Geometry cylinder = make("cylinder");
    ASSERT_TRUE(cylinder.setParameter("radius", 0.5).ok());
    ASSERT_TRUE(cylinder.setParameter("height", 2.0).ok());
    const double cylinderArea = 2.0 * units::pi * 0.5 * 2.0 + 2.0 * units::pi * 0.25;
    EXPECT_NEAR(meshArea(cylinder.tessellate(fine)), cylinderArea, 1e-3 * cylinderArea);

    Geometry cone = make("cone");
    ASSERT_TRUE(cone.setParameter("radius", 3.0).ok());
    ASSERT_TRUE(cone.setParameter("height", 4.0).ok());
    const double coneArea = units::pi * 3.0 * 5.0 + units::pi * 9.0;  // slant height 5
    EXPECT_NEAR(meshArea(cone.tessellate(fine)), coneArea, 1e-3 * coneArea);
    const Aabb coneBounds = cone.tessellate(fine).bounds();
    EXPECT_NEAR(coneBounds.min.z(), -2.0, 1e-6);
    EXPECT_NEAR(coneBounds.max.z(), 2.0, 1e-6);

    Geometry disc = make("disc");
    ASSERT_TRUE(disc.setParameter("radius", 2.0).ok());
    EXPECT_NEAR(meshArea(disc.tessellate(fine)), units::pi * 4.0, 1e-3 * units::pi * 4.0);
}

TEST(Geometry, EllipsoidBoundsAreItsSemiAxes)
{
    Geometry ellipsoid = make("ellipsoid");
    ASSERT_TRUE(ellipsoid.setParameter("radiusX", 3.0).ok());
    ASSERT_TRUE(ellipsoid.setParameter("radiusY", 2.0).ok());
    ASSERT_TRUE(ellipsoid.setParameter("radiusZ", 1.0).ok());
    const Aabb bounds = ellipsoid.tessellate().bounds();
    EXPECT_TRUE(bounds.max.isApprox(Vec3(3.0, 2.0, 1.0), 1e-5));
    EXPECT_TRUE(bounds.min.isApprox(Vec3(-3.0, -2.0, -1.0), 1e-5));
}

TEST(Geometry, RayIntersectionFindsTheNearestHitInWorldSpace)
{
    Geometry box = make("box");  // unit cube centred on the origin
    const TriangleMesh mesh = box.tessellate();
    Mat4 model = Mat4::Identity();
    model(0, 3) = 5.0;  // cube centred at x = 5

    const auto hit = intersect(Ray{Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0)}, mesh, model);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 4.5, 1e-9);

    EXPECT_FALSE(intersect(Ray{Vec3(0.0, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0)}, mesh, model).has_value());
    EXPECT_FALSE(intersect(Ray{Vec3(0.0, 2.0, 0.0), Vec3(1.0, 0.0, 0.0)}, mesh, model).has_value());
}
