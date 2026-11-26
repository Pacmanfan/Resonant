#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace resonant::render {

namespace {
constexpr double kMaxPitch = 1.5533430342749532;  // 89 degrees
constexpr double kMinDistance = 1e-4;
constexpr double kMaxDistance = 1e7;
}  // namespace

Vec3 Camera::eye() const
{
    const double cp = std::cos(pitch);
    return target + distance * Vec3(cp * std::cos(yaw), cp * std::sin(yaw), std::sin(pitch));
}

Vec3 Camera::forward() const
{
    return (target - eye()).normalized();
}

Vec3 Camera::right() const
{
    return forward().cross(Vec3::UnitZ()).normalized();
}

Vec3 Camera::up() const
{
    return right().cross(forward()).normalized();
}

double Camera::nearPlane() const
{
    return std::max(1e-6, distance * 0.01);
}

double Camera::farPlane() const
{
    return distance * 1000.0;
}

Eigen::Matrix4d Camera::viewMatrix() const
{
    const Vec3 f = forward();
    const Vec3 r = right();
    const Vec3 u = up();
    const Vec3 e = eye();
    Eigen::Matrix4d view = Eigen::Matrix4d::Identity();
    view.block<1, 3>(0, 0) = r.transpose();
    view.block<1, 3>(1, 0) = u.transpose();
    view.block<1, 3>(2, 0) = -f.transpose();
    view(0, 3) = -r.dot(e);
    view(1, 3) = -u.dot(e);
    view(2, 3) = f.dot(e);
    return view;
}

Eigen::Matrix4d Camera::projectionMatrix(double aspect) const
{
    const double n = nearPlane();
    const double f = farPlane();
    const double t = 1.0 / std::tan(0.5 * verticalFov);
    Eigen::Matrix4d projection = Eigen::Matrix4d::Zero();
    projection(0, 0) = t / std::max(aspect, 1e-9);
    projection(1, 1) = t;
    projection(2, 2) = (f + n) / (n - f);
    projection(2, 3) = 2.0 * f * n / (n - f);
    projection(3, 2) = -1.0;
    return projection;
}

void Camera::orbit(double deltaYaw, double deltaPitch)
{
    yaw = std::remainder(yaw + deltaYaw, 2.0 * 3.14159265358979323846);
    pitch = std::clamp(pitch + deltaPitch, -kMaxPitch, kMaxPitch);
}

void Camera::pan(double dx, double dy)
{
    // World size of the viewport height at the target's depth.
    const double height = 2.0 * distance * std::tan(0.5 * verticalFov);
    target += (-dx * height) * right() + (dy * height) * up();
}

void Camera::zoom(double factor)
{
    distance = std::clamp(distance * factor, kMinDistance, kMaxDistance);
}

void Camera::frame(const Aabb& box)
{
    if (!box.isValid())
        return;
    target = box.center();
    const double radius = std::max(0.5 * box.size().norm(), 1e-3);
    distance = std::clamp(radius / std::sin(0.5 * verticalFov) * 1.1, kMinDistance, kMaxDistance);
}

void Camera::reset()
{
    *this = Camera();
}

Ray Camera::pickRay(double ndcX, double ndcY, double aspect) const
{
    const Eigen::Matrix4d inverse = (projectionMatrix(aspect) * viewMatrix()).inverse();
    const auto unproject = [&inverse](double x, double y, double z) {
        const Eigen::Vector4d p = inverse * Eigen::Vector4d(x, y, z, 1.0);
        return Vec3(p.head<3>() / p.w());
    };
    const Vec3 nearPoint = unproject(ndcX, ndcY, -1.0);
    const Vec3 farPoint = unproject(ndcX, ndcY, 1.0);
    return Ray{nearPoint, (farPoint - nearPoint).normalized()};
}

}  // namespace resonant::render
