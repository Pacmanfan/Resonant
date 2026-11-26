#include "render/Camera.h"
#include "render/Layers.h"

#include "foundation/Units.h"

#include <gtest/gtest.h>

using namespace resonant;
using render::Camera;

TEST(Camera, EyeIsAtTheRequestedDistanceLookingAtTheTarget)
{
    Camera camera;
    camera.target = Vec3(1.0, 2.0, 3.0);
    camera.distance = 5.0;
    EXPECT_NEAR((camera.eye() - camera.target).norm(), 5.0, 1e-12);
    EXPECT_TRUE(camera.forward().isApprox((camera.target - camera.eye()).normalized(), 1e-12));
    // Z is up: with positive pitch the eye is above the target.
    EXPECT_GT(camera.eye().z(), camera.target.z());
    EXPECT_GT(camera.up().z(), 0.0);
}

TEST(Camera, ViewMatrixPutsTheTargetOnTheNegativeViewAxis)
{
    Camera camera;
    camera.target = Vec3(0.5, -1.0, 2.0);
    const Eigen::Vector4d target = camera.viewMatrix() * Eigen::Vector4d(0.5, -1.0, 2.0, 1.0);
    EXPECT_NEAR(target.x(), 0.0, 1e-12);
    EXPECT_NEAR(target.y(), 0.0, 1e-12);
    EXPECT_NEAR(target.z(), -camera.distance, 1e-12);
}

TEST(Camera, PickRayThroughTheCentreHitsTheTarget)
{
    Camera camera;
    camera.target = Vec3(2.0, 1.0, 0.0);
    const Ray ray = camera.pickRay(0.0, 0.0, 16.0 / 9.0);
    // The centre ray passes through the target.
    const Vec3 toTarget = camera.target - ray.origin;
    EXPECT_NEAR(toTarget.cross(ray.direction).norm(), 0.0, 1e-6);
    EXPECT_GT(toTarget.dot(ray.direction), 0.0);
}

TEST(Camera, PickRayAtTheEdgeMatchesTheFieldOfView)
{
    Camera camera;
    const Ray top = camera.pickRay(0.0, 1.0, 1.0);
    const double angle = std::acos(top.direction.normalized().dot(camera.forward()));
    EXPECT_NEAR(angle, 0.5 * camera.verticalFov, 1e-6);
}

TEST(Camera, OrbitKeepsDistanceAndClampsPitch)
{
    Camera camera;
    camera.orbit(1.0, 10.0);  // far past the pole
    EXPECT_LT(camera.pitch, units::pi / 2.0);
    EXPECT_NEAR((camera.eye() - camera.target).norm(), camera.distance, 1e-12);
    camera.orbit(0.0, -20.0);
    EXPECT_GT(camera.pitch, -units::pi / 2.0);
}

TEST(Camera, PanMovesTheTargetInTheViewPlane)
{
    Camera camera;
    const Vec3 forward = camera.forward();
    const Vec3 before = camera.target;
    camera.pan(0.25, -0.1);
    EXPECT_NEAR((camera.target - before).dot(forward), 0.0, 1e-12);
    EXPECT_GT((camera.target - before).norm(), 0.0);
    EXPECT_TRUE(camera.forward().isApprox(forward, 1e-12));
}

TEST(Camera, ZoomScalesDistanceWithinLimits)
{
    Camera camera;
    camera.distance = 10.0;
    camera.zoom(0.5);
    EXPECT_DOUBLE_EQ(camera.distance, 5.0);
    for (int i = 0; i < 200; ++i)
        camera.zoom(0.1);
    EXPECT_GT(camera.distance, 0.0);
}

TEST(Camera, FrameCentresOnTheBoxAndContainsIt)
{
    Camera camera;
    Aabb box;
    box.extend(Vec3(10.0, 10.0, 0.0));
    box.extend(Vec3(12.0, 14.0, 2.0));
    camera.frame(box);
    EXPECT_TRUE(camera.target.isApprox(Vec3(11.0, 12.0, 1.0)));
    // Every corner projects inside the view.
    const Eigen::Matrix4d viewProjection = camera.projectionMatrix(1.0) * camera.viewMatrix();
    for (int corner = 0; corner < 8; ++corner) {
        const Eigen::Vector4d p = viewProjection
                                  * Eigen::Vector4d((corner & 1) ? 12.0 : 10.0, (corner & 2) ? 14.0 : 10.0,
                                                    (corner & 4) ? 2.0 : 0.0, 1.0);
        EXPECT_LT(std::abs(p.x() / p.w()), 1.0);
        EXPECT_LT(std::abs(p.y() / p.w()), 1.0);
        EXPECT_GT(p.w(), 0.0);
    }
    // An empty box changes nothing.
    const Camera before = camera;
    camera.frame(Aabb());
    EXPECT_DOUBLE_EQ(camera.distance, before.distance);
}

TEST(Grid, SpacingFollowsTheCameraDistanceInDecades)
{
    EXPECT_DOUBLE_EQ(render::GridLayer::spacingFor(8.0), 1.0);
    EXPECT_DOUBLE_EQ(render::GridLayer::spacingFor(80.0), 10.0);
    EXPECT_DOUBLE_EQ(render::GridLayer::spacingFor(0.08), 0.01);
}
