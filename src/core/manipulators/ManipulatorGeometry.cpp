// The geometry kinds of wave manipulators: parameters, validation and
// display tessellation, and for reflectors the simulation surface and exact
// ray reflection. Every shape is generated from its mathematical definition.
#include "manipulators/ReflectorSurface.h"

#include "foundation/Units.h"
#include "manipulators/Analytics.h"
#include "manipulators/Helmholtz.h"
#include "manipulators/Horn.h"

#include <cmath>
#include <functional>

namespace resonant {

void registerManipulatorGeometry(GeometryRegistry& registry);

}  // namespace resonant

namespace resonant::manipulators {
namespace {

using units::pi;

ParameterDef length(std::string key, std::string label, double defaultValue)
{
    ParameterDef def;
    def.key = std::move(key);
    def.label = std::move(label);
    def.unit = "m";
    def.defaultValue = defaultValue;
    def.minValue = 0.0;
    def.maxValue = 1e6;
    return def;
}

ParameterDef number(std::string key, std::string label, double defaultValue, double minValue, double maxValue,
                    bool exclusiveMin = false)
{
    ParameterDef def;
    def.key = std::move(key);
    def.label = std::move(label);
    def.defaultValue = defaultValue;
    def.minValue = minValue;
    def.maxValue = maxValue;
    def.exclusiveMin = exclusiveMin;
    return def;
}

// A curve in the (r, z) half-plane, parametrized by t, with the unit normal
// (nr, nz) on the reflecting side and the arc-length speed ds/dt.
struct ProfileSample {
    double r, z, nr, nz, speed;
};
using Profile = std::function<ProfileSample(double t)>;
struct ProfileRange {
    Profile curve;
    double t0 = 0.0;
    double t1 = 1.0;
};

double arcLength(const ProfileRange& p)
{
    // Simpson's rule on the arc-length speed.
    const int n = 400;
    double sum = p.curve(p.t0).speed + p.curve(p.t1).speed;
    for (int i = 1; i < n; ++i)
        sum += (i % 2 ? 4.0 : 2.0) * p.curve(p.t0 + (p.t1 - p.t0) * i / n).speed;
    return sum * (p.t1 - p.t0) / (3.0 * n);
}

// The profile of an axisymmetric reflector kind, or an empty one.
ProfileRange profileOf(const Geometry& g)
{
    ProfileRange p;
    if (g.kindId() == "reflector.parabolic") {
        const double f = g.parameter("focalLength");
        p.t1 = 0.5 * g.parameter("aperture");  // t = r
        p.curve = [f](double r) {
            const double slope = r / (2.0 * f);
            const double norm = std::sqrt(1.0 + slope * slope);
            return ProfileSample{r, r * r / (4.0 * f), -slope / norm, 1.0 / norm, norm};
        };
    } else if (g.kindId() == "reflector.spherical") {
        const double R = g.parameter("curvatureRadius");
        p.t1 = std::asin(std::min(1.0, 0.5 * g.parameter("aperture") / R));  // t = angle at the centre
        p.curve = [R](double t) {
            return ProfileSample{R * std::sin(t), R - R * std::cos(t), -std::sin(t), std::cos(t), R};
        };
    } else if (g.kindId() == "reflector.elliptical") {
        const double a = g.parameter("semiMajor");
        const double b = g.parameter("semiMinor");
        p.t1 = std::acos(std::clamp(-g.parameter("cut"), -1.0, 1.0));  // t from the -Z pole
        p.curve = [a, b](double t) {
            const double r = b * std::sin(t);
            const double z = -a * std::cos(t);
            // Inward normal: minus the gradient of r^2/b^2 + z^2/a^2.
            const double gr = -r / (b * b);
            const double gz = -z / (a * a);
            const double norm = std::hypot(gr, gz);
            return ProfileSample{r, z, gr / norm, gz / norm, std::hypot(b * std::cos(t), a * std::sin(t))};
        };
    }
    return p;
}

// Sampling plan of a revolved profile: rings of nearly square elements.
struct Ring {
    double t, dt;
    int segments;
};
std::vector<Ring> ringsOf(const ProfileRange& p, double h)
{
    std::vector<Ring> rings;
    const int count = std::max(1, static_cast<int>(std::ceil(arcLength(p) / h)));
    const double dt = (p.t1 - p.t0) / count;
    for (int i = 0; i < count; ++i) {
        const double t = p.t0 + (i + 0.5) * dt;
        const double r = p.curve(t).r;
        rings.push_back({t, dt, std::max(6, static_cast<int>(std::ceil(2.0 * pi * r / h)))});
    }
    return rings;
}

std::vector<SurfaceElement> sampleRevolved(const ProfileRange& p, double h)
{
    std::vector<SurfaceElement> elements;
    for (const Ring& ring : ringsOf(p, h)) {
        const ProfileSample s = p.curve(ring.t);
        const double ringArea = 2.0 * pi * s.r * s.speed * ring.dt;
        for (int j = 0; j < ring.segments; ++j) {
            const double angle = 2.0 * pi * (j + 0.5) / ring.segments;
            const double c = std::cos(angle), sn = std::sin(angle);
            elements.push_back({Vec3(s.r * c, s.r * sn, s.z), Vec3(s.nr * c, s.nr * sn, s.nz), ringArea / ring.segments});
        }
    }
    return elements;
}

// Display mesh of a shell: the reflecting surface, a back surface offset by
// the thickness along -Z, and the rim joining them.
TriangleMesh shellMesh(const ProfileRange& p, double thickness, int segments)
{
    const int steps = std::max(8, segments / 2);
    std::vector<ProfilePoint> front, back;
    for (int i = 0; i <= steps; ++i) {
        const ProfileSample s = p.curve(p.t0 + (p.t1 - p.t0) * i / steps);
        front.push_back({s.r, s.z, s.nr, s.nz});
        back.push_back({s.r, s.z - thickness, -s.nr, -s.nz});
    }
    TriangleMesh mesh;
    revolveProfile(mesh, front, segments);
    revolveProfile(mesh, back, segments);
    const ProfileSample rim = p.curve(p.t1);
    revolveProfile(mesh, {{rim.r, rim.z, 1.0, 0.0}, {rim.r, rim.z - thickness, 1.0, 0.0}}, segments);
    return mesh;
}

void addQuad(TriangleMesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& normal)
{
    const auto ia = mesh.addVertex(a, normal), ib = mesh.addVertex(b, normal);
    const auto ic = mesh.addVertex(c, normal), id = mesh.addVertex(d, normal);
    mesh.addTriangle(ia, ib, ic);
    mesh.addTriangle(ia, ic, id);
}

void addBox(TriangleMesh& mesh, const Vec3& lo, const Vec3& hi)
{
    const double x0 = lo.x(), y0 = lo.y(), z0 = lo.z(), x1 = hi.x(), y1 = hi.y(), z1 = hi.z();
    addQuad(mesh, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, Vec3::UnitX());
    addQuad(mesh, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, -Vec3::UnitX());
    addQuad(mesh, {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, Vec3::UnitY());
    addQuad(mesh, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, -Vec3::UnitY());
    addQuad(mesh, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, Vec3::UnitZ());
    addQuad(mesh, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, -Vec3::UnitZ());
}

TriangleMesh troughMesh(const Geometry& g, const TessellationOptions& options)
{
    const double f = g.parameter("focalLength");
    const double halfWidth = 0.5 * g.parameter("width");
    const double halfLength = 0.5 * g.parameter("length");
    const double t = g.parameter("thickness");
    TriangleMesh mesh;
    const int steps = std::max(8, options.segments);
    for (int i = 0; i < steps; ++i) {
        const double xa = -halfWidth + 2.0 * halfWidth * i / steps;
        const double xb = -halfWidth + 2.0 * halfWidth * (i + 1) / steps;
        const double za = xa * xa / (4.0 * f), zb = xb * xb / (4.0 * f);
        const Vec3 normal = Vec3(-(xa + xb) / (4.0 * f), 0.0, 1.0).normalized();
        addQuad(mesh, {xa, -halfLength, za}, {xb, -halfLength, zb}, {xb, halfLength, zb}, {xa, halfLength, za}, normal);
        addQuad(mesh, {xa, -halfLength, za - t}, {xa, halfLength, za - t}, {xb, halfLength, zb - t},
                {xb, -halfLength, zb - t}, Vec3(-normal));
    }
    return mesh;
}

TriangleMesh lensMesh(const Geometry& g, const TessellationOptions& options)
{
    // Two spherical caps (or flats) on a cylinder. Surface 1 faces -Z.
    const double a = 0.5 * g.parameter("aperture");
    const double d = g.parameter("centerThickness");
    const double r1 = g.parameter("radius1");
    const double r2 = g.parameter("radius2");
    // Sagitta of a spherical surface of signed radius R at radius r.
    const auto sag = [](double R, double r) {
        return R == 0.0 ? 0.0 : (R > 0.0 ? 1.0 : -1.0) * (std::abs(R) - std::sqrt(std::max(0.0, R * R - r * r)));
    };
    const int steps = std::max(8, options.segments / 2);
    std::vector<ProfilePoint> first, second;
    for (int i = 0; i <= steps; ++i) {
        const double r = a * i / steps;
        // Surface 1 at z = -d/2 + sag(R1), outward normal towards -Z.
        const double slope1 = r1 == 0.0 ? 0.0 : r / std::sqrt(std::max(1e-300, r1 * r1 - r * r)) * (r1 > 0.0 ? 1.0 : -1.0);
        const double n1 = std::sqrt(1.0 + slope1 * slope1);
        first.push_back({r, -0.5 * d + sag(r1, r), slope1 / n1, -1.0 / n1});
        // Surface 2 at z = +d/2 + sag(R2) (R2 < 0 bulges towards +Z), outward normal towards +Z.
        const double slope2 = r2 == 0.0 ? 0.0 : r / std::sqrt(std::max(1e-300, r2 * r2 - r * r)) * (r2 > 0.0 ? 1.0 : -1.0);
        const double n2 = std::sqrt(1.0 + slope2 * slope2);
        second.push_back({r, 0.5 * d + sag(r2, r), -slope2 / n2, 1.0 / n2});
    }
    TriangleMesh mesh;
    revolveProfile(mesh, first, options.segments);
    revolveProfile(mesh, second, options.segments);
    revolveProfile(mesh, {{a, first.back().z, 1.0, 0.0}, {a, second.back().z, 1.0, 0.0}}, options.segments);
    return mesh;
}

Status validateLens(const Geometry& g)
{
    const double a = 0.5 * g.parameter("aperture");
    const double d = g.parameter("centerThickness");
    const double r1 = g.parameter("radius1");
    const double r2 = g.parameter("radius2");
    for (const double radius : {r1, r2})
        if (radius != 0.0 && std::abs(radius) < a)
            return Status::failure("a lens surface's radius of curvature must be at least half the aperture");
    const auto sag = [a](double R) {
        return R == 0.0 ? 0.0 : (R > 0.0 ? 1.0 : -1.0) * (std::abs(R) - std::sqrt(R * R - a * a));
    };
    // Edge thickness = centre thickness - sag1 + sag2.
    if (d - sag(r1) + sag(r2) <= 0.0)
        return Status::failure("the lens surfaces intersect: increase the centre thickness or the radii");
    return Status::success();
}

TriangleMesh tubeMesh(double innerA, double innerB, double length, double wall, int segments)
{
    // A tube along Z from -L/2 to +L/2 whose inner radius goes from innerA to innerB.
    const double h = 0.5 * length;
    const double slope = (innerB - innerA) / length;
    const double norm = std::sqrt(1.0 + slope * slope);
    TriangleMesh mesh;
    revolveProfile(mesh, {{innerA, -h, -1.0 / norm, slope / norm}, {innerB, h, -1.0 / norm, slope / norm}}, segments);
    revolveProfile(mesh, {{innerA + wall, -h, 1.0 / norm, -slope / norm}, {innerB + wall, h, 1.0 / norm, -slope / norm}}, segments);
    revolveProfile(mesh, {{innerA, -h, 0.0, -1.0}, {innerA + wall, -h, 0.0, -1.0}}, segments);
    revolveProfile(mesh, {{innerB, h, 0.0, 1.0}, {innerB + wall, h, 0.0, 1.0}}, segments);
    return mesh;
}

TriangleMesh rectangularDuctMesh(const Geometry& g, const TessellationOptions&)
{
    const double w = 0.5 * g.parameter("width"), hgt = 0.5 * g.parameter("height"), l = 0.5 * g.parameter("length");
    const double t = g.parameter("wallThickness");
    TriangleMesh mesh;
    addBox(mesh, {-w - t, -hgt - t, -l}, {-w, hgt + t, l});
    addBox(mesh, {w, -hgt - t, -l}, {w + t, hgt + t, l});
    addBox(mesh, {-w, -hgt - t, -l}, {w, -hgt, l});
    addBox(mesh, {-w, hgt, -l}, {w, hgt + t, l});
    return mesh;
}

GeometryKind kind(std::string id, std::string name, std::string category, std::vector<ParameterDef> parameters,
                  std::function<TriangleMesh(const Geometry&, const TessellationOptions&)> tessellate,
                  std::function<Status(const Geometry&)> validate = {})
{
    GeometryKind k;
    k.id = std::move(id);
    k.displayName = std::move(name);
    k.category = std::move(category);
    k.parameters = std::move(parameters);
    k.tessellate = std::move(tessellate);
    k.validate = std::move(validate);
    return k;
}

}  // namespace

bool isReflectorGeometry(const Geometry& geometry)
{
    return geometry.kind().category == "reflector";
}

std::vector<SurfaceElement> sampleReflectorSurface(const Geometry& g, double h)
{
    std::vector<SurfaceElement> elements;
    if (!(h > 0.0))
        return elements;
    if (const ProfileRange p = profileOf(g); p.curve)
        return sampleRevolved(p, h);
    if (g.kindId() == "reflector.cylindrical") {
        const double f = g.parameter("focalLength");
        const double halfWidth = 0.5 * g.parameter("width");
        const double length = g.parameter("length");
        // Arc length of the parabola from -halfWidth to +halfWidth.
        const double u = halfWidth / (2.0 * f);
        const double arc = 2.0 * f * (u * std::sqrt(1.0 + u * u) + std::asinh(u));
        const int across = std::max(1, static_cast<int>(std::ceil(arc / h)));
        const int along = std::max(1, static_cast<int>(std::ceil(length / h)));
        for (int i = 0; i < across; ++i) {
            const double x = -halfWidth + 2.0 * halfWidth * (i + 0.5) / across;
            const double slope = x / (2.0 * f);
            const double norm = std::sqrt(1.0 + slope * slope);
            const double strip = norm * (2.0 * halfWidth / across) * (length / along);
            for (int j = 0; j < along; ++j)
                elements.push_back({Vec3(x, -0.5 * length + length * (j + 0.5) / along, x * x / (4.0 * f)),
                                    Vec3(-slope / norm, 0.0, 1.0 / norm), strip});
        }
    } else if (g.kindId() == "reflector.flat") {
        const double w = g.parameter("width"), hgt = g.parameter("height");
        const int nx = std::max(1, static_cast<int>(std::ceil(w / h)));
        const int ny = std::max(1, static_cast<int>(std::ceil(hgt / h)));
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
                elements.push_back({Vec3(-0.5 * w + w * (i + 0.5) / nx, -0.5 * hgt + hgt * (j + 0.5) / ny, 0.0),
                                    Vec3::UnitZ(), (w / nx) * (hgt / ny)});
    }
    return elements;
}

size_t estimateSurfaceElements(const Geometry& g, double h)
{
    if (!(h > 0.0))
        return 0;
    if (const ProfileRange p = profileOf(g); p.curve) {
        size_t count = 0;
        for (const Ring& ring : ringsOf(p, h))
            count += static_cast<size_t>(ring.segments);
        return count;
    }
    // Area / h^2 is within a few percent of the exact count for the others.
    return static_cast<size_t>(std::ceil(reflectorArea(g) / (h * h)));
}

double reflectorArea(const Geometry& g)
{
    if (const ProfileRange p = profileOf(g); p.curve) {
        // 2 pi * integral of r ds, by Simpson's rule.
        const int n = 2000;
        const auto f = [&p](double t) {
            const ProfileSample s = p.curve(t);
            return s.r * s.speed;
        };
        double sum = f(p.t0) + f(p.t1);
        for (int i = 1; i < n; ++i)
            sum += (i % 2 ? 4.0 : 2.0) * f(p.t0 + (p.t1 - p.t0) * i / n);
        return 2.0 * pi * sum * (p.t1 - p.t0) / (3.0 * n);
    }
    if (g.kindId() == "reflector.cylindrical") {
        const double f = g.parameter("focalLength");
        const double u = 0.5 * g.parameter("width") / (2.0 * f);
        return 2.0 * f * (u * std::sqrt(1.0 + u * u) + std::asinh(u)) * g.parameter("length");
    }
    if (g.kindId() == "reflector.flat")
        return g.parameter("width") * g.parameter("height");
    return 0.0;
}

std::vector<NamedPoint> characteristicPoints(const Geometry& g)
{
    if (g.kindId() == "reflector.parabolic")
        return {{"Focus", Vec3(0, 0, g.parameter("focalLength"))}};
    if (g.kindId() == "reflector.cylindrical")
        return {{"Focal line (centre)", Vec3(0, 0, g.parameter("focalLength"))}};
    if (g.kindId() == "reflector.spherical")
        return {{"Paraxial focus (R/2)", Vec3(0, 0, 0.5 * g.parameter("curvatureRadius"))},
                {"Centre of curvature", Vec3(0, 0, g.parameter("curvatureRadius"))}};
    if (g.kindId() == "reflector.elliptical") {
        const double c = ellipseFocalDistance(g.parameter("semiMajor"), g.parameter("semiMinor"));
        return {{"Focus 1", Vec3(0, 0, -c)}, {"Focus 2", Vec3(0, 0, c)}};
    }
    return {};
}

std::optional<RayHit> reflectRay(const Geometry& g, const Ray& ray)
{
    const Vec3 o = ray.origin;
    const Vec3 d = ray.direction.normalized();
    // Intersections t > 0 of the ray with the surface, as roots of a
    // quadratic A t^2 + B t + C = 0, then the limits of the finite surface.
    double A = 0.0, B = 0.0, C = 0.0;
    std::function<bool(const Vec3&)> onSurface;
    std::function<Vec3(const Vec3&)> normalAt;

    if (g.kindId() == "reflector.parabolic") {
        const double f = g.parameter("focalLength");
        const double rim = 0.5 * g.parameter("aperture");
        A = d.x() * d.x() + d.y() * d.y();
        B = 2.0 * (o.x() * d.x() + o.y() * d.y()) - 4.0 * f * d.z();
        C = o.x() * o.x() + o.y() * o.y() - 4.0 * f * o.z();
        onSurface = [rim](const Vec3& p) { return std::hypot(p.x(), p.y()) <= rim * (1.0 + 1e-12); };
        normalAt = [f](const Vec3& p) { return Vec3(-p.x() / (2.0 * f), -p.y() / (2.0 * f), 1.0).normalized(); };
    } else if (g.kindId() == "reflector.cylindrical") {
        const double f = g.parameter("focalLength");
        const double halfWidth = 0.5 * g.parameter("width"), halfLength = 0.5 * g.parameter("length");
        A = d.x() * d.x();
        B = 2.0 * o.x() * d.x() - 4.0 * f * d.z();
        C = o.x() * o.x() - 4.0 * f * o.z();
        onSurface = [=](const Vec3& p) { return std::abs(p.x()) <= halfWidth * (1.0 + 1e-12) && std::abs(p.y()) <= halfLength * (1.0 + 1e-12); };
        normalAt = [f](const Vec3& p) { return Vec3(-p.x() / (2.0 * f), 0.0, 1.0).normalized(); };
    } else if (g.kindId() == "reflector.spherical") {
        const double R = g.parameter("curvatureRadius");
        const double rim = 0.5 * g.parameter("aperture");
        const Vec3 centre(0, 0, R);
        A = 1.0;
        B = 2.0 * d.dot(o - centre);
        C = (o - centre).squaredNorm() - R * R;
        onSurface = [rim, R](const Vec3& p) { return p.z() <= R && std::hypot(p.x(), p.y()) <= rim * (1.0 + 1e-12); };
        normalAt = [centre](const Vec3& p) { return Vec3((centre - p).normalized()); };
    } else if (g.kindId() == "reflector.elliptical") {
        const double a = g.parameter("semiMajor"), b = g.parameter("semiMinor");
        const double zMax = g.parameter("cut") * a;
        A = (d.x() * d.x() + d.y() * d.y()) / (b * b) + d.z() * d.z() / (a * a);
        B = 2.0 * ((o.x() * d.x() + o.y() * d.y()) / (b * b) + o.z() * d.z() / (a * a));
        C = (o.x() * o.x() + o.y() * o.y()) / (b * b) + o.z() * o.z() / (a * a) - 1.0;
        onSurface = [zMax, a](const Vec3& p) { return p.z() <= zMax + 1e-12 * a; };
        normalAt = [a, b](const Vec3& p) { return Vec3(-p.x() / (b * b), -p.y() / (b * b), -p.z() / (a * a)).normalized(); };
    } else if (g.kindId() == "reflector.flat") {
        if (std::abs(d.z()) < 1e-15)
            return std::nullopt;
        const double t = -o.z() / d.z();
        const Vec3 p = o + t * d;
        if (t <= 1e-12 || std::abs(p.x()) > 0.5 * g.parameter("width") || std::abs(p.y()) > 0.5 * g.parameter("height"))
            return std::nullopt;
        const Vec3 n = d.z() < 0.0 ? Vec3(Vec3::UnitZ()) : Vec3(-Vec3::UnitZ());
        return RayHit{p, n, (d - 2.0 * d.dot(n) * n).normalized(), t};
    } else {
        return std::nullopt;
    }

    std::vector<double> roots;
    if (std::abs(A) < 1e-18) {
        if (std::abs(B) > 1e-18)
            roots.push_back(-C / B);
    } else {
        const double discriminant = B * B - 4.0 * A * C;
        if (discriminant < 0.0)
            return std::nullopt;
        const double root = std::sqrt(discriminant);
        roots = {(-B - root) / (2.0 * A), (-B + root) / (2.0 * A)};
    }
    std::optional<RayHit> best;
    for (const double t : roots) {
        if (t <= 1e-9)
            continue;
        const Vec3 p = o + t * d;
        if (!onSurface(p) || (best && t >= best->distance))
            continue;
        Vec3 n = normalAt(p);
        if (n.dot(d) > 0.0)
            n = -n;  // a thin shell reflects on whichever side is hit
        best = RayHit{p, n, (d - 2.0 * d.dot(n) * n).normalized(), t};
    }
    return best;
}

}  // namespace resonant::manipulators

namespace resonant {

void registerManipulatorGeometry(GeometryRegistry& registry)
{
    using namespace manipulators;
    const auto shell = [](const Geometry& g, const TessellationOptions& o) {
        return shellMesh(profileOf(g), g.parameter("thickness"), o.segments);
    };

    registry.add(kind("reflector.parabolic", "Parabolic reflector", "reflector",
                      {length("focalLength", "Focal length", 0.05), length("aperture", "Aperture diameter", 0.2),
                       length("thickness", "Thickness", 0.003)},
                      shell));
    registry.add(kind("reflector.elliptical", "Elliptical reflector", "reflector",
                      {length("semiMajor", "Semi-major axis a (along Z)", 0.15),
                       length("semiMinor", "Semi-minor axis b", 0.09),
                       number("cut", "Aperture cut (z / a, -1 to 1)", 0.0, -0.98, 0.98),
                       length("thickness", "Thickness", 0.003)},
                      shell, [](const Geometry& g) {
                          return g.parameter("semiMinor") < g.parameter("semiMajor")
                                     ? Status::success()
                                     : Status::failure("the semi-minor axis must be smaller than the semi-major axis");
                      }));
    registry.add(kind("reflector.spherical", "Spherical reflector", "reflector",
                      {length("curvatureRadius", "Radius of curvature", 0.2), length("aperture", "Aperture diameter", 0.2),
                       length("thickness", "Thickness", 0.003)},
                      shell, [](const Geometry& g) {
                          return g.parameter("aperture") <= 2.0 * g.parameter("curvatureRadius")
                                     ? Status::success()
                                     : Status::failure("the aperture cannot exceed twice the radius of curvature");
                      }));
    registry.add(kind("reflector.cylindrical", "Cylindrical (trough) reflector", "reflector",
                      {length("focalLength", "Focal length", 0.05), length("width", "Aperture width (X)", 0.2),
                       length("length", "Length (Y)", 0.3), length("thickness", "Thickness", 0.003)},
                      troughMesh));
    registry.add(kind("reflector.flat", "Flat reflector", "reflector",
                      {length("width", "Width (X)", 0.2), length("height", "Height (Y)", 0.2),
                       length("thickness", "Thickness", 0.003)},
                      [](const Geometry& g, const TessellationOptions&) {
                          TriangleMesh mesh;
                          addBox(mesh, {-0.5 * g.parameter("width"), -0.5 * g.parameter("height"), -g.parameter("thickness")},
                                 {0.5 * g.parameter("width"), 0.5 * g.parameter("height"), 0.0});
                          return mesh;
                      }));

    ParameterDef radius1 = number("radius1", "Surface 1 radius (0 = flat)", 0.15, -1e6, 1e6);
    radius1.unit = "m";
    ParameterDef radius2 = number("radius2", "Surface 2 radius (0 = flat)", -0.15, -1e6, 1e6);
    radius2.unit = "m";
    registry.add(kind("lens.spherical", "Acoustic lens (spherical surfaces)", "lens",
                      {length("aperture", "Aperture diameter", 0.1), radius1, radius2,
                       length("centerThickness", "Centre thickness", 0.03)},
                      lensMesh, validateLens));

    registry.add(kind("waveguide.circular", "Circular waveguide", "waveguide",
                      {length("radius", "Inner radius (entry)", 0.02), length("endRadius", "Inner radius (exit)", 0.02),
                       length("length", "Length (Z)", 0.3), length("wallThickness", "Wall thickness", 0.002)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          return tubeMesh(g.parameter("radius"), g.parameter("endRadius"), g.parameter("length"),
                                          g.parameter("wallThickness"), o.segments);
                      }));
    {
        ParameterDef profile = number("profile", "Profile (0 conical, 1 exponential, 2 hyperbolic, 3 uniform)", 1.0, 0.0, 3.0);
        profile.integer = true;
        registry.add(kind("horn.acoustic", "Acoustic horn", "waveguide",
                          {length("throatRadius", "Throat radius (-Z end)", 0.02), length("mouthRadius", "Mouth radius (+Z end)", 0.15),
                           length("length", "Length (Z)", 0.5), profile, number("flare", "Hyperbolic flare T (0 - 1)", 0.7, 0.0, 1.0, true),
                           length("wallThickness", "Wall thickness", 0.003)},
                          [](const Geometry& g, const TessellationOptions& o) {
                              const HornSpec h = hornSpecOf(g);
                              const double L = h.length, t = g.parameter("wallThickness");
                              const int n = 48;
                              std::vector<ProfilePoint> inner, outer;
                              for (int i = 0; i <= n; ++i) {
                                  const double x = L * i / n, dx = 1e-4 * L;
                                  const double r = hornRadius(h, x);
                                  const double x1 = std::min(L, x + dx), x0 = std::max(0.0, x - dx);
                                  const double slope = (hornRadius(h, x1) - hornRadius(h, x0)) / (x1 - x0);
                                  const double norm = std::sqrt(1.0 + slope * slope);
                                  inner.push_back({r, x - 0.5 * L, -1.0 / norm, slope / norm});
                                  outer.push_back({r + t, x - 0.5 * L, 1.0 / norm, -slope / norm});
                              }
                              TriangleMesh mesh;
                              revolveProfile(mesh, inner, o.segments);
                              revolveProfile(mesh, outer, o.segments);
                              revolveProfile(mesh, {{h.throatRadius, -0.5 * L, 0.0, -1.0}, {h.throatRadius + t, -0.5 * L, 0.0, -1.0}}, o.segments);
                              revolveProfile(mesh, {{h.mouthRadius, 0.5 * L, 0.0, 1.0}, {h.mouthRadius + t, 0.5 * L, 0.0, 1.0}}, o.segments);
                              return mesh;
                          },
                          [](const Geometry& g) { return hornSpecOf(g).validate(); }));
    }
    // A Helmholtz resonator: a closed cylindrical cavity centred on the origin, its neck along +Z.
    registry.add(kind("resonator.helmholtz", "Helmholtz resonator", "cavity",
                      {length("cavityRadius", "Cavity radius", 0.025), length("cavityLength", "Cavity length (Z)", 0.05),
                       length("neckRadius", "Neck radius", 0.005), length("neckLength", "Neck length", 0.01),
                       length("wallThickness", "Wall thickness", 0.002)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          const double R = g.parameter("cavityRadius"), h = 0.5 * g.parameter("cavityLength"), a = g.parameter("neckRadius"),
                                       L = g.parameter("neckLength"), t = g.parameter("wallThickness");
                          TriangleMesh mesh;
                          const auto segment = [&](double r0, double z0, double r1, double z1, double nr, double nz) {
                              revolveProfile(mesh, {{r0, z0, nr, nz}, {r1, z1, nr, nz}}, o.segments);
                          };
                          segment(0.0, -h - t, R + t, -h - t, 0.0, -1.0);  // outer bottom
                          segment(R + t, -h - t, R + t, h + t, 1.0, 0.0);  // outer side
                          segment(R + t, h + t, a + t, h + t, 0.0, 1.0);   // outer top
                          segment(a + t, h + t, a + t, h + L, 1.0, 0.0);   // neck outside
                          segment(a + t, h + L, a, h + L, 0.0, 1.0);       // neck rim
                          segment(a, h + L, a, h, -1.0, 0.0);              // neck bore
                          segment(a, h, R, h, 0.0, -1.0);                  // inner top
                          segment(R, h, R, -h, -1.0, 0.0);                 // inner side
                          segment(R, -h, 0.0, -h, 0.0, 1.0);               // inner bottom
                          return mesh;
                      },
                      [](const Geometry& g) { return helmholtzSpecOf(g).validate(); }));
    registry.add(kind("waveguide.rectangular", "Rectangular waveguide", "waveguide",
                      {length("width", "Inner width (X)", 0.04), length("height", "Inner height (Y)", 0.02),
                       length("length", "Length (Z)", 0.3), length("wallThickness", "Wall thickness", 0.002)},
                      rectangularDuctMesh));

    registry.add(kind("cavity.rectangular", "Rectangular cavity", "cavity",
                      {length("sizeX", "Inner size X", 0.3), length("sizeY", "Inner size Y", 0.2),
                       length("sizeZ", "Inner size Z", 0.15), length("wallThickness", "Wall thickness", 0.005)},
                      [](const Geometry& g, const TessellationOptions&) {
                          const double t = g.parameter("wallThickness");
                          const Vec3 half(0.5 * g.parameter("sizeX") + t, 0.5 * g.parameter("sizeY") + t, 0.5 * g.parameter("sizeZ") + t);
                          TriangleMesh mesh;
                          addBox(mesh, -half, half);
                          return mesh;
                      }));
    registry.add(kind("cavity.cylindrical", "Cylindrical cavity", "cavity",
                      {length("radius", "Inner radius", 0.1), length("length", "Inner length (Z)", 0.3),
                       length("wallThickness", "Wall thickness", 0.005)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          const double r = g.parameter("radius") + g.parameter("wallThickness");
                          const double h = 0.5 * g.parameter("length") + g.parameter("wallThickness");
                          TriangleMesh mesh;
                          revolveProfile(mesh, {{r, -h, 1.0, 0.0}, {r, h, 1.0, 0.0}}, o.segments);
                          revolveProfile(mesh, {{0.0, h, 0.0, 1.0}, {r, h, 0.0, 1.0}}, o.segments);
                          revolveProfile(mesh, {{0.0, -h, 0.0, -1.0}, {r, -h, 0.0, -1.0}}, o.segments);
                          return mesh;
                      }));
    registry.add(kind("cavity.spherical", "Spherical cavity", "cavity",
                      {length("radius", "Inner radius", 0.1), length("wallThickness", "Wall thickness", 0.005)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          auto sphere = Geometry::create("sphere").take();
                          [[maybe_unused]] const Status status = sphere.setParameter("radius", g.parameter("radius") + g.parameter("wallThickness"));
                          return sphere.tessellate(o);
                      }));

    registry.add(kind("diffraction.slit", "Plate with a slit", "diffraction",
                      {length("width", "Plate width (X)", 0.2), length("height", "Plate height (Y)", 0.2),
                       length("slitWidth", "Slit width (X)", 0.01), length("thickness", "Thickness", 0.003)},
                      [](const Geometry& g, const TessellationOptions&) {
                          const double w = 0.5 * g.parameter("width"), h = 0.5 * g.parameter("height");
                          const double s = 0.5 * g.parameter("slitWidth"), t = g.parameter("thickness");
                          TriangleMesh mesh;
                          addBox(mesh, {-w, -h, -t}, {-s, h, 0.0});
                          addBox(mesh, {s, -h, -t}, {w, h, 0.0});
                          return mesh;
                      },
                      [](const Geometry& g) {
                          return g.parameter("slitWidth") < g.parameter("width")
                                     ? Status::success()
                                     : Status::failure("the slit must be narrower than the plate");
                      }));
}

}  // namespace resonant
