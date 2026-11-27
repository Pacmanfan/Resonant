#pragma once

// The surface of a structural model for display: deformed by a scaled
// displacement and carrying one value per vertex for colouring.
//
// The scale is a visual magnification chosen by the viewer; it is never a
// physical quantity, and the undeformed outline is returned separately so
// the two can always be told apart.
#include "mechanics/StructuralModel.h"

#include <array>

namespace resonant::mechanics {

struct SurfaceMesh {
    std::vector<Eigen::Vector3f> positions, normals;
    std::vector<float> values;
    std::vector<std::uint32_t> indices;
    std::vector<std::pair<Vec3, Vec3>> outline;  // undeformed
    bool empty() const { return indices.empty(); }
};

// `grid` is the element count along each mesh direction (as in
// StructureStudy::grid); `nodeValues` has one value per node.
SurfaceMesh deformedSurface(const StructuralModel& model, const std::array<int, 3>& grid, const Eigen::VectorXd& displacement, double scale,
                            const std::vector<double>& nodeValues);

// A magnification that makes the largest translation `fraction` of the
// structure's largest dimension.
double automaticScale(const StructuralModel& model, const Eigen::VectorXd& displacement, double fraction = 0.1);
double largestDimension(const StructuralModel& model);

}  // namespace resonant::mechanics
