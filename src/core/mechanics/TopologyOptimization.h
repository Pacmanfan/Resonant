#pragma once

// Topology optimization, a proof of concept (RES-OPT-011): minimum
// compliance of a two-dimensional plane-stress design region under a volume
// constraint, by SIMP (solid isotropic material with penalization), a
// density filter and optimality-criteria updates - the formulation of the
// well-known 88-line code of Andreassen et al. (2011), written here on
// Eigen sparse matrices.
//
//   minimize   c(x) = U^T K(x) U,  K(x) = sum_e (E_min + x_e^p (E_0 - E_min)) k_e
//   subject to sum_e x_e = f N,  0 <= x_e <= 1
//
// on a structured mesh of square bilinear (Q4) elements. Benchmarks: the
// cantilever (left edge clamped, a load at the middle of the right edge) and
// the half MBB beam (symmetry on the left edge, a roller at the bottom right,
// a load at the top left).
//
// What the result is NOT: a finished physical design. The density field is a
// numerical material distribution with intermediate densities; it depends on
// the mesh unless the filter radius is held in physical units, its smallest
// feature is set by the filter radius, and it is two-dimensional. The
// cleanup step thresholds it, removes islands that carry no load, and
// re-analyses the binary design; the export extrudes the cleaned design and
// says so.
#include "foundation/Result.h"
#include "geometry/TriangleMesh.h"

#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::mechanics {

enum class TopologyBenchmark { Cantilever, HalfMbbBeam };
std::string_view toString(TopologyBenchmark benchmark);

struct TopologySettings {
    TopologyBenchmark benchmark = TopologyBenchmark::Cantilever;
    int nelx = 60, nely = 30;          // elements; cells are square
    double width = 0.06;               // m, along x; height = width * nely / nelx
    double thickness = 0.001;          // m, out of plane
    double youngsModulus = 70e9;       // Pa
    double poissonsRatio = 0.33;
    double load = 100.0;               // N
    double volumeFraction = 0.5;
    double penalty = 3.0;
    double filterRadius = 0.0;         // m; 0 = 1.5 element sizes
    double minimumStiffness = 1e-9;    // E_min / E_0
    int maxIterations = 200;
    double tolerance = 0.01;           // largest density change per iteration
    double threshold = 0.5;            // cleanup
    Status validate() const;
    double elementSize() const { return width / nelx; }
    double height() const { return elementSize() * nely; }
};

struct TopologyResult {
    TopologySettings settings;
    std::vector<double> density;       // physical (filtered) densities, index ely + elx * nely, ely = 0 at the top
    std::vector<double> compliance;    // J, per iteration
    std::vector<double> volume;        // fraction, per iteration
    std::vector<double> change;        // per iteration
    int iterations = 0;
    bool converged = false;
    double initialCompliance = 0.0;    // of the uniform design at the volume fraction
    double grayness = 0.0;             // fraction of elements with 0.1 < x < 0.9
    // Cleanup
    std::vector<std::uint8_t> solid;   // 1 = material after threshold and island removal
    int islandsRemoved = 0;
    double cleanedVolumeFraction = 0.0;
    double cleanedCompliance = 0.0;    // re-analysed binary design (void at E_min)
    double minimumFeature = 0.0;       // m, about twice the filter radius
    std::vector<std::string> notes;
};

Result<TopologyResult> optimizeTopology(const TopologySettings& settings, JobContext* context = nullptr);

// Compliance of a density field (no optimization), for validation.
Result<double> topologyCompliance(const TopologySettings& settings, const std::vector<double>& density, std::vector<double>* displacement = nullptr);

// The cleaned design extruded by the thickness as a closed voxel surface
// (metres; x along the width, y up, z through the thickness).
TriangleMesh extrudeTopology(const TopologyResult& result);

}  // namespace resonant::mechanics
