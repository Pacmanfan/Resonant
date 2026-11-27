#pragma once

// Structured meshes for the element formulations of StructuralModel.
//
// Every mesh here is regular: a line of frame elements, a grid of
// rectangles, a grid of boxes. Supports are applied afterwards.
#include "mechanics/StructuralModel.h"

#include <functional>

namespace resonant::mechanics {

// Frame elements from `start` to `end`. `sectionAt(s)` gives the section at
// a fraction s of the length (evaluated at each element's midpoint), so
// tapered and stepped members are possible; `up` fixes the local y axis.
StructuralModel frameLine(const Vec3& start, const Vec3& end, int elements, const std::function<Section(double)>& sectionAt,
                          const ElasticMaterial& material, const Vec3& up);
StructuralModel frameLine(const Vec3& start, const Vec3& end, int elements, const Section& section, const ElasticMaterial& material,
                          const Vec3& up);

// A rectangle of sides a (along axis1) and b (along axis2), centred on
// `centre`, divided into n1 x n2 elements. Node (i, j) has index
// i + (n1 + 1) j.
StructuralModel rectangularGrid(ElementKind kind, const Vec3& centre, const Vec3& axis1, const Vec3& axis2, double a, double b, int n1,
                                int n2, double thickness, double tension, const ElasticMaterial& material);

// A box centred on `centre` with edges along the three axes, sizes
// s1 x s2 x s3, divided into n1 x n2 x n3 hexahedra. Node (i, j, k) has
// index i + (n1 + 1) (j + (n2 + 1) k).
StructuralModel hexBox(const Vec3& centre, const Vec3& axis1, const Vec3& axis2, const Vec3& axis3, const Vec3& size, int n1, int n2,
                       int n3, const ElasticMaterial& material);

// Cross-sections.
Section rectangularSection(double width, double height);  // width along local y, height along local z
Section circularSection(double radius);
Section tubeSection(double outerRadius, double innerRadius);

// Supports, by local dof index. The dofs of a frame node: 0..2
// translations, 3..5 rotations; plate: 0 w, 1 rotation about axis1, 2
// about axis2; membrane: 0; solid: 0..2.
void fixDofs(StructuralModel& model, size_t node, std::initializer_list<int> dofs);
void fixAll(StructuralModel& model, size_t node);

}  // namespace resonant::mechanics
