// A local transform: translation, rotation, scale.
//
// Conventions (ADR-000205):
//   * position in metres;
//   * rotation as intrinsic Z-Y-X Euler angles in radians, stored as
//     [x, y, z] = [roll about X, pitch about Y, yaw about Z]. The rotation
//     matrix is R = Rz(z) * Ry(y) * Rx(x): a point is rolled, then pitched,
//     then yawed, about the fixed parent axes;
//   * scale is a per-axis factor applied before rotation;
//   * matrix() = T * R * S maps local coordinates to the parent's.
#pragma once

#include "foundation/Types.h"

namespace resonant {

struct Transform {
    Vec3 position = Vec3::Zero();
    Vec3 rotation = Vec3::Zero();
    Vec3 scale = Vec3::Ones();

    Mat3 rotationMatrix() const;
    Mat4 matrix() const;

    friend bool operator==(const Transform& a, const Transform& b)
    {
        return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
    }
};

// Transforms a point (w = 1) or a direction (w = 0) by a 4x4 matrix.
Vec3 transformPoint(const Mat4& matrix, const Vec3& point);
Vec3 transformDirection(const Mat4& matrix, const Vec3& direction);

}  // namespace resonant
