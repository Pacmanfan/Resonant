#pragma once

// Geometry kinds for mechanical wave manipulation, along local Z from
// -length/2 to +length/2, both circular in cross-section:
//
//   rod.horn      a velocity transformer: conical (linear radius),
//                 exponential or stepped from the input to the output radius.
//   rod.periodic  alternating segments of two radii, repeated: a 1D
//                 phononic structure.
#include "geometry/Geometry.h"

namespace resonant::mechanics {

enum class HornProfile { Conical = 0, Exponential = 1, Stepped = 2 };

// Radius at a fraction s in [0, 1] of the length, from the start (-Z end).
double rodRadiusAt(const Geometry& geometry, double s);
bool isRodGeometry(const Geometry& geometry);

}  // namespace resonant::mechanics

namespace resonant {
void registerMechanicalGeometry(GeometryRegistry& registry);
}
