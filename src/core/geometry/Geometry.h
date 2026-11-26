// Engineering geometry: a shape defined by a kind and named parameters in
// SI units ("box" with width, depth, height). This is the model the user
// edits and the project stores. Display meshes are derived from it by
// tessellate(); simulations derive their own representations.
//
// Kinds are described by GeometryKind entries in a GeometryRegistry, so a
// module can add shapes (reflectors, lenses, ...) without changing this
// class.
#pragma once

#include "foundation/Result.h"
#include "geometry/TriangleMesh.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {

struct ParameterDef {
    std::string key;         // stable identifier, stored in project files
    std::string label;       // shown in the user interface
    std::string unit;        // SI unit symbol: "m", "rad", or "" for a count / ratio
    double defaultValue = 1.0;
    double minValue = 0.0;   // inclusive unless exclusiveMin
    double maxValue = 1e9;   // inclusive
    bool exclusiveMin = true;
    bool integer = false;
};

struct TessellationOptions {
    int segments = 48;  // subdivisions around a full revolution
};

class Geometry;

struct GeometryKind {
    std::string id;           // "box", "sphere", ...
    std::string displayName;  // "Box"
    std::string category;     // "primitive", "reflector", ...
    std::vector<ParameterDef> parameters;
    // Display mesh of a geometry of this kind (parameters already valid).
    std::function<TriangleMesh(const Geometry&, const TessellationOptions&)> tessellate;
    // Optional rule involving several parameters; empty when there is none.
    std::function<Status(const Geometry&)> validate;

    const ParameterDef* findParameter(std::string_view key) const;
};

class GeometryRegistry {
public:
    // The registry holding the built-in kinds of every core module.
    static GeometryRegistry& instance();

    void add(GeometryKind kind);
    const GeometryKind* find(std::string_view id) const;
    std::vector<const GeometryKind*> kinds() const;                         // in registration order
    std::vector<const GeometryKind*> kindsInCategory(std::string_view category) const;

private:
    std::vector<std::unique_ptr<GeometryKind>> kinds_;
};

class Geometry {
public:
    // A geometry of the given kind with default parameters.
    static Result<Geometry> create(std::string_view kindId);

    const GeometryKind& kind() const { return *kind_; }
    const std::string& kindId() const { return kind_->id; }

    double parameter(std::string_view key) const;
    const std::map<std::string, double, std::less<>>& parameters() const { return parameters_; }

    // Sets one parameter. Fails (and changes nothing) for an unknown key, a
    // non-finite value, a value outside the parameter's range, a non-integer
    // value of an integer parameter, or a violated kind rule.
    Status setParameter(std::string_view key, double value);

    Status validate() const;

    TriangleMesh tessellate(const TessellationOptions& options = {}) const;

    friend bool operator==(const Geometry& a, const Geometry& b)
    {
        return a.kind_ == b.kind_ && a.parameters_ == b.parameters_;
    }

private:
    explicit Geometry(const GeometryKind* kind) : kind_(kind) {}

    const GeometryKind* kind_;
    std::map<std::string, double, std::less<>> parameters_;
};

// Checks one value against a parameter definition.
Status checkParameter(const ParameterDef& def, double value);

// --- tessellation helpers shared by the modules that define kinds ----------

// One vertex of a profile curve in the (r, z) half-plane with its outward
// normal (nr, nz) in the same plane.
struct ProfilePoint {
    double r = 0.0;
    double z = 0.0;
    double nr = 0.0;
    double nz = 1.0;
};

// Appends the surface obtained by revolving the profile polyline about the
// Z axis through `sweep` radians starting at angle `start`.
void revolveProfile(TriangleMesh& mesh, const std::vector<ProfilePoint>& profile, int segments,
                    double start = 0.0, double sweep = 2.0 * 3.14159265358979323846);

// Registers box, sphere, cylinder, cone, plane, disc and ellipsoid.
void registerPrimitiveGeometry(GeometryRegistry& registry);

}  // namespace resonant
