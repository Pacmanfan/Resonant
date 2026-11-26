#include "geometry/Geometry.h"

#include <algorithm>
#include <cmath>

namespace resonant {

const ParameterDef* GeometryKind::findParameter(std::string_view key) const
{
    const auto it = std::find_if(parameters.begin(), parameters.end(),
                                 [key](const ParameterDef& def) { return def.key == key; });
    return it == parameters.end() ? nullptr : &*it;
}

GeometryRegistry& GeometryRegistry::instance()
{
    static GeometryRegistry registry = [] {
        GeometryRegistry created;
        registerPrimitiveGeometry(created);
        return created;
    }();
    return registry;
}

void GeometryRegistry::add(GeometryKind kind)
{
    kinds_.push_back(std::make_unique<GeometryKind>(std::move(kind)));
}

const GeometryKind* GeometryRegistry::find(std::string_view id) const
{
    for (const auto& kind : kinds_)
        if (kind->id == id)
            return kind.get();
    return nullptr;
}

std::vector<const GeometryKind*> GeometryRegistry::kinds() const
{
    std::vector<const GeometryKind*> result;
    result.reserve(kinds_.size());
    for (const auto& kind : kinds_)
        result.push_back(kind.get());
    return result;
}

std::vector<const GeometryKind*> GeometryRegistry::kindsInCategory(std::string_view category) const
{
    std::vector<const GeometryKind*> result;
    for (const auto& kind : kinds_)
        if (kind->category == category)
            result.push_back(kind.get());
    return result;
}

Status checkParameter(const ParameterDef& def, double value)
{
    if (!std::isfinite(value))
        return Status::failure(def.label + " must be a finite number");
    const bool belowMin = def.exclusiveMin ? value <= def.minValue : value < def.minValue;
    if (belowMin) {
        return Status::failure(def.label + " must be " + (def.exclusiveMin ? "greater than " : "at least ")
                               + std::to_string(def.minValue) + (def.unit.empty() ? "" : " " + def.unit));
    }
    if (value > def.maxValue)
        return Status::failure(def.label + " must be at most " + std::to_string(def.maxValue)
                               + (def.unit.empty() ? "" : " " + def.unit));
    if (def.integer && std::floor(value) != value)
        return Status::failure(def.label + " must be a whole number");
    return Status::success();
}

Result<Geometry> Geometry::create(std::string_view kindId)
{
    const GeometryKind* kind = GeometryRegistry::instance().find(kindId);
    if (!kind)
        return Result<Geometry>::failure("unknown geometry kind '" + std::string(kindId) + "'");
    Geometry geometry(kind);
    for (const auto& def : kind->parameters)
        geometry.parameters_[def.key] = def.defaultValue;
    return geometry;
}

double Geometry::parameter(std::string_view key) const
{
    const auto it = parameters_.find(key);
    return it == parameters_.end() ? 0.0 : it->second;
}

Status Geometry::setParameter(std::string_view key, double value)
{
    const ParameterDef* def = kind_->findParameter(key);
    if (!def)
        return Status::failure(kind_->displayName + " has no parameter '" + std::string(key) + "'");
    if (Status status = checkParameter(*def, value); !status)
        return status;
    const auto it = parameters_.find(key);
    const double previous = it->second;
    it->second = value;
    if (kind_->validate) {
        if (Status status = kind_->validate(*this); !status) {
            it->second = previous;
            return status;
        }
    }
    return Status::success();
}

Status Geometry::validate() const
{
    for (const auto& def : kind_->parameters) {
        const auto it = parameters_.find(def.key);
        if (it == parameters_.end())
            return Status::failure(kind_->displayName + " is missing parameter '" + def.key + "'");
        if (Status status = checkParameter(def, it->second); !status)
            return status;
    }
    if (kind_->validate)
        return kind_->validate(*this);
    return Status::success();
}

TriangleMesh Geometry::tessellate(const TessellationOptions& options) const
{
    return kind_->tessellate ? kind_->tessellate(*this, options) : TriangleMesh{};
}

void revolveProfile(TriangleMesh& mesh, const std::vector<ProfilePoint>& profile, int segments, double start,
                    double sweep)
{
    if (profile.size() < 2 || segments < 3)
        return;
    const int columns = segments + 1;
    const auto base = static_cast<std::uint32_t>(mesh.positions.size());
    for (const ProfilePoint& point : profile) {
        for (int j = 0; j < columns; ++j) {
            const double angle = start + sweep * static_cast<double>(j) / segments;
            const double c = std::cos(angle);
            const double s = std::sin(angle);
            mesh.addVertex(Vec3(point.r * c, point.r * s, point.z), Vec3(point.nr * c, point.nr * s, point.nz));
        }
    }
    for (size_t i = 0; i + 1 < profile.size(); ++i) {
        for (int j = 0; j < segments; ++j) {
            const auto a = base + static_cast<std::uint32_t>(i * columns + j);
            const auto b = a + 1;
            const auto d = base + static_cast<std::uint32_t>((i + 1) * columns + j);
            const auto c = d + 1;
            mesh.addTriangle(a, b, c);
            mesh.addTriangle(a, c, d);
        }
    }
}

}  // namespace resonant
