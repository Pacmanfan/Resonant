#pragma once

// Thermal loads of the finite-element model (REQ-CHAT-20261126-016,
// RES-CEME-012, docs/mechanics/THERMAL_LOADS.md).
//
// A temperature change Delta T in an element gives the free thermal strain
// eps_th = alpha Delta T (I, no shear). Its equivalent nodal load is
//   f = integral B^T D eps_th dV          (solid hexahedra, 2x2x2 Gauss)
//   f = -/+ E A alpha Delta T along the axis at the two nodes   (frames)
// and the stress of a displacement u is D (B u - eps_th). Linear thermoelasticity:
// small strains, properties independent of temperature inside one solve, no
// thermal bending of frames (a uniform temperature over the section).
// Plates and membranes are not supported.
#include "foundation/Result.h"
#include "mechanics/StructuralModel.h"

#include <Eigen/Dense>

#include <array>
#include <vector>

namespace resonant::mechanics {

struct ThermalLoadInput {
    std::vector<double> elementTemperature;  // K, one per element
    double referenceTemperature = 293.15;    // K: the stress-free temperature
    std::vector<double> expansion;           // 1/K, one per model material
};

// The load vector over all model dofs (fixed dofs included; solveStatic reduces it).
Result<Eigen::VectorXd> thermalLoadVector(const StructuralModel& model, const ThermalLoadInput& input);

struct ThermalStressResult {
    // Per element at its centre: solid xx, yy, zz, xy, yz, zx (Pa); frames: the axial stress in [0].
    std::vector<std::array<double, 6>> stress;
    std::vector<double> vonMises;  // Pa
    double maxVonMises = 0.0;
};
Result<ThermalStressResult> thermalStress(const StructuralModel& model, const ThermalLoadInput& input, const Eigen::VectorXd& displacement);

}  // namespace resonant::mechanics
