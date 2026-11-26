// A display mesh: triangles with per-vertex normals. A TriangleMesh is
// derived from engineering geometry (geometry/Geometry.h) for rendering and
// picking. It is never the source of truth for a shape, and it is not a
// simulation mesh.
#pragma once

#include "foundation/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace resonant {

struct Aabb {
    Vec3 min = Vec3::Constant(std::numeric_limits<double>::infinity());
    Vec3 max = Vec3::Constant(-std::numeric_limits<double>::infinity());

    bool isValid() const { return (min.array() <= max.array()).all(); }
    void extend(const Vec3& point)
    {
        min = min.cwiseMin(point);
        max = max.cwiseMax(point);
    }
    Vec3 center() const { return 0.5 * (min + max); }
    Vec3 size() const { return max - min; }
};

struct TriangleMesh {
    std::vector<Eigen::Vector3f> positions;
    std::vector<Eigen::Vector3f> normals;
    std::vector<std::uint32_t> indices;

    size_t triangleCount() const { return indices.size() / 3; }
    bool empty() const { return indices.empty(); }

    std::uint32_t addVertex(const Vec3& position, const Vec3& normal);
    // Adds a triangle; the winding is chosen so the face normal agrees with
    // the vertex normals. Degenerate (zero-area) triangles are dropped.
    void addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
    void append(const TriangleMesh& other);

    Aabb bounds() const;
};

struct Ray {
    Vec3 origin = Vec3::Zero();
    Vec3 direction = Vec3::UnitX();  // need not be normalized
};

// Distance parameter t (point = origin + t * direction, t >= 0) of the
// nearest intersection of the ray with the mesh placed by `model`, or
// nullopt. Both triangle faces count.
std::optional<double> intersect(const Ray& ray, const TriangleMesh& mesh, const Mat4& model);

}  // namespace resonant
