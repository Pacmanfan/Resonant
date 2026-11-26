// An orbit camera for the engineering viewport. Pure mathematics (no
// OpenGL calls), so it is testable without a context.
//
// World axes: X horizontal, Y depth, Z up. The camera looks at `target`
// from a distance; `yaw` is the azimuth of the eye about the Z axis measured
// from +X towards +Y, `pitch` its elevation above the XY plane.
#pragma once

#include "geometry/TriangleMesh.h"

#include <Eigen/Core>

namespace resonant::render {

class Camera {
public:
    Vec3 target = Vec3::Zero();
    double distance = 8.0;                  // metres from eye to target
    double yaw = -1.0471975511965976;       // -60 degrees
    double pitch = 0.4363323129985824;      // 25 degrees
    double verticalFov = 0.7853981633974483; // 45 degrees

    Vec3 eye() const;
    Vec3 forward() const;  // unit vector from the eye to the target
    Vec3 right() const;
    Vec3 up() const;

    double nearPlane() const;
    double farPlane() const;

    Eigen::Matrix4d viewMatrix() const;
    Eigen::Matrix4d projectionMatrix(double aspect) const;

    // Rotate about the target; angles in radians. Pitch is kept short of
    // the poles.
    void orbit(double deltaYaw, double deltaPitch);
    // Move the target in the view plane; dx, dy as fractions of the
    // viewport height (so a drag follows the cursor at the target's depth).
    void pan(double dx, double dy);
    // factor < 1 moves closer.
    void zoom(double factor);
    // Look at a box so that it fills the view.
    void frame(const Aabb& box);
    void reset();

    // The world-space ray through a point of the viewport given in
    // normalized device coordinates (x, y in [-1, 1], y up).
    Ray pickRay(double ndcX, double ndcY, double aspect) const;
};

}  // namespace resonant::render
