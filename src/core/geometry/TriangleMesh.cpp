#include "geometry/TriangleMesh.h"

#include "foundation/Transform.h"

namespace resonant {

std::uint32_t TriangleMesh::addVertex(const Vec3& position, const Vec3& normal)
{
    positions.emplace_back(position.cast<float>());
    normals.emplace_back(normal.cast<float>());
    return static_cast<std::uint32_t>(positions.size() - 1);
}

void TriangleMesh::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    const Eigen::Vector3f face = (positions[b] - positions[a]).cross(positions[c] - positions[a]);
    if (face.squaredNorm() <= 1e-24f)
        return;
    const Eigen::Vector3f vertexNormal = normals[a] + normals[b] + normals[c];
    if (face.dot(vertexNormal) < 0.0f)
        std::swap(b, c);
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

void TriangleMesh::append(const TriangleMesh& other)
{
    const auto offset = static_cast<std::uint32_t>(positions.size());
    positions.insert(positions.end(), other.positions.begin(), other.positions.end());
    normals.insert(normals.end(), other.normals.begin(), other.normals.end());
    indices.reserve(indices.size() + other.indices.size());
    for (const std::uint32_t index : other.indices)
        indices.push_back(index + offset);
}

Aabb TriangleMesh::bounds() const
{
    Aabb box;
    for (const auto& position : positions)
        box.extend(position.cast<double>());
    return box;
}

std::optional<double> intersect(const Ray& ray, const TriangleMesh& mesh, const Mat4& model)
{
    // Work in the mesh's local frame: t is preserved because the ray is
    // transformed with the same affine map as its points.
    const Mat4 inverse = model.inverse();
    const Vec3 origin = transformPoint(inverse, ray.origin);
    const Vec3 direction = transformDirection(inverse, ray.direction);

    std::optional<double> nearest;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vec3 a = mesh.positions[mesh.indices[i]].cast<double>();
        const Vec3 b = mesh.positions[mesh.indices[i + 1]].cast<double>();
        const Vec3 c = mesh.positions[mesh.indices[i + 2]].cast<double>();
        // Moeller-Trumbore.
        const Vec3 e1 = b - a;
        const Vec3 e2 = c - a;
        const Vec3 p = direction.cross(e2);
        const double det = e1.dot(p);
        if (std::abs(det) < 1e-18)
            continue;
        const double inv = 1.0 / det;
        const Vec3 s = origin - a;
        const double u = s.dot(p) * inv;
        if (u < 0.0 || u > 1.0)
            continue;
        const Vec3 q = s.cross(e1);
        const double v = direction.dot(q) * inv;
        if (v < 0.0 || u + v > 1.0)
            continue;
        const double t = e2.dot(q) * inv;
        if (t >= 0.0 && (!nearest || t < *nearest))
            nearest = t;
    }
    return nearest;
}

}  // namespace resonant
