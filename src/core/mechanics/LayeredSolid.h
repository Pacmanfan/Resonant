#pragma once

// Layered and coated three-dimensional bodies (REQ-CHAT-20261128-004,
// RES-MECH-019..022, ADR-000232).
//
// A body is a reference surface (a plane, a cylinder, a sphere, or a surface
// of revolution), layers built outwards along its normal, and an interface
// between every two consecutive layers. It is meshed with quadratic 20-node
// hexahedra (StructuralModel, ElementKind::Solid20), with elements through the
// thickness of every layer: curvature, variable thickness, material
// transitions and the stresses through each layer are resolved in three
// dimensions. Layered-plate theory (mechanics/Laminate) and the beam and plate
// models stay as lower-fidelity references: they are not equivalent to this.
//
// Every interface states its model; none is assumed (a missing model refuses
// the analysis that needs it):
//   perfect-bonding     the layers share their nodes: an explicit idealization
//   finite-stiffness    a spring layer: normal and shear stiffness, N/m^3
//   sliding             normal stiffness and no shear: free slip, no separation
//   frictional-contact  Coulomb friction with a penalty contact: no tension;
//                       a point sticks, or slips with a shear traction of mu
//                       times its contact pressure (nonlinear, holonomic: valid
//                       for monotonic, proportional loading; no load history)
// Any interface may have debonded regions ((u, v) boxes): traction-free,
// without contact (an interpenetration there is reported, not prevented); and
// normal and shear strengths, for the quadratic traction criterion of damage
// initiation (no damage evolution). A cohesive-zone model is not implemented.
// Thermally, each interface states perfect contact or a contact resistance.
//
// Small displacements and strains, linear elasticity in the layers.

#include "foundation/Result.h"
#include "foundation/Types.h"
#include "mechanics/ElasticMaterial.h"
#include "mechanics/StructuralModel.h"

#include <Eigen/Core>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace resonant::mechanics {

// The surface the first layer lies on, parameterized by (u, v) in [0, 1]^2.
struct ReferenceSurface {
    enum class Kind { Plane, Cylinder, Sphere, Revolution };
    Kind kind = Kind::Plane;
    double length = 0.0;  // Plane: extent along x (u); Cylinder: axial length along z (v), m
    double width = 0.0;   // Plane: extent along y (v), m
    double radius = 0.0;  // Cylinder, Sphere: radius of the inner face of the first layer, m
    double sweep = 0.0;   // Cylinder (u), Sphere and Revolution (v): angle covered about z, rad (up to 2 pi)
    double polarStart = 0.0, polarEnd = 0.0;  // Sphere (u): angles from +z, rad
    // Revolution (u): the meridian, (r, z) points in m, joined by a smooth
    // (Catmull-Rom) curve; the layers build on its right-hand side in the (r, z) plane, outwards
    // for a profile listed from bottom to top.
    std::vector<std::array<double, 2>> profile;

    Status validate() const;
    // The point, the unit normal (the side the layers build on) and the unit
    // tangent along u at (u, v).
    void evaluate(double u, double v, Vec3& point, Vec3& normal, Vec3& tangentU) const;
    std::string describe() const;
};

// Orthotropic engineering constants in the material axes: 1 in the surface at
// the layer's angle from the u direction, 2 in the surface across it, 3 along
// the normal. Pa, and dimensionless Poisson ratios (nu_ij = -eps_j / eps_i).
struct OrthotropicConstants {
    double e1 = 0, e2 = 0, e3 = 0, g12 = 0, g13 = 0, g23 = 0, nu12 = 0, nu13 = 0, nu23 = 0;
};

struct LayerMaterial {
    ElasticMaterial base;  // isotropic E, nu, rho (and the name, loss factor, provenance)
    std::optional<OrthotropicConstants> orthotropic;  // replaces base's E and nu (base keeps the density)
    double angle = 0.0;  // rad, of material axis 1 from the u direction, about the normal
    // A material transition: the fraction of `graded` rises from 0 to 1 along
    // `grading` as f = s^exponent (s the position through the layer, or u, or
    // v); E, nu, rho and the conductivity mix linearly (the Voigt estimate, an
    // upper bound of the stiffness). Element by element (constant in each).
    std::optional<ElasticMaterial> graded;
    enum class Grading { Thickness, U, V };
    Grading grading = Grading::Thickness;
    double exponent = 1.0;
    // Thermal conduction (the thermal analysis needs it).
    std::optional<double> conductivity;        // W/(m K)
    std::optional<double> gradedConductivity;  // W/(m K), of `graded`
};

// Layer thickness, constant or varying linearly along u or v.
struct LayerThickness {
    enum class Along { Constant, U, V };
    Along along = Along::Constant;
    double start = 0.0, end = 0.0;  // m (Constant: start)
    double at(double u, double v) const;
};

struct BodyLayer {
    std::string name;
    LayerMaterial material;
    LayerThickness thickness;
    int elementsThrough = 1;
};

enum class InterfaceMechanics { PerfectBonding, FiniteStiffness, Sliding, FrictionalContact };
std::string_view toString(InterfaceMechanics kind);
enum class InterfaceThermal { Perfect, Resistance };

struct ParameterBox {
    double u0 = 0, u1 = 0, v0 = 0, v1 = 0;
    bool contains(double u, double v) const { return u >= u0 && u <= u1 && v >= v0 && v <= v1; }
};

struct LayerInterface {
    std::string name;
    std::optional<InterfaceMechanics> mechanical;  // nullopt: undeclared, the mechanical analyses refuse
    double normalStiffness = 0.0;  // N/m^3: finite-stiffness, sliding; frictional-contact (the contact penalty; 0: derived)
    double shearStiffness = 0.0;   // N/m^3: finite-stiffness; frictional-contact (the stick penalty; 0: derived)
    double frictionCoefficient = 0.0;
    std::vector<ParameterBox> debonded;            // separated, traction-free regions
    std::optional<double> normalStrength, shearStrength;  // Pa, the quadratic initiation criterion
    std::optional<InterfaceThermal> thermal;       // nullopt: undeclared, the thermal analysis refuses
    double thermalResistance = 0.0;                // m^2 K / W
    std::string provenance;
};

struct LayeredBody {
    std::string name;
    ReferenceSurface surface;
    std::vector<BodyLayer> layers;             // from the reference surface outwards
    std::vector<LayerInterface> interfaces;    // layers.size() - 1: between layer i and i + 1
    int elementsU = 8, elementsV = 4;
    Status validate() const;
    double thicknessAt(double u, double v) const;
};

enum class BodyEdge { U0, U1, V0, V1 };

struct SymmetryPlane {
    int axis = 0;        // 0 x, 1 y, 2 z: the displacement along it is held on the plane
    double value = 0.0;  // the plane x_axis = value, m
};

struct LayeredSupports {
    std::vector<BodyEdge> clamped;           // every node of the edge face, all layers, held
    std::vector<SymmetryPlane> symmetry;
    std::vector<int> heldAxes;               // displacement along these axes held at every node (plane strain, ...)
    bool innerFaceClamped = false;           // the inner face of the first layer held
};

struct LayeredLoads {
    double innerPressure = 0.0;  // Pa, on the inner face, pushing into the body
    double outerPressure = 0.0;  // Pa, on the outer face, pushing into the body
    Vec3 outerTraction = Vec3::Zero();  // Pa, a further traction on the outer face, world axes
    // A total force (N, world axes) spread as a uniform traction over an edge face (all layers).
    std::vector<std::pair<BodyEdge, Vec3>> edgeForces;
};

// The mesh of a body.
struct LayeredMesh {
    StructuralModel structure;               // Solid20 elements and interface elements
    std::vector<int> elementLayer;           // the layer of each element
    std::vector<std::array<double, 3>> elementParameter;  // (u, v, s) at each element's centre (s: through its layer)
    std::vector<std::array<int, 3>> elementCell;           // (i, j, k): k through its layer
    std::vector<std::array<std::array<double, 2>, 9>> interfaceParameter;  // (u, v) of each interface element's Gauss points
    std::vector<std::array<bool, 9>> interfaceDebonded;
    std::vector<std::array<std::vector<int>, 2>> layerFaceNodes;  // per layer: the nodes of its inner (0) and outer (1) face
    std::vector<std::array<double, 2>> nodeParameter;             // (u, v) of each node
    std::vector<std::string> assumptions;
};
// The mechanical mesh: perfect bonding (without debonded regions) shares the
// nodes; every other interface gets interface elements (a perfect bond with a
// debonded region is enforced by a stated penalty).
Result<LayeredMesh> meshLayeredBody(const LayeredBody& body, const LayeredSupports& supports);
// The nodal forces (full dofs) of the loads on a meshed body.
Result<Eigen::VectorXd> layeredLoadVector(const LayeredBody& body, const LayeredMesh& mesh, const LayeredLoads& loads);

struct LayeredElementResult {
    int layer = 0;
    Vec3 centre = Vec3::Zero();
    std::array<double, 3> parameter{};       // (u, v, s)
    Eigen::Matrix<double, 6, 1> stress;      // world axes, Voigt order, Pa
    // In the surface frame (u tangent, the tangent across it, the normal):
    // s_uu, s_vv, s_nn, s_uv, s_vn, s_nu, Pa.
    Eigen::Matrix<double, 6, 1> surfaceStress;
    double vonMises = 0.0;
};

struct LayeredInterfaceResult {
    int interface = 0;
    std::array<double, 2> parameter{};  // (u, v)
    Vec3 position = Vec3::Zero();
    double normalTraction = 0.0;  // Pa, positive in tension
    double shearTraction = 0.0;   // Pa
    double opening = 0.0;         // m, the normal jump (0 for shared nodes)
    double slip = 0.0;            // m
    double contactPressure = 0.0; // Pa, frictional contact
    std::optional<double> failureIndex;  // (max(t_n, 0) / N)^2 + (t_s / S)^2, with both strengths stated
    std::string state;  // bonded, stick, slip, open, debonded
};

struct LayeredStaticResult {
    LayeredMesh mesh;
    Eigen::VectorXd displacement;  // full dofs of mesh.structure
    double maxDisplacement = 0.0;  // m
    double compliance = 0.0;       // f^T u, J
    std::vector<LayeredElementResult> elements;
    std::vector<LayeredInterfaceResult> interfacePoints;
    int iterations = 1;  // frictional contact: stick/slip iterations
    bool converged = true;
    std::string fidelity;
    std::vector<std::string> assumptions, warnings;
    // The largest failure index of an interface, or nullopt without strengths.
    std::optional<double> maxFailureIndex(int interface) const;
    double maxVonMises(int layer) const;
};
Result<LayeredStaticResult> solveLayeredStatic(const LayeredBody& body, const LayeredSupports& supports, const LayeredLoads& loads);

struct LayeredModalResult {
    LayeredMesh mesh;
    std::vector<double> frequencies;  // Hz
    std::vector<Eigen::VectorXd> shapes;  // full dofs
    std::string fidelity;
    std::vector<std::string> assumptions, warnings;
};
// Refuses frictional contact (stick or slip depends on the load).
Result<LayeredModalResult> solveLayeredModal(const LayeredBody& body, const LayeredSupports& supports, int count);

struct LayeredThermalBoundary {
    std::optional<double> innerTemperature, outerTemperature;  // K
    std::optional<double> innerFlux;  // W/m^2 into the body through the inner face
    std::optional<std::array<double, 2>> outerConvection;  // (h W/(m^2 K), ambient K)
};

struct LayeredThermalResult {
    std::vector<double> temperature;      // per node of `nodes`
    std::vector<Vec3> nodes;
    double heatIn = 0.0;                  // W, into the body through the inner face
    double innerArea = 0.0;               // m^2
    std::vector<double> interfaceJump;    // K, mean T_outer side - T_inner side, per interface
    std::vector<std::array<double, 2>> layerRange;  // K, (min, max) per layer
    std::string fidelity;
    std::vector<std::string> assumptions, warnings;
};
// Steady conduction on the same geometry, 20-node elements; refuses an
// interface without a thermal model or a layer without a conductivity.
Result<LayeredThermalResult> solveLayeredConduction(const LayeredBody& body, const LayeredThermalBoundary& boundary);

// The elasticity matrix of orthotropic constants in their own axes (refused,
// never repaired, when it is not positive definite).
Result<Eigen::Matrix<double, 6, 6>> orthotropicStiffness(const OrthotropicConstants& c);
// The elasticity matrix in world axes, of one given in material axes whose
// directions in world coordinates are the columns of q.
Eigen::Matrix<double, 6, 6> rotateStiffness(const Eigen::Matrix<double, 6, 6>& material, const Eigen::Matrix3d& q);

}  // namespace resonant::mechanics
