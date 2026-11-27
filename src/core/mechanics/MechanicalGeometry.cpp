#include "mechanics/MechanicalGeometry.h"

#include <cmath>

namespace resonant::mechanics {

namespace {

ParameterDef lengthParameter(std::string key, std::string label, double value)
{
    ParameterDef def;
    def.key = std::move(key);
    def.label = std::move(label);
    def.unit = "m";
    def.defaultValue = value;
    def.minValue = 0.0;
    def.maxValue = 1e6;
    return def;
}

double periodicLength(const Geometry& g)
{
    return g.parameter("cells") * (g.parameter("lengthA") + g.parameter("lengthB"));
}

}  // namespace

bool isRodGeometry(const Geometry& g)
{
    return g.kindId() == "rod.horn" || g.kindId() == "rod.periodic";
}

double rodRadiusAt(const Geometry& g, double s)
{
    s = std::clamp(s, 0.0, 1.0);
    if (g.kindId() == "rod.horn") {
        const double r1 = g.parameter("inputRadius"), r2 = g.parameter("outputRadius");
        switch (static_cast<HornProfile>(std::lround(g.parameter("profile")))) {
        case HornProfile::Conical: return r1 + (r2 - r1) * s;
        case HornProfile::Exponential: return r1 * std::pow(r2 / r1, s);
        case HornProfile::Stepped: return s < 0.5 ? r1 : r2;
        }
        return r1;
    }
    if (g.kindId() == "rod.periodic") {
        const double a = g.parameter("lengthA"), b = g.parameter("lengthB");
        const double x = s * periodicLength(g);
        const double within = std::fmod(x, a + b);
        return within < a ? g.parameter("radiusA") : g.parameter("radiusB");
    }
    return 0.0;
}

}  // namespace resonant::mechanics

namespace resonant {

void registerMechanicalGeometry(GeometryRegistry& registry)
{
    using namespace mechanics;
    // Display: the radius profile revolved about Z, with end caps.
    const auto tessellate = [](const Geometry& g, const TessellationOptions& options) {
        const bool periodic = g.kindId() == "rod.periodic";
        const double length = periodic ? periodicLength(g) : g.parameter("length");
        std::vector<ProfilePoint> profile;
        profile.push_back({0.0, -0.5 * length, 0.0, -1.0});
        const int samples = periodic ? 0 : 64;
        const auto addSide = [&](double s0, double s1) {
            const double r0 = rodRadiusAt(g, s0 + 1e-9), r1 = rodRadiusAt(g, s1 - 1e-9);
            const double z0 = -0.5 * length + s0 * length, z1 = -0.5 * length + s1 * length;
            const double slope = (r1 - r0) / std::max(1e-12, z1 - z0);
            const double n = std::sqrt(1.0 + slope * slope);
            TriangleMesh piece;
            revolveProfile(piece, {{r0, z0, 1.0 / n, -slope / n}, {r1, z1, 1.0 / n, -slope / n}}, options.segments);
            return piece;
        };
        TriangleMesh mesh;
        // Caps of every radius change.
        std::vector<double> breaks{0.0};
        if (periodic) {
            const double a = g.parameter("lengthA"), b = g.parameter("lengthB");
            for (int c = 0; c < static_cast<int>(g.parameter("cells")); ++c) {
                breaks.push_back((c * (a + b) + a) / length);
                breaks.push_back(((c + 1) * (a + b)) / length);
            }
        } else if (std::lround(g.parameter("profile")) == static_cast<long>(HornProfile::Stepped)) {
            breaks = {0.0, 0.5, 1.0};
        } else {
            for (int i = 1; i <= samples; ++i)
                breaks.push_back(static_cast<double>(i) / samples);
        }
        if (breaks.back() < 1.0)
            breaks.push_back(1.0);
        for (size_t i = 1; i < breaks.size(); ++i) {
            mesh.append(addSide(breaks[i - 1], breaks[i]));
            if (i + 1 < breaks.size()) {
                // Annulus between two radii at a step.
                const double z = -0.5 * length + breaks[i] * length;
                const double ra = rodRadiusAt(g, breaks[i] - 1e-9), rb = rodRadiusAt(g, breaks[i] + 1e-9);
                if (std::abs(ra - rb) > 1e-12)
                    revolveProfile(mesh, {{std::min(ra, rb), z, 0.0, ra > rb ? 1.0 : -1.0}, {std::max(ra, rb), z, 0.0, ra > rb ? 1.0 : -1.0}},
                                   options.segments);
            }
        }
        revolveProfile(mesh, {{0.0, -0.5 * length, 0.0, -1.0}, {rodRadiusAt(g, 0.0), -0.5 * length, 0.0, -1.0}}, options.segments);
        revolveProfile(mesh, {{0.0, 0.5 * length, 0.0, 1.0}, {rodRadiusAt(g, 1.0), 0.5 * length, 0.0, 1.0}}, options.segments);
        (void)profile;
        return mesh;
    };

    GeometryKind horn;
    horn.id = "rod.horn";
    horn.displayName = "Horn (velocity transformer)";
    horn.category = "mechanical";
    ParameterDef profile;
    profile.key = "profile";
    profile.label = "Profile (0 conical, 1 exponential, 2 stepped)";
    profile.defaultValue = 2.0;
    profile.minValue = 0.0;
    profile.maxValue = 2.0;
    profile.exclusiveMin = false;
    profile.integer = true;
    horn.parameters = {lengthParameter("inputRadius", "Input radius (-Z end)", 0.02), lengthParameter("outputRadius", "Output radius (+Z end)", 0.01),
                       lengthParameter("length", "Length (Z)", 0.25), profile};
    horn.tessellate = tessellate;
    registry.add(horn);

    GeometryKind periodic;
    periodic.id = "rod.periodic";
    periodic.displayName = "Periodic rod (phononic)";
    periodic.category = "mechanical";
    ParameterDef cells;
    cells.key = "cells";
    cells.label = "Cells";
    cells.defaultValue = 8.0;
    cells.minValue = 1.0;
    cells.maxValue = 200.0;
    cells.exclusiveMin = false;
    cells.integer = true;
    periodic.parameters = {lengthParameter("radiusA", "Radius of segment A", 0.01), lengthParameter("lengthA", "Length of segment A", 0.05),
                           lengthParameter("radiusB", "Radius of segment B", 0.004), lengthParameter("lengthB", "Length of segment B", 0.05),
                           cells};
    periodic.tessellate = tessellate;
    registry.add(periodic);
}

}  // namespace resonant
