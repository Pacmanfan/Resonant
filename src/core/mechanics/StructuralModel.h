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
//
// Small displacements, small strains, linear-elastic isotropic materials.
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

enum class ElementKind { Frame, Plate, Membrane, Solid };

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

struct Element {
    std::array<int, 8> nodes{};  // Frame: 2; Plate and Membrane: 4, counter-clockwise; Solid: 8
    int material = 0;
    int section = 0;      // Frame
    Vec3 up = Vec3::UnitZ();  // Frame: a direction in the local x-y plane, not along the element
    double thickness = 0.0;   // Plate, Membrane: m
    double tension = 0.0;     // Membrane: N/m, the same in every direction
};

class StructuralModel {
public:
    ElementKind kind = ElementKind::Frame;
    std::vector<Vec3> nodes;  // world positions, m
    std::vector<Element> elements;
    std::vector<ElasticMaterial> materials;
    std::vector<Section> sections;
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

}  // namespace resonant::mechanics
