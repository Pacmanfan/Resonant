#include "foundation/Transform.h"

namespace resonant {

Mat3 Transform::rotationMatrix() const
{
    const Eigen::AngleAxisd roll(rotation.x(), Vec3::UnitX());
    const Eigen::AngleAxisd pitch(rotation.y(), Vec3::UnitY());
    const Eigen::AngleAxisd yaw(rotation.z(), Vec3::UnitZ());
    return (yaw * pitch * roll).toRotationMatrix();
}

Mat4 Transform::matrix() const
{
    Mat4 result = Mat4::Identity();
    result.block<3, 3>(0, 0) = rotationMatrix() * scale.asDiagonal();
    result.block<3, 1>(0, 3) = position;
    return result;
}

Vec3 transformPoint(const Mat4& matrix, const Vec3& point)
{
    return matrix.block<3, 3>(0, 0) * point + matrix.block<3, 1>(0, 3);
}

Vec3 transformDirection(const Mat4& matrix, const Vec3& direction)
{
    return matrix.block<3, 3>(0, 0) * direction;
}

}  // namespace resonant
