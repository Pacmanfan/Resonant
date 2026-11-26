// The simulation representation of a reflecting surface: small flat
// elements with a position, a unit normal and an area, generated from the
// parametric geometry (never from the display mesh) at a resolution chosen
// for the wavelength.
//
// Local frames of the reflector kinds (the reflecting side faces +Z):
//   reflector.parabolic    z = r^2 / (4 f); vertex at the origin, axis +Z,
//                          focus at (0, 0, f)
//   reflector.spherical    z = R - sqrt(R^2 - r^2); vertex at the origin,
//                          centre of curvature at (0, 0, R), paraxial focus (0, 0, R/2)
//   reflector.elliptical   prolate spheroid r^2/b^2 + z^2/a^2 = 1 about the Z
//                          axis, centre at the origin, kept for z <= cut * a;
//                          foci at (0, 0, -c) and (0, 0, +c), c = sqrt(a^2 - b^2)
//   reflector.cylindrical  parabolic trough z = x^2 / (4 f), extruded along Y;
//                          focal line x = 0, z = f
//   reflector.flat         the rectangle |x| <= width/2, |y| <= height/2 in z = 0
#pragma once

#include "geometry/Geometry.h"

namespace resonant::manipulators {

struct SurfaceElement {
    Vec3 position = Vec3::Zero();
    Vec3 normal = Vec3::UnitZ();  // unit, towards the reflecting (+Z / concave) side
    double area = 0.0;            // m^2
};

// True for the geometry kinds that are reflecting surfaces.
bool isReflectorGeometry(const Geometry& geometry);

// Surface elements in the geometry's local frame, none larger than
// maxElementSize in either direction.
std::vector<SurfaceElement> sampleReflectorSurface(const Geometry& geometry, double maxElementSize);

// The number of elements sampleReflectorSurface would produce, without
// producing them (for preflight).
size_t estimateSurfaceElements(const Geometry& geometry, double maxElementSize);

// Exact surface area of the reflecting side (closed form or fine quadrature),
// for checking the sampling.
double reflectorArea(const Geometry& geometry);

// Geometric points of a reflector in its local frame (focus, foci, centre of
// curvature), with names, for display and for design intent.
struct NamedPoint {
    std::string name;
    Vec3 position;
};
std::vector<NamedPoint> characteristicPoints(const Geometry& geometry);

// The surface height z and unit normal at a point of the axisymmetric
// reflectors, given its distance r from the axis (parabolic, spherical) -
// used by the ray tracer and by tests.
struct SurfacePoint {
    Vec3 position;
    Vec3 normal;
};

// One specular reflection of a ray on a reflector in its local frame.
// Returns the hit point, the surface normal there and the reflected
// direction, or nullopt when the ray misses. Geometrical acoustics: valid
// when the reflector and its curvature radii are large against the
// wavelength; no diffraction, no interference.
struct RayHit {
    Vec3 point;
    Vec3 normal;
    Vec3 reflected;  // unit
    double distance = 0.0;
};
std::optional<RayHit> reflectRay(const Geometry& geometry, const Ray& ray);

}  // namespace resonant::manipulators
