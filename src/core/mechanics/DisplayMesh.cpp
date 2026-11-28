#include "mechanics/DisplayMesh.h"

#include <cmath>
#include <map>

namespace resonant::mechanics {
namespace {

void addTriangle(SurfaceMesh& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    mesh.indices.insert(mesh.indices.end(), {a, b, c});
}

// Vertex normals from the triangles (area-weighted).
void computeNormals(SurfaceMesh& mesh)
{
    mesh.normals.assign(mesh.positions.size(), Eigen::Vector3f::Zero());
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const auto a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
        const Eigen::Vector3f n = (mesh.positions[b] - mesh.positions[a]).cross(mesh.positions[c] - mesh.positions[a]);
        mesh.normals[a] += n;
        mesh.normals[b] += n;
        mesh.normals[c] += n;
    }
    for (Eigen::Vector3f& n : mesh.normals)
        n = n.norm() > 0.0f ? Eigen::Vector3f(n.normalized()) : Eigen::Vector3f::UnitZ();
}

}  // namespace

double largestDimension(const StructuralModel& model)
{
    Vec3 lo = model.nodes.front(), hi = model.nodes.front();
    for (const Vec3& p : model.nodes) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }
    return (hi - lo).maxCoeff();
}

double automaticScale(const StructuralModel& model, const Eigen::VectorXd& u, double fraction)
{
    double largest = 0.0;
    for (size_t n = 0; n < model.nodes.size(); ++n)
        largest = std::max(largest, model.translation(u, n).norm());
    return largest > 0.0 ? fraction * largestDimension(model) / largest : 1.0;
}

SurfaceMesh deformedSurface(const StructuralModel& model, const std::array<int, 3>& grid, const Eigen::VectorXd& u, double scale,
                            const std::vector<double>& values)
{
    SurfaceMesh mesh;
    const auto moved = [&](size_t node) -> Vec3 { return model.nodes[node] + scale * model.translation(u, node); };
    const auto value = [&](size_t node) { return node < values.size() ? static_cast<float>(values[node]) : 0.0f; };

    switch (model.kind) {
    case ElementKind::Frame: {
        // A tube along the deformed centreline: each node gets a ring in the
        // plane of the section (rectangular sections as 4 corners, circular as 16).
        const Section& s0 = model.sections.front();
        const int ring = s0.circular ? 16 : 4;
        const Vec3 axis = (model.nodes.back() - model.nodes.front()).normalized();
        Vec3 ey = model.elements.front().up - model.elements.front().up.dot(axis) * axis;
        if (ey.norm() < 1e-9)
            ey = axis.unitOrthogonal();
        ey.normalize();
        const Vec3 ez = axis.cross(ey);
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            // The section at a node: that of an adjacent element.
            const Element& e = model.elements[std::min(n, model.elements.size() - 1)];
            const Section& s = model.sections[static_cast<size_t>(e.section)];
            const Vec3 centre = moved(n);
            for (int k = 0; k < ring; ++k) {
                Vec3 offset;
                if (s.circular) {
                    const double angle = 2.0 * 3.14159265358979323846 * k / ring;
                    offset = s.cy * (std::cos(angle) * ey + std::sin(angle) * ez);
                } else {
                    const double sy = (k == 0 || k == 3) ? -1.0 : 1.0, sz = (k < 2) ? -1.0 : 1.0;
                    offset = sy * s.cy * ey + sz * s.cz * ez;
                }
                mesh.positions.push_back((centre + offset).cast<float>());
                mesh.values.push_back(value(n));
            }
        }
        for (size_t n = 0; n + 1 < model.nodes.size(); ++n)
            for (int k = 0; k < ring; ++k) {
                const auto a = static_cast<std::uint32_t>(n * static_cast<size_t>(ring) + static_cast<size_t>(k));
                const auto b = static_cast<std::uint32_t>(n * static_cast<size_t>(ring) + static_cast<size_t>((k + 1) % ring));
                addTriangle(mesh, a, b, b + static_cast<std::uint32_t>(ring));
                addTriangle(mesh, a, b + static_cast<std::uint32_t>(ring), a + static_cast<std::uint32_t>(ring));
            }
        mesh.outline.push_back({model.nodes.front(), model.nodes.back()});
        break;
    }
    case ElementKind::Plate:
    case ElementKind::Membrane: {
        const int n1 = grid[0], n2 = grid[1];
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            mesh.positions.push_back(moved(n).cast<float>());
            mesh.values.push_back(value(n));
        }
        // Both faces, so either side is lit.
        const auto back = static_cast<std::uint32_t>(mesh.positions.size());
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            mesh.positions.push_back(mesh.positions[n]);
            mesh.values.push_back(mesh.values[n]);
        }
        for (const Element& e : model.elements) {
            const auto a = static_cast<std::uint32_t>(e.nodes[0]), b = static_cast<std::uint32_t>(e.nodes[1]);
            const auto c = static_cast<std::uint32_t>(e.nodes[2]), d = static_cast<std::uint32_t>(e.nodes[3]);
            addTriangle(mesh, a, b, c);
            addTriangle(mesh, a, c, d);
            addTriangle(mesh, back + a, back + c, back + b);
            addTriangle(mesh, back + a, back + d, back + c);
        }
        const auto node = [&](int i, int j) { return model.nodes[static_cast<size_t>(i + (n1 + 1) * j)]; };
        mesh.outline = {{node(0, 0), node(n1, 0)}, {node(n1, 0), node(n1, n2)}, {node(n1, n2), node(0, n2)}, {node(0, n2), node(0, 0)}};
        break;
    }
    case ElementKind::Solid20: {
        // Faces that belong to one element only (by their corners); each is drawn
        // as eight triangles around its centre, through its edge midpoints, so
        // curved layers stay curved. Corners, then the midpoints of their edges:
        constexpr int faces[6][8] = {{0, 3, 2, 1, 11, 10, 9, 8},     {4, 5, 6, 7, 12, 13, 14, 15}, {0, 1, 5, 4, 8, 17, 12, 16},
                                     {2, 3, 7, 6, 10, 19, 14, 18}, {1, 2, 6, 5, 9, 18, 13, 17},  {0, 4, 7, 3, 16, 15, 19, 11}};
        std::map<std::array<int, 4>, std::pair<int, std::array<int, 8>>> count;
        for (const Element& e : model.elements)
            for (const auto& f : faces) {
                std::array<int, 8> ordered{};
                for (size_t q = 0; q < 8; ++q)
                    ordered[q] = e.nodes[static_cast<size_t>(f[q])];
                std::array<int, 4> key{ordered[0], ordered[1], ordered[2], ordered[3]};
                std::sort(key.begin(), key.end());
                auto& entry = count[key];
                ++entry.first;
                entry.second = ordered;
            }
        std::map<int, std::uint32_t> vertex;
        const auto vertexOf = [&](int node) {
            auto found = vertex.find(node);
            if (found != vertex.end())
                return found->second;
            const auto index = static_cast<std::uint32_t>(mesh.positions.size());
            mesh.positions.push_back(moved(static_cast<size_t>(node)).cast<float>());
            mesh.values.push_back(value(static_cast<size_t>(node)));
            vertex.emplace(node, index);
            return index;
        };
        for (const auto& [key, entry] : count) {
            if (entry.first != 1)
                continue;
            const auto& q = entry.second;
            // The face centre of the quadratic face: -1/4 of the corners plus 1/2 of the midpoints.
            Vec3 centre = Vec3::Zero();
            float centreValue = 0.0f;
            for (size_t c = 0; c < 4; ++c) {
                centre += -0.25 * moved(static_cast<size_t>(q[c])) + 0.5 * moved(static_cast<size_t>(q[4 + c]));
                centreValue += -0.25f * value(static_cast<size_t>(q[c])) + 0.5f * value(static_cast<size_t>(q[4 + c]));
            }
            const auto mid = static_cast<std::uint32_t>(mesh.positions.size());
            mesh.positions.push_back(centre.cast<float>());
            mesh.values.push_back(centreValue);
            const std::array<int, 8> ring{q[0], q[4], q[1], q[5], q[2], q[6], q[3], q[7]};
            for (size_t r = 0; r < 8; ++r)
                addTriangle(mesh, mid, vertexOf(ring[r]), vertexOf(ring[(r + 1) % 8]));
        }
        // The outline: the feature edges of the outer surface, where two outer faces
        // meet at more than 30 degrees (the corners and rims of a layered body), undeformed.
        std::map<std::pair<int, int>, std::pair<std::vector<Vec3>, int>> edges;  // corner pair -> (face normals, midpoint)
        for (const auto& [key, entry] : count) {
            if (entry.first != 1)
                continue;
            const auto& q = entry.second;
            const Vec3 normal = (model.nodes[static_cast<size_t>(q[2])] - model.nodes[static_cast<size_t>(q[0])])
                                    .cross(model.nodes[static_cast<size_t>(q[3])] - model.nodes[static_cast<size_t>(q[1])])
                                    .normalized();
            for (size_t c = 0; c < 4; ++c) {
                const int a = q[c], b = q[(c + 1) % 4];
                auto& slot = edges[{std::min(a, b), std::max(a, b)}];
                slot.first.push_back(normal);
                slot.second = q[4 + c];
            }
        }
        const double feature = std::cos(30.0 * 3.14159265358979323846 / 180.0);
        for (const auto& [pair, slot] : edges)
            if (slot.first.size() != 2 || std::abs(slot.first[0].dot(slot.first[1])) < feature) {
                mesh.outline.push_back({model.nodes[static_cast<size_t>(pair.first)], model.nodes[static_cast<size_t>(slot.second)]});
                mesh.outline.push_back({model.nodes[static_cast<size_t>(slot.second)], model.nodes[static_cast<size_t>(pair.second)]});
            }
        break;
    }
    case ElementKind::Solid: {
        // Faces that belong to one element only.
        constexpr int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 4, 7, 3}};
        std::map<std::array<int, 4>, std::pair<int, std::array<int, 4>>> count;
        for (const Element& e : model.elements)
            for (const auto& f : faces) {
                std::array<int, 4> ordered{e.nodes[static_cast<size_t>(f[0])], e.nodes[static_cast<size_t>(f[1])],
                                           e.nodes[static_cast<size_t>(f[2])], e.nodes[static_cast<size_t>(f[3])]};
                std::array<int, 4> key = ordered;
                std::sort(key.begin(), key.end());
                auto& entry = count[key];
                ++entry.first;
                entry.second = ordered;
            }
        std::map<int, std::uint32_t> vertex;
        const auto vertexOf = [&](int node) {
            auto found = vertex.find(node);
            if (found != vertex.end())
                return found->second;
            const auto index = static_cast<std::uint32_t>(mesh.positions.size());
            mesh.positions.push_back(moved(static_cast<size_t>(node)).cast<float>());
            mesh.values.push_back(value(static_cast<size_t>(node)));
            vertex.emplace(node, index);
            return index;
        };
        for (const auto& [key, entry] : count) {
            if (entry.first != 1)
                continue;
            const auto& q = entry.second;
            const auto a = vertexOf(q[0]), b = vertexOf(q[1]), c = vertexOf(q[2]), d = vertexOf(q[3]);
            addTriangle(mesh, a, b, c);
            addTriangle(mesh, a, c, d);
        }
        // The twelve edges of the undeformed box.
        const int n1 = grid[0], n2 = grid[1], n3 = grid[2];
        const auto node = [&](int i, int j, int k) { return model.nodes[static_cast<size_t>(i + (n1 + 1) * (j + (n2 + 1) * k))]; };
        for (int a = 0; a <= 1; ++a)
            for (int b = 0; b <= 1; ++b) {
                mesh.outline.push_back({node(0, a * n2, b * n3), node(n1, a * n2, b * n3)});
                mesh.outline.push_back({node(a * n1, 0, b * n3), node(a * n1, n2, b * n3)});
                mesh.outline.push_back({node(a * n1, b * n2, 0), node(a * n1, b * n2, n3)});
            }
        break;
    }
    }
    computeNormals(mesh);
    return mesh;
}

}  // namespace resonant::mechanics
