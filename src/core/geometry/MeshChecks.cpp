#include "geometry/MeshChecks.h"

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace resonant {

namespace {

struct Welded {
    std::vector<Vec3> points;
    std::vector<std::array<size_t, 3>> triangles;
};

Welded weld(const TriangleMesh& mesh, double tolerance)
{
    Welded w;
    std::map<std::tuple<long long, long long, long long>, size_t> index;
    const double q = tolerance > 0.0 ? tolerance : 1e-9;
    std::vector<size_t> remap(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const Vec3 p = mesh.positions[i].cast<double>();
        const auto key = std::make_tuple(std::llround(p.x() / q), std::llround(p.y() / q), std::llround(p.z() / q));
        const auto it = index.find(key);
        if (it == index.end()) {
            index.emplace(key, w.points.size());
            remap[i] = w.points.size();
            w.points.push_back(p);
        } else {
            remap[i] = it->second;
        }
    }
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        w.triangles.push_back({remap[mesh.indices[t]], remap[mesh.indices[t + 1]], remap[mesh.indices[t + 2]]});
    return w;
}

}  // namespace

MeshIntegrity checkMesh(const TriangleMesh& mesh, double weldTolerance)
{
    MeshIntegrity r;
    const Welded w = weld(mesh, weldTolerance);
    r.triangles = w.triangles.size();
    r.vertices = w.points.size();
    std::map<std::pair<size_t, size_t>, int> edges;
    for (const auto& t : w.triangles) {
        const Vec3 a = w.points[t[0]], b = w.points[t[1]], c = w.points[t[2]];
        const double area = 0.5 * (b - a).cross(c - a).norm();
        if (area <= 1e-24 || t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) {
            ++r.degenerate;
            continue;
        }
        r.area += area;
        r.volume += a.dot(b.cross(c)) / 6.0;
        for (int k = 0; k < 3; ++k) {
            size_t i = t[static_cast<size_t>(k)], j = t[static_cast<size_t>((k + 1) % 3)];
            if (i > j)
                std::swap(i, j);
            ++edges[{i, j}];
        }
        r.bounds.extend(a);
        r.bounds.extend(b);
        r.bounds.extend(c);
    }
    for (const auto& [e, n] : edges) {
        if (n == 1)
            ++r.boundaryEdges;
        else if (n > 2)
            ++r.nonManifoldEdges;
    }
    r.volume = std::abs(r.volume);
    r.closed = r.triangles > 0 && r.boundaryEdges == 0 && r.nonManifoldEdges == 0;
    return r;
}

double overhangFraction(const TriangleMesh& mesh, const Vec3& buildDirection, double maxOverhangRad)
{
    const Vec3 up = buildDirection.normalized();
    const Aabb b = mesh.bounds();
    const double base = b.isValid() ? b.min.dot(up) : 0.0;
    double total = 0.0, over = 0.0;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vec3 a = mesh.positions[mesh.indices[t]].cast<double>();
        const Vec3 p = mesh.positions[mesh.indices[t + 1]].cast<double>();
        const Vec3 c = mesh.positions[mesh.indices[t + 2]].cast<double>();
        Vec3 n = (p - a).cross(c - a);
        const double area = 0.5 * n.norm();
        if (area <= 0.0)
            continue;
        n.normalize();
        // Orient by the vertex normals (the mesh's outward side).
        const Vec3 vn = (mesh.normals[mesh.indices[t]] + mesh.normals[mesh.indices[t + 1]] + mesh.normals[mesh.indices[t + 2]]).cast<double>();
        if (vn.dot(n) < 0.0)
            n = -n;
        total += area;
        const double height = std::min({a.dot(up), p.dot(up), c.dot(up)});
        const bool onPlate = std::abs(height - base) < 1e-9 && std::abs(std::max({a.dot(up), p.dot(up), c.dot(up)}) - base) < 1e-9;
        // Facing down more steeply than the limit: angle from -up below (90 deg - limit).
        if (!onPlate && n.dot(-up) > std::cos(0.5 * 3.14159265358979323846 - maxOverhangRad) + 1e-12)
            over += area;
    }
    return total > 0.0 ? over / total : 0.0;
}

PrintabilityReport checkPrintability(const TriangleMesh& mesh, const PrintabilityLimits& limits, std::optional<double> wall,
                                     std::optional<double> feature)
{
    PrintabilityReport r;
    r.integrity = checkMesh(mesh);
    const auto mm = [](double v) {
        std::ostringstream s;
        s.precision(4);
        s << v * 1e3 << " mm";
        return s.str();
    };
    if (r.integrity.triangles == 0)
        r.failures.push_back("the mesh is empty");
    if (!r.integrity.closed)
        r.failures.push_back("the mesh is not closed (" + std::to_string(r.integrity.boundaryEdges) + " open and " +
                             std::to_string(r.integrity.nonManifoldEdges) + " non-manifold edges): it does not enclose a solid");
    if (r.integrity.degenerate)
        r.warnings.push_back(std::to_string(r.integrity.degenerate) + " degenerate triangle(s)");
    if (limits.minimumWall) {
        if (!wall)
            r.warnings.push_back("the wall thickness is not known from the design; it was not checked");
        else if (*wall < *limits.minimumWall)
            r.failures.push_back("wall thickness " + mm(*wall) + " is below the minimum " + mm(*limits.minimumWall));
    }
    if (limits.minimumFeature) {
        if (!feature)
            r.warnings.push_back("the smallest feature size is not known from the design; it was not checked");
        else if (*feature < *limits.minimumFeature)
            r.failures.push_back("feature size " + mm(*feature) + " is below the minimum " + mm(*limits.minimumFeature));
    }
    if (limits.buildVolume && r.integrity.bounds.isValid()) {
        const Vec3 size = r.integrity.bounds.size();
        for (int a = 0; a < 3; ++a)
            if (size[a] > (*limits.buildVolume)[a])
                r.failures.push_back(std::string("the part is ") + mm(size[a]) + " along " + "xyz"[a] + ", the build volume " +
                                     mm((*limits.buildVolume)[a]));
    }
    r.overhang = overhangFraction(mesh, limits.buildDirection, limits.maxOverhangRad);
    if (r.overhang > limits.allowedOverhangFraction)
        r.warnings.push_back(std::to_string(static_cast<int>(std::round(100.0 * r.overhang))) +
                             " % of the surface overhangs beyond the limit: support material or another build direction is needed");
    return r;
}

Status writeStl(const TriangleMesh& mesh, const std::filesystem::path& path, const std::string& name)
{
    std::ofstream out(path);
    if (!out)
        return Status::failure("cannot write " + path.string());
    out.precision(9);
    out << "solid " << name << "\n";
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vec3 a = mesh.positions[mesh.indices[t]].cast<double>();
        const Vec3 b = mesh.positions[mesh.indices[t + 1]].cast<double>();
        const Vec3 c = mesh.positions[mesh.indices[t + 2]].cast<double>();
        Vec3 n = (b - a).cross(c - a);
        if (n.norm() > 0.0)
            n.normalize();
        out << "  facet normal " << n.x() << " " << n.y() << " " << n.z() << "\n    outer loop\n";
        for (const Vec3& v : {a, b, c})
            out << "      vertex " << v.x() << " " << v.y() << " " << v.z() << "\n";
        out << "    endloop\n  endfacet\n";
    }
    out << "endsolid " << name << "\n";
    return out ? Status::success() : Status::failure("writing " + path.string() + " failed");
}

Status writeObj(const TriangleMesh& mesh, const std::filesystem::path& path, const std::string& comment)
{
    std::ofstream out(path);
    if (!out)
        return Status::failure("cannot write " + path.string());
    out.precision(9);
    out << "# Resonant mesh export, units: metres\n";
    if (!comment.empty()) {
        std::istringstream lines(comment);
        for (std::string line; std::getline(lines, line);)
            out << "# " << line << "\n";
    }
    for (const auto& p : mesh.positions)
        out << "v " << p.x() << " " << p.y() << " " << p.z() << "\n";
    for (const auto& n : mesh.normals)
        out << "vn " << n.x() << " " << n.y() << " " << n.z() << "\n";
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        out << "f";
        for (int k = 0; k < 3; ++k) {
            const auto i = mesh.indices[t + static_cast<size_t>(k)] + 1;
            out << " " << i << "//" << i;
        }
        out << "\n";
    }
    return out ? Status::success() : Status::failure("writing " + path.string() + " failed");
}

}  // namespace resonant
