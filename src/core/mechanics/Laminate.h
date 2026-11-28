#pragma once

// Classical lamination theory (REQ-CHAT-20261127-001, RES-FAM-011): plies that are
// in-plane orthotropic (isotropic as a special case) at angles, perfectly bonded,
// under the Kirchhoff hypothesis (thin, no transverse shear). Gives the extension,
// coupling and bending matrices, the effective in-plane engineering constants,
// the neutral surface and the reduced bending stiffness, the free thermal
// response (mid-plane strain, curvature, ply stresses) and the bending modes of
// cantilevered beams and simply supported plates from the laminate's stiffness
// and mass - so that a deposited layer changes the structure's behaviour by its
// own stiffness and mass, never assumed negligible.
//
// Assumptions: linear elasticity, small strain, plane stress in every ply,
// perfect bonding (no slip, no peel), no transverse shear (thin laminates, long
// wavelengths). Interfacial shear and peel stresses concentrate at free edges and
// are not given by the theory.

#include "foundation/Result.h"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace resonant::mechanics {

struct Ply {
    std::string name;
    double thickness = 0.0;                              // m
    double E1 = 0.0, E2 = 0.0, G12 = 0.0, nu12 = 0.0;    // Pa, Pa, Pa, -
    double density = 0.0;                                // kg/m^3
    double angle = 0.0;                                  // rad: the ply's 1-axis from the laminate x-axis
    double alpha1 = 0.0, alpha2 = 0.0;                   // 1/K
    double lossFactor = 0.0;                             // structural loss factor
    static Ply isotropic(std::string name, double thickness, double youngs, double poisson, double density, double alpha = 0.0, double lossFactor = 0.0);
    Status validate() const;
};

struct Laminate {
    std::vector<Ply> plies;  // from the bottom (z = -h/2) to the top
    std::vector<double> z;   // the ply boundaries measured from the mid-plane (n + 1 values)
    Eigen::Matrix3d A, B, D; // N/m, N, N m
    double thickness = 0.0;
    double massPerArea = 0.0;
    double Ex = 0.0, Ey = 0.0, Gxy = 0.0, nuxy = 0.0;  // from A^-1 / h
    Eigen::Matrix3d Dstar;   // D - B A^-1 B: bending about the neutral surface
    double neutralSurface = 0.0;  // z of the neutral surface for bending along x (B11 / A11)
    bool symmetric = true;   // B vanishes (relative to A h)
    Eigen::Matrix3d Qbar(size_t ply) const;
    Eigen::Vector3d alpha(size_t ply) const;  // ply expansion in laminate axes (alpha_x, alpha_y, alpha_xy engineering)
};

Result<Laminate> laminate(const std::vector<Ply>& plies);

struct ThermalResponse {
    double deltaT = 0.0;
    Eigen::Vector3d strain = Eigen::Vector3d::Zero();     // mid-plane strain of the free laminate
    Eigen::Vector3d curvature = Eigen::Vector3d::Zero();  // 1/m
    struct PlyStress {
        Eigen::Vector3d bottom, top;  // sigma_x, sigma_y, tau_xy in laminate axes, Pa
    };
    std::vector<PlyStress> stress;
};
// The free laminate under a uniform temperature change (no external load).
ThermalResponse thermalResponse(const Laminate& laminate, double deltaT);

// Narrow-beam bending along x (uniaxial stress in every ply): EI per unit width with each
// ply's axial modulus 1 / s11_bar, the neutral axis and the strain-energy-weighted loss factor.
struct BeamSection {
    double bendingStiffness = 0.0;  // EI / b, N m
    double neutralAxis = 0.0;       // from the bottom face, m
    double massPerArea = 0.0;       // kg/m^2
    double lossFactor = 0.0;        // sum eta_i E_i I_i / EI
    std::vector<double> axialModulus, centroid;  // per ply: E_x and the mid-height from the bottom face
};
BeamSection beamSection(const std::vector<Ply>& plies);

// beta_n L of a clamped-free Euler-Bernoulli beam.
double cantileverRoot(int n);
// f_n = (beta_n L)^2 / (2 pi L^2) sqrt(EI / m'), Hz.
std::vector<double> cantileverFrequencies(double bendingStiffness, double massPerLength, double length, int count);

struct PlateMode {
    int m = 1, n = 1;
    double frequency = 0.0;  // Hz
};
// A simply supported rectangular plate, specially orthotropic (D16 = D26 = 0 assumed):
// omega = pi^2 sqrt((D11 (m/a)^4 + 2 (D12 + 2 D66) (m/a)^2 (n/b)^2 + D22 (n/b)^4) / rho_h).
std::vector<PlateMode> simplySupportedPlateModes(const Eigen::Matrix3d& D, double massPerArea, double a, double b, int count);

}  // namespace resonant::mechanics
