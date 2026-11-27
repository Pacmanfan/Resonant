#pragma once

// Mesh integrity, manufacturing checks and mesh export (RES-OPT-022).
//
// A simulation-ready shape is not automatically manufacturing-ready. These
// checks look at a triangle mesh as a fabricated solid would need it: closed
// (every edge shared by exactly two triangles after welding coincident
// vertices), no degenerate or non-manifold edges, an enclosed volume, a size
// that fits a build volume, and the area that overhangs beyond an angle from
// the build direction. Wall thickness and feature size are checked against
// values the caller supplies (they come from the parametric definition, not
// from the mesh). STEP export is not implemented; STL and OBJ are.
#include "foundation/Result.h"
#include "geometry/TriangleMesh.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace resonant {

struct MeshIntegrity {
    size_t triangles = 0;
    size_t vertices = 0;        // after welding
    size_t degenerate = 0;      // zero-area triangles
    size_t boundaryEdges = 0;   // edges of one triangle only (holes)
    size_t nonManifoldEdges = 0;  // edges of more than two triangles
    bool closed = false;
    double area = 0.0;          // m^2
    double volume = 0.0;        // m^3, enclosed (meaningful when closed)
    Aabb bounds;
};
MeshIntegrity checkMesh(const TriangleMesh& mesh, double weldTolerance = 1e-9);

// Fraction of the surface area facing downwards more steeply than
// `maxOverhangRad` from the build direction (45 degrees is a common limit
// for fused deposition), not counting the bottom face on the build plate.
double overhangFraction(const TriangleMesh& mesh, const Vec3& buildDirection, double maxOverhangRad);

struct PrintabilityLimits {
    std::optional<double> minimumWall;        // m
    std::optional<double> minimumFeature;     // m
    std::optional<Vec3> buildVolume;          // m, the printer's box
    double maxOverhangRad = 0.785398163397448;  // 45 degrees
    double allowedOverhangFraction = 0.0;     // more needs support material
    Vec3 buildDirection = Vec3::UnitZ();
};

struct PrintabilityReport {
    MeshIntegrity integrity;
    double overhang = 0.0;
    std::vector<std::string> failures;   // the part cannot be made as it is
    std::vector<std::string> warnings;   // it can, with care (support material, ...)
    bool printable() const { return failures.empty(); }
};
// `wall` and `feature` are the design's smallest wall thickness and feature
// size when known (from its parameters).
PrintabilityReport checkPrintability(const TriangleMesh& mesh, const PrintabilityLimits& limits, std::optional<double> wall = {},
                                     std::optional<double> feature = {});

Status writeStl(const TriangleMesh& mesh, const std::filesystem::path& path, const std::string& name = "resonant");  // ASCII STL, metres
Status writeObj(const TriangleMesh& mesh, const std::filesystem::path& path, const std::string& comment = {});      // Wavefront OBJ, metres

}  // namespace resonant
