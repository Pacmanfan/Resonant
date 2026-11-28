// Shared numeric types and the physics-domain identifier.
#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <complex>
#include <optional>
#include <string>
#include <string_view>

namespace resonant {

using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Mat4 = Eigen::Matrix4d;
using Complex = std::complex<double>;

// A physical wave domain. Each domain has its own material property set and
// its own solvers; nothing is shared between domains except the scene.
enum class PhysicsDomain { Acoustic, Mechanical, Electromagnetic, Thermal };

std::string_view toString(PhysicsDomain domain);
std::optional<PhysicsDomain> physicsDomainFromString(std::string_view text);

}  // namespace resonant
