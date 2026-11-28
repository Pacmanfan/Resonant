#pragma once

// Spatially varying, tensor-valued material fields (REQ-CHAT-20261128-006, RES-FAM-021,
// ADR-000234).
//
// A field gives one property of a region as a function of position. Properties:
// - density;
// - the elastic stiffness;
// - the thermal and the electrical conductivity;
// - the permittivity;
// - the piezoelectric coupling;
// - the magnetic permeability.
// Every field states:
// - its coordinate system: Cartesian object axes, cylindrical (r, theta, z) or
//   spherical (r, polar, azimuth) about an origin and an axis;
// - its tensor representation: a scalar; a symmetric 3x3 tensor as
//   [xx yy zz xy yz zx]; a 6x6 Voigt stiffness in the mechanics order
//   (xx yy zz xy yz zx, engineering shear) as its 21 upper entries row by row;
//   or a 3x6 coupling, row-major;
// - whether its components are in the frame's local basis (e_r, e_theta, e_z ...)
//   or in the object's axes;
// - its form, analytic or sampled, and, separately, its source: analytic, measured,
//   imported or computed. A computed constant is analytic in form; measured samples
//   are sampled.
//
// Analytic fields blend two tensors along one coordinate of the frame:
// T = T0 + (T1 - T0) f(q), with f linear, a power law, an exponential or a table over
// q normalized to [q0, q1].
//
// Sampled fields interpolate their samples: nearest, inverse-distance weighting, or
// piecewise linear along one coordinate. All three are convex combinations, so they
// keep positive-definite tensors positive definite. A position beyond the samples'
// support is refused, never extrapolated silently. Invalid tensors (not symmetric
// where they must be, not positive definite) are refused, never repaired.

#include "foundation/Result.h"
#include "foundation/Types.h"

#include <Eigen/Core>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace resonant::materials {

enum class FieldRank { Scalar, Symmetric3, Stiffness6, Coupling36 };
std::string_view toString(FieldRank rank);
std::optional<FieldRank> fieldRankFromString(std::string_view text);
int componentCount(FieldRank rank);  // 1, 6, 21, 18

// The property keys and the rank each takes.
struct FieldProperty {
    std::string key, title, unit;
    std::vector<FieldRank> ranks;
    bool positiveDefinite = true;  // its tensor must be positive definite (scalars: positive)
};
const std::vector<FieldProperty>& fieldProperties();
const FieldProperty* findFieldProperty(std::string_view key);

struct FieldFrame {
    enum class Kind { Cartesian, Cylindrical, Spherical };
    Kind kind = Kind::Cartesian;
    Vec3 origin = Vec3::Zero();
    Vec3 axis = Vec3::UnitZ();       // cylindrical and spherical: the z axis (polar axis)
    Vec3 reference = Vec3::UnitX();  // the direction of theta = 0 (azimuth 0)
    // The frame's coordinates of a point: (x, y, z), (r, theta, z) or (r, polar, azimuth).
    Vec3 coordinates(const Vec3& point) const;
    // The local orthonormal basis at a point, as columns: (x, y, z), (e_r, e_theta, e_z) or (e_r, e_polar, e_azimuth).
    Eigen::Matrix3d basis(const Vec3& point) const;
    std::string describe() const;
};

struct FieldSample {
    Vec3 at = Vec3::Zero();             // in the frame's coordinates
    std::vector<double> components;
    std::vector<double> uncertainty;    // standard, per component (may be empty)
};

struct MaterialField {
    std::string id, region, property;  // property: a key of fieldProperties()
    FieldRank rank = FieldRank::Scalar;
    std::string unit;
    FieldFrame frame;
    bool frameBasis = false;           // components in the frame's local basis (else in the object's axes)
    std::string form = "analytic";     // analytic (a blend of two tensors) or sampled (interpolated samples)
    std::string source = "analytic";   // where the values come from: analytic, measured, imported, computed
    std::string provenance = "unstated";
    // Analytic.
    std::vector<double> value0, value1;  // at q0 and q1
    int coordinate = 2;                  // which frame coordinate the blend follows
    double q0 = 0.0, q1 = 1.0;
    std::string profile = "linear";      // constant, linear, power-law, exponential, table
    double exponent = 1.0;               // power-law: s^exponent; exponential: (e^(exponent s) - 1) / (e^exponent - 1)
    std::vector<std::pair<double, double>> table;  // (s, f), s increasing in [0, 1]
    // Sampled (measured, imported, computed).
    std::vector<FieldSample> samples;
    std::string interpolation = "linear-1d";  // nearest, idw, linear-1d
    double support = 0.0;                     // nearest and idw: the largest distance (m) from a sample at which a value is given

    Status validate() const;
    // The components at a point, in the representation's order and the stated basis.
    Result<Eigen::VectorXd> componentsAt(const Vec3& point) const;
    // The tensor at a point in the object's axes: 1x1, 3x3, 6x6 or 3x6.
    Result<Eigen::MatrixXd> tensorAt(const Vec3& point) const;
    nlohmann::json toJson() const;
    static Result<MaterialField> fromJson(const nlohmann::json& j);
};

// The tensor of a representation's components (in the components' own basis).
Eigen::MatrixXd tensorOf(FieldRank rank, const Eigen::VectorXd& components);
// The Bond stress-transformation matrix of a rotation whose columns are the new axes in the old ones.
Eigen::Matrix<double, 6, 6> bondMatrix(const Eigen::Matrix3d& q);

}  // namespace resonant::materials
