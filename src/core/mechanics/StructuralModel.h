#pragma once

// A linear finite-element model of one structure.
//
// One element formulation per model:
//
//   Frame     3D Euler-Bernoulli beam, 2 nodes, 6 dofs per node
//             (ux, uy, uz, rx, ry, rz): axial, torsion, bending in two planes.
//   Plate     Kirchhoff thin plate, rectangular 4-node element of Adini,
//             Clough and Melosh (ACM), 3 dofs per node (w, rotation about the
//             first plane axis, rotation about the second).
//   Membrane  pre-tensioned membrane, bilinear 4-node element, 1 dof per node
//             (w, out of plane).
//   Solid     3D linear elasticity, trilinear 8-node hexahedron, 3 dofs per
//             node (ux, uy, uz).
//   Solid20   3D linear elasticity, quadratic 20-node (serendipity)
//             hexahedron, 3 dofs per node: curved geometry to second order and
//             bending of thin layers without the trilinear element's shear
//             locking (mechanics/LayeredSolid builds layered bodies of it).
//
// Solid and Solid20 elements may carry their own (anisotropic) elasticity
// matrix and density instead of their material's isotropic values; Solid20
// models may contain zero-thickness interface elements: a spring layer between
// two coincident faces (REQ-CHAT-20261128-004, ADR-000232).
//
// Small displacements, small strains, linear elasticity.
// The formulations, their assumptions and their validation are documented
// in docs/mechanics/STRUCTURAL_MECHANICS.md. Nothing here knows about scenes:
// Idealization.h builds a model from a scene object.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "mechanics/ElasticMaterial.h"

#include <Eigen/Sparse>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace resonant::mechanics {

enum class ElementKind { Frame, Plate, Membrane, Solid, Solid20 };

std::string_view toString(ElementKind kind);
int dofsPerNode(ElementKind kind);

// Cross-section of a frame element.
struct Section {
    double area = 0.0;     // m^2
    double iy = 0.0;       // m^4, second moment about the local y axis (bending in the x-z plane)
    double iz = 0.0;       // m^4, about the local z axis (bending in the x-y plane)
    double torsion = 0.0;  // m^4, torsion constant J
    double polar = 0.0;    // m^4, polar second moment Iy + Iz (rotary inertia in torsion)
    double cy = 0.0;       // m, largest distance of a fibre from the neutral axis along local y
    double cz = 0.0;       // m, along local z
    bool circular = false; // bending stresses of the two planes combine as a vector sum
    std::string description;
};

// Nodes of the 20-node hexahedron: corners 0-7 as the 8-node one, then the
// edge midpoints 8-11 (edges 0-1, 1-2, 2-3, 3-0), 12-15 (4-5, 5-6, 6-7, 7-4) and
// 16-19 (0-4, 1-5, 2-6, 3-7). Natural coordinates of each node:
const std::array<std::array<int, 3>, 20>& solid20NaturalCoordinates();

// An elasticity matrix (Voigt order xx, yy, zz, xy, yz, zx, engineering shear
// strains) and density for one element, instead of its material's isotropic
// values: anisotropic, rotated or graded matter.
struct Constitutive {
    Eigen::Matrix<double, 6, 6> stiffness = Eigen::Matrix<double, 6, 6>::Zero();  // Pa
    double density = 0.0;  // kg/m^3
    std::string description;
};

struct Element {
    std::array<int, 20> nodes{};  // Frame: 2; Plate and Membrane: 4, counter-clockwise; Solid: 8; Solid20: 20
    int constitutive = -1;        // Solid, Solid20: index into StructuralModel::constitutive, or -1 for the material's
    int material = 0;
    int section = 0;      // Frame
    Vec3 up = Vec3::UnitZ();  // Frame: a direction in the local x-y plane, not along the element
    double thickness = 0.0;   // Plate, Membrane: m
    double tension = 0.0;     // Membrane: N/m, the same in every direction
};

// A zero-thickness interface between two coincident 8-node faces of Solid20
// elements (corners then edge midpoints, in the same order on both faces): the
// traction is t = D (u_upper - u_lower) in the face's frame (two tangents and
// the normal from the lower face towards the upper), D = diag(ks, ks, kn).
// The factors scale kn and ks at each of the 3 x 3 Gauss points of the face:
// 1 bonded, 0 separated (a debonded or open point), in between a slipping
// point of a frictional contact (set by mechanics/LayeredSolid).
struct InterfaceElement {
    std::array<int, 8> lower{}, upper{};
    double normalStiffness = 0.0;  // N/m^3
    double shearStiffness = 0.0;   // N/m^3
    std::array<double, 9> normalFactor{1, 1, 1, 1, 1, 1, 1, 1, 1};
    std::array<double, 9> shearFactor{1, 1, 1, 1, 1, 1, 1, 1, 1};
    int group = -1;  // which interface of the body it belongs to
};

class StructuralModel {
public:
    ElementKind kind = ElementKind::Frame;
    std::vector<Vec3> nodes;  // world positions, m
    std::vector<Element> elements;
    std::vector<ElasticMaterial> materials;
    std::vector<Section> sections;
    std::vector<Constitutive> constitutive;     // Solid, Solid20: per-element elasticity (see Element::constitutive)
    std::vector<InterfaceElement> interfaces;   // Solid20 only
    // Per node: bit d set when local dof d is held at zero.
    std::vector<std::uint8_t> fixed;
    // Plate and Membrane: the plane, in world coordinates. Out-of-plane
    // displacement is along `normal`; the rotation dofs are about axis1 and axis2.
    Vec3 axis1 = Vec3::UnitX(), axis2 = Vec3::UnitY(), normal = Vec3::UnitZ();

    int dofsPerNode() const { return mechanics::dofsPerNode(kind); }
    size_t dofCount() const { return nodes.size() * static_cast<size_t>(dofsPerNode()); }
    size_t dof(size_t node, int local) const { return node * static_cast<size_t>(dofsPerNode()) + static_cast<size_t>(local); }
    void fix(size_t node, int local) { fixed[node] |= static_cast<std::uint8_t>(1u << local); }
    bool isFixed(size_t node, int local) const { return (fixed[node] >> local) & 1u; }
    size_t fixedDofCount() const;
    double totalMass() const;

    // World displacement of a node (translations only) from a full dof vector.
    Vec3 translation(const Eigen::VectorXd& u, size_t node) const;
    Eigen::Vector3cd translation(const Eigen::VectorXcd& u, size_t node) const;
    // Unit vector of a translational dof direction in world coordinates, or
    // zero for a rotation dof.
    Vec3 dofDirection(int local) const;

    Status validate() const;
};

// Numbering of the dofs that are not fixed.
struct DofMap {
    std::vector<int> free;  // per model dof: index among the free dofs, or -1
    std::vector<size_t> model;  // per free dof: the model dof
    size_t freeCount() const { return model.size(); }
    Eigen::VectorXd expand(const Eigen::VectorXd& reduced) const;
    Eigen::VectorXcd expand(const Eigen::VectorXcd& reduced) const;
    Eigen::VectorXd reduce(const Eigen::VectorXd& full) const;
};
DofMap makeDofMap(const StructuralModel& model);

struct SystemMatrices {
    DofMap map;
    Eigen::SparseMatrix<double> stiffness;      // K, free dofs only
    Eigen::SparseMatrix<double> mass;           // M, consistent
    Eigen::SparseMatrix<double> lossStiffness;  // sum of eta_e K_e over elements whose material states a loss factor
    size_t nonZeros = 0;
};

SystemMatrices assemble(const StructuralModel& model);

// Element matrices in world coordinates (for tests and stress recovery).
struct ElementMatrices {
    Eigen::MatrixXd stiffness, mass;
};
ElementMatrices elementMatrices(const StructuralModel& model, const Element& element);

// Stress of an element from a full displacement vector.
struct ElementStress {
    bool available = false;
    double vonMises = 0.0;    // Pa
    double maxNormal = 0.0;   // Pa, largest |normal stress|
    std::string description;  // where and how it is evaluated
};
ElementStress elementStress(const StructuralModel& model, const Element& element, const Eigen::VectorXd& u);
// Whether this kind of model has stresses (a membrane does not: only its pre-tension).
bool hasStress(ElementKind kind);

// The solid element's strain-displacement matrix (engineering shear strains,
// order xx, yy, zz, xy, yz, zx) at a natural point of the hexahedron with
// corner positions x, and the Jacobian determinant there; and the isotropic
// elasticity matrix in the same order. Used by the thermal loads
// (mechanics/ThermalLoads).
Eigen::Matrix<double, 6, 24> solidStrainMatrix(const std::array<Vec3, 8>& x, double xi, double eta, double zeta, double& detJ);
Eigen::Matrix<double, 6, 6> solidElasticity(const ElasticMaterial& material);

int nodesPerElement(ElementKind kind);

// The elasticity matrix of a Solid or Solid20 element (its own, or its material's).
Eigen::Matrix<double, 6, 6> elementElasticity(const StructuralModel& model, const Element& element);
double elementDensity(const StructuralModel& model, const Element& element);

// The 20-node element's shape functions and their natural derivatives at a natural point.
void solid20Shape(double xi, double eta, double zeta, Eigen::Matrix<double, 20, 1>& n, Eigen::Matrix<double, 20, 3>& dn);
// Its shape-function gradients in world coordinates (20 x 3) and the Jacobian
// determinant there (corner and midpoint positions x).
Eigen::Matrix<double, 20, 3> solid20Gradients(const std::array<Vec3, 20>& x, double xi, double eta, double zeta, double& detJ);
std::array<Vec3, 20> solid20Positions(const StructuralModel& model, const Element& element);
// The 20-node element's strain-displacement matrix (Voigt, engineering shear) from its gradients.
Eigen::Matrix<double, 6, 60> solid20StrainMatrix(const Eigen::Matrix<double, 20, 3>& gradients);

// Stress (Voigt order, Pa) of a Solid or Solid20 element at a natural point.
Eigen::Matrix<double, 6, 1> solidStress(const StructuralModel& model, const Element& element, const Eigen::VectorXd& u, double xi, double eta,
                                        double zeta);

// The traction across an interface element at each of its 3 x 3 Gauss points.
struct InterfacePointState {
    Vec3 position = Vec3::Zero();
    Vec3 normal = Vec3::UnitZ();  // from the lower face towards the upper
    Vec3 jump = Vec3::Zero();     // u_upper - u_lower, m
    double opening = 0.0;         // jump along the normal, m (positive: separating)
    double slip = 0.0;            // tangential jump, m
    double normalTraction = 0.0;  // Pa, positive in tension (with the point's factor)
    double shearTraction = 0.0;   // Pa, magnitude (with the point's factor)
    double xi = 0.0, eta = 0.0;   // natural point on the face
};
std::array<InterfacePointState, 9> interfaceStates(const StructuralModel& model, const InterfaceElement& element, const Eigen::VectorXd& u);
// The 48 x 48 stiffness of an interface element (lower dofs, then upper).
Eigen::MatrixXd interfaceStiffness(const StructuralModel& model, const InterfaceElement& element);
// The face frame (rows: tangent 1, tangent 2, normal) at each of the 3 x 3 Gauss points.
std::array<Eigen::Matrix3d, 9> interfaceFrames(const StructuralModel& model, const InterfaceElement& element);
// With a given traction-jump tangent per Gauss point (3 x 3, in its face frame),
// instead of the element's diagonal stiffnesses: the 48 x 48 stiffness.
Eigen::MatrixXd interfaceStiffness(const StructuralModel& model, const InterfaceElement& element, const std::array<Eigen::Matrix3d, 9>& localTangent);
// The nodal forces (48: lower dofs, then upper) of tractions given per Gauss point
// in its face frame, acting on the upper face (the lower face takes the opposite).
Eigen::Matrix<double, 48, 1> interfaceForces(const StructuralModel& model, const InterfaceElement& element, const std::array<Eigen::Vector3d, 9>& localTraction);

}  // namespace resonant::mechanics
