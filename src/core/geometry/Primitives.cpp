// The primitive geometry kinds. All are centred on the local origin.
//
//   box        width (X), depth (Y), height (Z)
//   sphere     radius
//   cylinder   radius, height; axis along Z
//   cone       radius of the base, height; base at -height/2, apex at +height/2
//   plane      width (X), depth (Y); a rectangle in the XY plane, normal +Z
//   disc       radius; a circle in the XY plane, normal +Z
//   ellipsoid  semi-axes radiusX, radiusY, radiusZ
#include "geometry/Geometry.h"

#include "foundation/Units.h"

#include <cmath>

namespace resonant {
namespace {

ParameterDef length(std::string key, std::string label, double defaultValue)
{
    ParameterDef def;
    def.key = std::move(key);
    def.label = std::move(label);
    def.unit = "m";
    def.defaultValue = defaultValue;
    def.minValue = 0.0;
    def.maxValue = 1e6;
    def.exclusiveMin = true;
    return def;
}

void addQuad(TriangleMesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& normal)
{
    const auto ia = mesh.addVertex(a, normal);
    const auto ib = mesh.addVertex(b, normal);
    const auto ic = mesh.addVertex(c, normal);
    const auto id = mesh.addVertex(d, normal);
    mesh.addTriangle(ia, ib, ic);
    mesh.addTriangle(ia, ic, id);
}

TriangleMesh tessellateBox(const Geometry& g, const TessellationOptions&)
{
    const double x = 0.5 * g.parameter("width");
    const double y = 0.5 * g.parameter("depth");
    const double z = 0.5 * g.parameter("height");
    TriangleMesh mesh;
    addQuad(mesh, {x, -y, -z}, {x, y, -z}, {x, y, z}, {x, -y, z}, Vec3::UnitX());
    addQuad(mesh, {-x, -y, -z}, {-x, -y, z}, {-x, y, z}, {-x, y, -z}, -Vec3::UnitX());
    addQuad(mesh, {-x, y, -z}, {-x, y, z}, {x, y, z}, {x, y, -z}, Vec3::UnitY());
    addQuad(mesh, {-x, -y, -z}, {x, -y, -z}, {x, -y, z}, {-x, -y, z}, -Vec3::UnitY());
    addQuad(mesh, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}, Vec3::UnitZ());
    addQuad(mesh, {-x, -y, -z}, {-x, y, -z}, {x, y, -z}, {x, -y, -z}, -Vec3::UnitZ());
    return mesh;
}

TriangleMesh tessellateEllipsoidWith(double rx, double ry, double rz, int segments)
{
    TriangleMesh mesh;
    const int rings = std::max(4, segments / 2);
    const int columns = segments + 1;
    for (int i = 0; i <= rings; ++i) {
        const double polar = units::pi * static_cast<double>(i) / rings;  // 0 at +Z
        const double sp = std::sin(polar);
        const double cp = std::cos(polar);
        for (int j = 0; j < columns; ++j) {
            const double azimuth = 2.0 * units::pi * static_cast<double>(j) / segments;
            const Vec3 unit(sp * std::cos(azimuth), sp * std::sin(azimuth), cp);
            const Vec3 position(rx * unit.x(), ry * unit.y(), rz * unit.z());
            // Gradient of x^2/rx^2 + y^2/ry^2 + z^2/rz^2.
            const Vec3 normal = Vec3(unit.x() / rx, unit.y() / ry, unit.z() / rz).normalized();
            mesh.addVertex(position, normal);
        }
    }
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const auto a = static_cast<std::uint32_t>(i * columns + j);
            const auto b = a + 1;
            const auto d = static_cast<std::uint32_t>((i + 1) * columns + j);
            const auto c = d + 1;
            mesh.addTriangle(a, b, c);
            mesh.addTriangle(a, c, d);
        }
    }
    return mesh;
}

TriangleMesh tessellateSphere(const Geometry& g, const TessellationOptions& options)
{
    const double r = g.parameter("radius");
    return tessellateEllipsoidWith(r, r, r, options.segments);
}

TriangleMesh tessellateEllipsoid(const Geometry& g, const TessellationOptions& options)
{
    return tessellateEllipsoidWith(g.parameter("radiusX"), g.parameter("radiusY"), g.parameter("radiusZ"),
                                   options.segments);
}

TriangleMesh tessellateCylinder(const Geometry& g, const TessellationOptions& options)
{
    const double r = g.parameter("radius");
    const double h = 0.5 * g.parameter("height");
    TriangleMesh mesh;
    revolveProfile(mesh, {{r, -h, 1.0, 0.0}, {r, h, 1.0, 0.0}}, options.segments);
    revolveProfile(mesh, {{0.0, h, 0.0, 1.0}, {r, h, 0.0, 1.0}}, options.segments);
    revolveProfile(mesh, {{0.0, -h, 0.0, -1.0}, {r, -h, 0.0, -1.0}}, options.segments);
    return mesh;
}

TriangleMesh tessellateCone(const Geometry& g, const TessellationOptions& options)
{
    const double r = g.parameter("radius");
    const double height = g.parameter("height");
    const double h = 0.5 * height;
    // Outward normal of the slanted side in the (r, z) plane.
    const double slant = std::hypot(r, height);
    const double nr = height / slant;
    const double nz = r / slant;
    TriangleMesh mesh;
    revolveProfile(mesh, {{r, -h, nr, nz}, {0.0, h, nr, nz}}, options.segments);
    revolveProfile(mesh, {{0.0, -h, 0.0, -1.0}, {r, -h, 0.0, -1.0}}, options.segments);
    return mesh;
}

TriangleMesh tessellatePlane(const Geometry& g, const TessellationOptions&)
{
    const double x = 0.5 * g.parameter("width");
    const double y = 0.5 * g.parameter("depth");
    TriangleMesh mesh;
    addQuad(mesh, {-x, -y, 0.0}, {x, -y, 0.0}, {x, y, 0.0}, {-x, y, 0.0}, Vec3::UnitZ());
    return mesh;
}

TriangleMesh tessellateDisc(const Geometry& g, const TessellationOptions& options)
{
    TriangleMesh mesh;
    revolveProfile(mesh, {{0.0, 0.0, 0.0, 1.0}, {g.parameter("radius"), 0.0, 0.0, 1.0}}, options.segments);
    return mesh;
}

GeometryKind makeKind(std::string id, std::string name, std::vector<ParameterDef> parameters,
                      TriangleMesh (*tessellate)(const Geometry&, const TessellationOptions&))
{
    GeometryKind kind;
    kind.id = std::move(id);
    kind.displayName = std::move(name);
    kind.category = "primitive";
    kind.parameters = std::move(parameters);
    kind.tessellate = tessellate;
    return kind;
}

}  // namespace

void registerPrimitiveGeometry(GeometryRegistry& registry)
{
    registry.add(makeKind("box", "Box",
                          {length("width", "Width (X)", 1.0), length("depth", "Depth (Y)", 1.0),
                           length("height", "Height (Z)", 1.0)},
                          tessellateBox));
    registry.add(makeKind("sphere", "Sphere", {length("radius", "Radius", 0.5)}, tessellateSphere));
    registry.add(makeKind("cylinder", "Cylinder", {length("radius", "Radius", 0.5), length("height", "Height (Z)", 1.0)},
                          tessellateCylinder));
    registry.add(makeKind("cone", "Cone", {length("radius", "Base radius", 0.5), length("height", "Height (Z)", 1.0)},
                          tessellateCone));
    registry.add(makeKind("plane", "Plane", {length("width", "Width (X)", 2.0), length("depth", "Depth (Y)", 2.0)},
                          tessellatePlane));
    registry.add(makeKind("disc", "Disc", {length("radius", "Radius", 0.5)}, tessellateDisc));
    registry.add(makeKind("ellipsoid", "Ellipsoid",
                          {length("radiusX", "Semi-axis X", 0.75), length("radiusY", "Semi-axis Y", 0.5),
                           length("radiusZ", "Semi-axis Z", 0.35)},
                          tessellateEllipsoid));
}

}  // namespace resonant
