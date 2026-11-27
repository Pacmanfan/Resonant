#pragma once

// Where measurements are taken, and what can honestly be said between them.
//
// A scan definition lists measurement positions (fixed, imported, or
// generated as a line, a plane, a volume or an angular sweep). A spatial
// data set associates measured complex values with their actual poses.
// A reconstruction estimates values between measured points on a plane and
// marks every node as measured (a sample lies at the node), interpolated
// (estimated from samples within the interpolation radius) or empty (no
// sample close enough: no value, never extrapolated). Sparse measurements
// are not a measured continuous field.
#include "experiments/ExperimentTypes.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <string>
#include <vector>

namespace resonant::experiments {

enum class ScanKind { Fixed, Imported, Linear, Planar, Volumetric, Angular, ProbeArray };
std::string_view toString(ScanKind kind);
Result<ScanKind> scanKindFromString(std::string_view text);

struct ScanDefinition {
    ScanKind kind = ScanKind::Fixed;
    std::vector<Pose> points;                    // Fixed, Imported, ProbeArray
    std::string source;                          // Imported: the file it came from
    // Linear: count points from start to end.
    double start[3] = {0.0, 0.0, 0.0}, end[3] = {0.0, 0.0, 0.0};
    int count = 0;
    // Planar / Volumetric: origin + i u + j v (+ k w), i < nu, j < nv, k < nw.
    double origin[3] = {0.0, 0.0, 0.0}, axisU[3] = {0.0, 0.0, 0.0}, axisV[3] = {0.0, 0.0, 0.0}, axisW[3] = {0.0, 0.0, 0.0};
    int nu = 0, nv = 0, nw = 1;
    // Angular: positions on a circle around centre, radius, in the plane
    // "azimuth" (XY, angle from +X) or "elevation" (XZ, angle from +Z), the
    // probe pointing at the centre.
    double center[3] = {0.0, 0.0, 0.0};
    double radius = 1.0;
    double startDeg = 0.0, stopDeg = 360.0, stepDeg = 10.0;
    std::string plane = "azimuth";
    std::string frame = "scene";
    double positionUncertainty = 0.0;  // m, standard, applied to generated points

    bool empty() const { return kind == ScanKind::Fixed && points.empty(); }
};

nlohmann::json toJson(const ScanDefinition& scan);
Result<ScanDefinition> scanDefinitionFromJson(const nlohmann::json& value);
// The positions in order. Angular scans also give the angle of each point.
Result<std::vector<Pose>> scanPositions(const ScanDefinition& scan, std::vector<double>* anglesDeg = nullptr);
// "x,y,z[,dx,dy,dz]" per line (m), '#' comments, an optional header line.
Result<ScanDefinition> importPositionsCsv(const std::string& text, const std::string& source);

struct SpatialSample {
    Pose pose;
    std::complex<double> value;
    double standardUncertainty = 0.0;  // of |value|, in the data set's unit; 0 = unknown
    double angleDeg = 0.0;             // angular scans
    std::string measurementId;
};

struct SpatialDataset {
    std::string id;
    std::string experimentId;
    QuantitySpec quantity;
    double frequencyHz = 0.0;
    DataCategory category = DataCategory::Processed;
    std::string frame = "scene";
    std::vector<SpatialSample> samples;
    std::string note;
};

nlohmann::json toJson(const SpatialDataset& dataset);
Result<SpatialDataset> spatialDatasetFromJson(const nlohmann::json& value);

enum class NodeCoverage : unsigned char { Empty = 0, Interpolated = 1, Measured = 2 };

struct PlaneReconstruction {
    double origin[3] = {0, 0, 0}, axisU[3] = {0, 0, 0}, axisV[3] = {0, 0, 0};  // node (i, j) = origin + i u + j v
    int nu = 0, nv = 0;
    std::vector<std::complex<double>> values;  // [j * nu + i]; zero where empty
    std::vector<NodeCoverage> coverage;
    std::vector<double> distanceToNearest;     // m
    size_t measured = 0, interpolated = 0, empty = 0;
    size_t samplesUsed = 0;                    // samples within the plane tolerance
    double maxDistance = 0.0;
    std::string method;
    DataCategory category = DataCategory::Reconstructed;
};

// Inverse-distance weighting (power 2) of the complex values of the samples
// that lie within planeTolerance of the plane, using only samples within
// maxDistance of a node. A node within nodeTolerance of a sample takes its
// value and is marked measured.
Result<PlaneReconstruction> reconstructPlane(const SpatialDataset& dataset, const double origin[3], const double axisU[3],
                                             const double axisV[3], int nu, int nv, double maxDistance, double planeTolerance,
                                             double nodeTolerance);

}  // namespace resonant::experiments
