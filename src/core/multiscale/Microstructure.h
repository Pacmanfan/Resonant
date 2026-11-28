#pragma once

// Voxel microstructures (RES-MSC-005): a box of phases on a regular grid, with grains
// and their crystallographic axes. Generated from a specification (seeded, so an RVE is
// reproducible from its record and labelled synthetic) or given as imported voxel data.
//
// Specification (a representation's data):
//   {"generator": "homogeneous" | "layered" | "spheres" | "voronoi" | "voxels",
//    "voxels": [nx, ny, nz], "size": [Lx, Ly, Lz] (m), "phases": ["matrix", "inclusion", ...], "seed": 1,
//    layered:  "axis": 0..2, "fractions": [f0, f1, ...], "periods": 1
//    spheres:  "matrix": 0, "inclusion": 1, "radius": r (m), "volumeFraction": f   (non-overlapping, periodic)
//    voronoi:  "grains": N, "phase": 0, "texture": "random" | "aligned", "axis": [x, y, z], "spreadDeg": s
//    voxels:   "data": [phase per voxel, x fastest], "grain": [...], "axes": [[x, y, z], ...], "periodic": bool}
// Generated structures are periodic; imported ones say whether they are.

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace resonant::multiscale {

struct VoxelRve {
    std::array<int, 3> n{1, 1, 1};
    std::array<double, 3> size{1.0, 1.0, 1.0};  // m
    std::vector<std::string> phases;            // names, by index
    std::vector<std::uint16_t> phase;           // per voxel, x fastest
    std::vector<std::int32_t> grain;            // per voxel (-1: no grain)
    std::vector<std::array<double, 3>> axes;    // per grain: the unit c-axis
    bool periodic = true;
    std::string generator;

    size_t count() const { return static_cast<size_t>(n[0]) * n[1] * n[2]; }
    size_t index(int i, int j, int k) const { return static_cast<size_t>(i) + static_cast<size_t>(n[0]) * (j + static_cast<size_t>(n[1]) * k); }
    double voxel(int axis) const { return size[static_cast<size_t>(axis)] / n[static_cast<size_t>(axis)]; }
    std::vector<double> volumeFractions() const;
    int grainCount() const { return static_cast<int>(axes.size()); }
};

Result<VoxelRve> generateRve(const nlohmann::json& specification);

}  // namespace resonant::multiscale
