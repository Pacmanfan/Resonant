#pragma once

// Simulation geometry for grid solvers: which points lie inside the body of
// a parametric shape.
//
// This is derived from the shape's mathematical definition, never from its
// display mesh. Shells and plates thinner than a solver can represent are
// thickened to `minThickness`, and the returned note says so.
#include "geometry/Geometry.h"

#include <functional>
#include <optional>

namespace resonant::fdtd {

struct SolidModel {
    // True when the point (in the shape's local frame, metres) is inside the body.
    std::function<bool(const Vec3&)> contains;
    // Radius of a sphere around the local origin that encloses the body.
    double boundingRadius = 0.0;
    // Not empty when the simulation geometry differs from the design.
    std::string note;
};

// Empty when no solid model exists for the geometry kind.
std::optional<SolidModel> solidModelOf(const Geometry& geometry, double minThickness);

}  // namespace resonant::fdtd
