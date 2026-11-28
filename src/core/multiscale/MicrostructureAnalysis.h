#pragma once

// Microstructure statistics, and microstructure in continuum properties
// (REQ-CHAT-20261128-006, RES-MSC-017, ADR-000234).
//
// Of a voxel RVE (multiscale/Microstructure), the statistics are:
// - the phase fractions; the porosity (the phases named void);
// - the grains and their volume-equivalent mean diameter;
// - the grain-boundary area density (faces between voxels of different grains of
//   one phase) and the phase-boundary area density (faces between different
//   phases);
// - the number of separate inclusions (connected regions of the named inclusion
//   phases, face-connected);
// - recorded defects: kind, density, unit and provenance per grain or phase.
//   They are metadata, not computed.
//
// The conduction problem of an RVE feeds the homogenization
// (multiscale/PotentialHomogenization). It takes a conductivity tensor per phase,
// with a grain-boundary (Kapitza) resistance R by a diffuse-interface
// approximation. Each voxel next to another grain carries R / 2 in series per
// such face, along that face's axis: a scaling S K S of its tensor, exact in
// series for planar boundaries normal to an axis and approximate otherwise.

#include "foundation/Result.h"
#include "multiscale/Microstructure.h"
#include "multiscale/PotentialHomogenization.h"

#include <Eigen/Core>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::multiscale {

struct DefectRecord {
    std::string kind;     // vacancy, dislocation, stacking-fault, twin, crack ...
    std::string where;    // a grain index, a phase, or "all"
    double density = 0.0;
    std::string unit;     // 1/m^2 (dislocations), site fraction (vacancies) ...
    std::string provenance = "unstated";
    nlohmann::json toJson() const;
};

struct MicrostructureStatistics {
    std::vector<double> phaseFractions;  // by phase index
    double porosity = 0.0;               // the void phases' fraction
    int grains = 0;
    double meanGrainDiameter = 0.0;      // m, volume-equivalent sphere of the mean grain volume
    double grainBoundaryDensity = 0.0;   // m^2 / m^3
    double phaseBoundaryDensity = 0.0;   // m^2 / m^3
    int inclusions = 0;
    std::vector<DefectRecord> defects;
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};

MicrostructureStatistics microstructureStatistics(const VoxelRve& rve, const std::vector<std::string>& voidPhases = {},
                                                  const std::vector<std::string>& inclusionPhases = {}, const std::vector<DefectRecord>& defects = {});

// The conduction problem with a conductivity tensor per phase (in the RVE's axes)
// and a grain-boundary resistance (m^2 K / W; 0: none).
Result<PotentialProblem> conductionProblem(const VoxelRve& rve, const std::vector<Eigen::Matrix3d>& phaseConductivity, double boundaryResistance = 0.0);

}  // namespace resonant::multiscale
