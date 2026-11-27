#include "fdtd/SolidModel.h"

#include <algorithm>
#include <cmath>

namespace resonant::fdtd {
namespace {

std::string thickened(double designed, double used)
{
    char buffer[200];
    std::snprintf(buffer, sizeof buffer,
                  "its thickness of %.3g mm is below what the grid resolves and is represented as %.3g mm", designed * 1e3, used * 1e3);
    return buffer;
}

// Sagitta of a spherical surface of signed radius R at radius r (0 = flat).
double sag(double R, double r)
{
    return R == 0.0 ? 0.0 : (R > 0.0 ? 1.0 : -1.0) * (std::abs(R) - std::sqrt(std::max(0.0, R * R - r * r)));
}

}  // namespace

std::optional<SolidModel> solidModelOf(const Geometry& g, double minThickness)
{
    const std::string& kind = g.kindId();
    const auto p = [&g](const char* key) { return g.parameter(key); };
    SolidModel model;
    // Thickness of a shell or plate, at least what the grid can hold.
    const auto shellThickness = [&](const char* key) {
        const double designed = p(key);
        if (designed >= minThickness)
            return designed;
        model.note = thickened(designed, minThickness);
        return minThickness;
    };

    if (kind == "box") {
        const Vec3 half(0.5 * p("width"), 0.5 * p("depth"), 0.5 * p("height"));
        model.contains = [half](const Vec3& q) { return (q.array().abs() <= half.array()).all(); };
        model.boundingRadius = half.norm();
    } else if (kind == "sphere") {
        const double r = p("radius");
        model.contains = [r](const Vec3& q) { return q.squaredNorm() <= r * r; };
        model.boundingRadius = r;
    } else if (kind == "ellipsoid") {
        const Vec3 radii(p("radiusX"), p("radiusY"), p("radiusZ"));
        model.contains = [radii](const Vec3& q) { return (q.array() / radii.array()).matrix().squaredNorm() <= 1.0; };
        model.boundingRadius = radii.maxCoeff();
    } else if (kind == "cylinder") {
        const double r = p("radius"), h = 0.5 * p("height");
        model.contains = [r, h](const Vec3& q) { return std::abs(q.z()) <= h && q.x() * q.x() + q.y() * q.y() <= r * r; };
        model.boundingRadius = std::hypot(r, h);
    } else if (kind == "cone") {
        // Base at z = -h/2, apex at z = +h/2.
        const double r = p("radius"), height = p("height"), h = 0.5 * height;
        model.contains = [r, height, h](const Vec3& q) {
            if (std::abs(q.z()) > h)
                return false;
            const double limit = r * (h - q.z()) / height;
            return q.x() * q.x() + q.y() * q.y() <= limit * limit;
        };
        model.boundingRadius = std::hypot(r, h);
    } else if (kind == "plane") {
        // A surface without thickness: a plate centred on it.
        const double w = 0.5 * p("width"), d = 0.5 * p("depth"), t = 0.5 * minThickness;
        model.note = "it has no thickness and is represented as a plate " + std::to_string(minThickness * 1e3) + " mm thick";
        model.contains = [w, d, t](const Vec3& q) { return std::abs(q.x()) <= w && std::abs(q.y()) <= d && std::abs(q.z()) <= t; };
        model.boundingRadius = std::hypot(w, d) + t;
    } else if (kind == "disc") {
        const double r = p("radius"), t = 0.5 * minThickness;
        model.note = "it has no thickness and is represented as a plate " + std::to_string(minThickness * 1e3) + " mm thick";
        model.contains = [r, t](const Vec3& q) { return std::abs(q.z()) <= t && q.x() * q.x() + q.y() * q.y() <= r * r; };
        model.boundingRadius = r + t;
    } else if (kind == "reflector.parabolic") {
        const double f = p("focalLength"), a = 0.5 * p("aperture"), t = shellThickness("thickness");
        model.contains = [f, a, t](const Vec3& q) {
            const double r2 = q.x() * q.x() + q.y() * q.y();
            if (r2 > a * a)
                return false;
            const double surface = r2 / (4.0 * f);
            return q.z() <= surface && q.z() >= surface - t;
        };
        model.boundingRadius = std::hypot(a, a * a / (4.0 * f)) + t;
    } else if (kind == "reflector.spherical") {
        const double R = p("curvatureRadius"), a = 0.5 * p("aperture"), t = shellThickness("thickness");
        model.contains = [R, a, t](const Vec3& q) {
            const double r2 = q.x() * q.x() + q.y() * q.y();
            if (r2 > a * a)
                return false;
            const double surface = R - std::sqrt(std::max(0.0, R * R - r2));
            return q.z() <= surface && q.z() >= surface - t;
        };
        model.boundingRadius = std::hypot(a, R) + t;
    } else if (kind == "reflector.elliptical") {
        // The reflecting surface is the inside of the spheroid; the shell lies outside it.
        const double a = p("semiMajor"), b = p("semiMinor"), cut = p("cut") * a, t = shellThickness("thickness");
        model.contains = [a, b, cut, t](const Vec3& q) {
            if (q.z() > cut)
                return false;
            const double r2 = q.x() * q.x() + q.y() * q.y();
            const double inner = r2 / (b * b) + q.z() * q.z() / (a * a);
            const double outer = r2 / ((b + t) * (b + t)) + q.z() * q.z() / ((a + t) * (a + t));
            return inner >= 1.0 && outer <= 1.0;
        };
        model.boundingRadius = a + t;
    } else if (kind == "reflector.cylindrical") {
        const double f = p("focalLength"), w = 0.5 * p("width"), l = 0.5 * p("length"), t = shellThickness("thickness");
        model.contains = [f, w, l, t](const Vec3& q) {
            if (std::abs(q.x()) > w || std::abs(q.y()) > l)
                return false;
            const double surface = q.x() * q.x() / (4.0 * f);
            return q.z() <= surface && q.z() >= surface - t;
        };
        model.boundingRadius = std::hypot(std::hypot(w, l), w * w / (4.0 * f)) + t;
    } else if (kind == "reflector.flat") {
        const double w = 0.5 * p("width"), h = 0.5 * p("height"), t = shellThickness("thickness");
        model.contains = [w, h, t](const Vec3& q) { return std::abs(q.x()) <= w && std::abs(q.y()) <= h && q.z() <= 0.0 && q.z() >= -t; };
        model.boundingRadius = std::hypot(w, h) + t;
    } else if (kind == "lens.spherical") {
        const double a = 0.5 * p("aperture"), d = p("centerThickness"), r1 = p("radius1"), r2 = p("radius2");
        model.contains = [a, d, r1, r2](const Vec3& q) {
            const double r = std::hypot(q.x(), q.y());
            return r <= a && q.z() >= -0.5 * d + sag(r1, r) && q.z() <= 0.5 * d + sag(r2, r);
        };
        model.boundingRadius = std::hypot(a, 0.5 * d + std::abs(sag(r1, a)) + std::abs(sag(r2, a)));
    } else if (kind == "waveguide.circular") {
        const double ra = p("radius"), rb = p("endRadius"), length = p("length"), wall = shellThickness("wallThickness");
        model.contains = [ra, rb, length, wall](const Vec3& q) {
            if (std::abs(q.z()) > 0.5 * length)
                return false;
            const double inner = ra + (rb - ra) * (q.z() / length + 0.5);
            const double r = std::hypot(q.x(), q.y());
            return r >= inner && r <= inner + wall;
        };
        model.boundingRadius = std::hypot(std::max(ra, rb) + wall, 0.5 * length);
    } else if (kind == "waveguide.rectangular") {
        const double w = 0.5 * p("width"), h = 0.5 * p("height"), l = 0.5 * p("length"), wall = shellThickness("wallThickness");
        model.contains = [w, h, l, wall](const Vec3& q) {
            const double x = std::abs(q.x()), y = std::abs(q.y());
            return std::abs(q.z()) <= l && x <= w + wall && y <= h + wall && !(x < w && y < h);
        };
        model.boundingRadius = Vec3(w + wall, h + wall, l).norm();
    } else if (kind == "cavity.rectangular") {
        const Vec3 half(0.5 * p("sizeX"), 0.5 * p("sizeY"), 0.5 * p("sizeZ"));
        const double wall = shellThickness("wallThickness");
        model.contains = [half, wall](const Vec3& q) {
            const Eigen::Array3d a = q.array().abs();
            return (a <= half.array() + wall).all() && !(a < half.array()).all();
        };
        model.boundingRadius = (half.array() + wall).matrix().norm();
    } else if (kind == "cavity.cylindrical") {
        const double r = p("radius"), l = 0.5 * p("length"), wall = shellThickness("wallThickness");
        model.contains = [r, l, wall](const Vec3& q) {
            const double rr = std::hypot(q.x(), q.y()), z = std::abs(q.z());
            return rr <= r + wall && z <= l + wall && !(rr < r && z < l);
        };
        model.boundingRadius = std::hypot(r + wall, l + wall);
    } else if (kind == "cavity.spherical") {
        const double r = p("radius"), wall = shellThickness("wallThickness");
        model.contains = [r, wall](const Vec3& q) {
            const double d2 = q.squaredNorm();
            return d2 >= r * r && d2 <= (r + wall) * (r + wall);
        };
        model.boundingRadius = r + wall;
    } else if (kind == "diffraction.slit") {
        const double w = 0.5 * p("width"), h = 0.5 * p("height"), s = 0.5 * p("slitWidth"), t = shellThickness("thickness");
        model.contains = [w, h, s, t](const Vec3& q) {
            const double x = std::abs(q.x());
            return x >= s && x <= w && std::abs(q.y()) <= h && q.z() <= 0.0 && q.z() >= -t;
        };
        model.boundingRadius = std::hypot(w, h) + t;
    } else if (kind == "rod.horn" || kind == "rod.periodic") {
        // Circular rods along Z (geometry kinds of the mechanical module,
        // described here from their parameters).
        const bool periodic = kind == "rod.periodic";
        const double length = periodic ? p("cells") * (p("lengthA") + p("lengthB")) : p("length");
        const auto radiusAt = [&g, periodic, length](double z) {
            const double s = std::clamp(z / length + 0.5, 0.0, 1.0);
            if (periodic) {
                const double a = g.parameter("lengthA"), b = g.parameter("lengthB");
                return std::fmod(s * length, a + b) < a ? g.parameter("radiusA") : g.parameter("radiusB");
            }
            const double r1 = g.parameter("inputRadius"), r2 = g.parameter("outputRadius");
            switch (std::lround(g.parameter("profile"))) {
            case 0: return r1 + (r2 - r1) * s;
            case 1: return r1 * std::pow(r2 / r1, s);
            default: return s < 0.5 ? r1 : r2;
            }
        };
        model.contains = [radiusAt, length](const Vec3& q) {
            return std::abs(q.z()) <= 0.5 * length && std::hypot(q.x(), q.y()) <= radiusAt(q.z());
        };
        const double largest = periodic ? std::max(p("radiusA"), p("radiusB")) : std::max(p("inputRadius"), p("outputRadius"));
        model.boundingRadius = std::hypot(largest, 0.5 * length);
    } else {
        return std::nullopt;
    }
    return model;
}

}  // namespace resonant::fdtd
