#include "manipulators/Manipulator.h"

#include "manipulators/Analytics.h"
#include "manipulators/ReflectorSurface.h"
#include "project/ProjectSerializer.h"

#include <nlohmann/json.hpp>

namespace resonant::manipulators {

using nlohmann::json;

std::string_view toString(Category category)
{
    switch (category) {
    case Category::Reflector: return "reflector";
    case Category::Lens: return "lens";
    case Category::Waveguide: return "waveguide";
    case Category::ResonantStructure: return "resonantStructure";
    case Category::DiffractionStructure: return "diffractionStructure";
    }
    return "reflector";
}

std::string_view toString(BoundaryCondition boundary)
{
    switch (boundary) {
    case BoundaryCondition::Rigid: return "rigid";
    case BoundaryCondition::PressureRelease: return "pressureRelease";
    case BoundaryCondition::Impedance: return "impedance";
    case BoundaryCondition::Absorbing: return "absorbing";
    }
    return "rigid";
}

std::string_view toString(SupportLevel level)
{
    switch (level) {
    case SupportLevel::GeometryOnly: return "geometryOnly";
    case SupportLevel::AnalyticalEstimate: return "analyticalEstimate";
    case SupportLevel::ApproximateField: return "approximateField";
    }
    return "geometryOnly";
}

json ManipulatorComponent::toJson() const
{
    json value = {{"category", std::string(toString(settings.category))},
                  {"domain", std::string(resonant::toString(settings.domain))},
                  {"boundary", std::string(toString(settings.boundary))},
                  {"notes", settings.notes}};
    if (settings.intendedFocus)
        value["intendedFocus"] = resonant::toJson(*settings.intendedFocus);
    if (settings.intendedSource)
        value["intendedSource"] = resonant::toJson(*settings.intendedSource);
    return value;
}

bool ManipulatorComponent::equals(const Component& other) const
{
    const auto* typed = dynamic_cast<const ManipulatorComponent*>(&other);
    return typed && typed->settings == settings;
}

std::optional<Category> categoryOfGeometry(const Geometry& geometry)
{
    const std::string& category = geometry.kind().category;
    if (category == "reflector") return Category::Reflector;
    if (category == "lens") return Category::Lens;
    if (category == "waveguide") return Category::Waveguide;
    if (category == "cavity") return Category::ResonantStructure;
    if (category == "diffraction") return Category::DiffractionStructure;
    return std::nullopt;
}

SupportLevel supportOf(const Geometry& geometry)
{
    const std::string& category = geometry.kind().category;
    if (category == "reflector")
        return SupportLevel::ApproximateField;
    if (category == "lens" || category == "waveguide" || category == "cavity")
        return SupportLevel::AnalyticalEstimate;
    return SupportLevel::GeometryOnly;
}

std::string supportDescription(const Geometry& geometry)
{
    switch (supportOf(geometry)) {
    case SupportLevel::ApproximateField:
        return "Field effect calculated by the reflector model (Kirchhoff approximation: single reflection, "
               "diffraction from the finite aperture included, no multiple reflections or shadowing between "
               "surfaces). Ray paths are available as a geometrical check. Not a full-wave solution.";
    case SupportLevel::AnalyticalEstimate:
        return "Geometry plus closed-form estimates for the idealized shape. NO field effect is calculated: this "
               "object does not change any simulated field.";
    case SupportLevel::GeometryOnly:
        return "Geometry only. No physical model exists for this structure yet: it does not change any simulated "
               "field.";
    }
    return {};
}

Result<std::unique_ptr<SceneObject>> makeManipulator(std::string name, std::string_view geometryKind)
{
    auto geometry = Geometry::create(geometryKind);
    if (!geometry)
        return Result<std::unique_ptr<SceneObject>>::failure(geometry.message());
    const auto category = categoryOfGeometry(geometry.value());
    if (!category)
        return Result<std::unique_ptr<SceneObject>>::failure("'" + std::string(geometryKind)
                                                             + "' is not a wave-manipulator geometry");
    auto object = std::make_unique<SceneObject>(Id::generate(), kManipulatorType);
    object->state.name = std::move(name);
    ManipulatorSettings settings;
    settings.category = *category;
    object->state.component = std::make_shared<ManipulatorComponent>(settings);
    object->state.geometry = geometry.take();
    return object;
}

void registerManipulatorComponents(ComponentRegistry& registry)
{
    registry.add(kManipulatorType, [](const json& v) -> Result<ComponentPtr> {
        try {
            ManipulatorSettings s;
            const std::string category = v.at("category").get<std::string>();
            bool found = false;
            for (const Category c : {Category::Reflector, Category::Lens, Category::Waveguide,
                                     Category::ResonantStructure, Category::DiffractionStructure})
                if (toString(c) == category) {
                    s.category = c;
                    found = true;
                }
            if (!found)
                return Result<ComponentPtr>::failure("unknown manipulator category '" + category + "'");
            const std::string boundary = v.at("boundary").get<std::string>();
            found = false;
            for (const BoundaryCondition b : {BoundaryCondition::Rigid, BoundaryCondition::PressureRelease,
                                              BoundaryCondition::Impedance, BoundaryCondition::Absorbing})
                if (toString(b) == boundary) {
                    s.boundary = b;
                    found = true;
                }
            if (!found)
                return Result<ComponentPtr>::failure("unknown boundary condition '" + boundary + "'");
            const auto domain = physicsDomainFromString(v.value("domain", std::string("acoustic")));
            if (!domain)
                return Result<ComponentPtr>::failure("unknown physics domain in a manipulator");
            s.domain = *domain;
            s.notes = v.value("notes", std::string());
            for (const auto& [key, target] : {std::pair{"intendedFocus", &s.intendedFocus}, {"intendedSource", &s.intendedSource}}) {
                if (v.contains(key)) {
                    auto point = vec3FromJson(v[key], key);
                    if (!point)
                        return Result<ComponentPtr>::failure(point.message());
                    *target = point.take();
                }
            }
            return ComponentPtr(std::make_shared<ManipulatorComponent>(s));
        } catch (const json::exception& error) {
            return Result<ComponentPtr>::failure(std::string("invalid wave manipulator: ") + error.what());
        }
    });
}

std::vector<ConstraintViolation> checkConstraints(const Geometry& geometry, const ManufacturingConstraints& constraints)
{
    std::vector<ConstraintViolation> violations;
    const auto has = [&geometry](const char* key) { return geometry.kind().findParameter(key) != nullptr; };

    if (constraints.minimumWallThickness) {
        for (const char* key : {"thickness", "wallThickness"}) {
            if (has(key) && geometry.parameter(key) < *constraints.minimumWallThickness)
                violations.push_back({"minimum wall thickness", geometry.parameter(key), *constraints.minimumWallThickness,
                                      "increase the " + geometry.kind().findParameter(key)->label + " to at least the limit"});
        }
    }
    if (constraints.maximumDimension) {
        const Aabb bounds = geometry.tessellate().bounds();
        const double largest = bounds.isValid() ? bounds.size().maxCoeff() : 0.0;
        if (largest > *constraints.maximumDimension)
            violations.push_back({"maximum dimension", largest, *constraints.maximumDimension,
                                  "reduce the aperture, length or size so the largest extent fits"});
    }
    if (constraints.minimumCurvatureRadius) {
        double smallest = std::numeric_limits<double>::infinity();
        if (geometry.kindId() == "reflector.parabolic" || geometry.kindId() == "reflector.cylindrical")
            smallest = 2.0 * geometry.parameter("focalLength");  // radius of curvature at the vertex
        else if (geometry.kindId() == "reflector.spherical")
            smallest = geometry.parameter("curvatureRadius");
        else if (geometry.kindId() == "reflector.elliptical")
            smallest = geometry.parameter("semiMinor") * geometry.parameter("semiMinor") / geometry.parameter("semiMajor");
        else if (geometry.kindId() == "lens.spherical")
            for (const char* key : {"radius1", "radius2"})
                if (geometry.parameter(key) != 0.0)
                    smallest = std::min(smallest, std::abs(geometry.parameter(key)));
        if (smallest < *constraints.minimumCurvatureRadius)
            violations.push_back({"minimum curvature radius", smallest, *constraints.minimumCurvatureRadius,
                                  "use a longer focal length or larger radii of curvature"});
    }
    if (constraints.minimumFeatureSize && has("slitWidth") && geometry.parameter("slitWidth") < *constraints.minimumFeatureSize)
        violations.push_back({"minimum feature size", geometry.parameter("slitWidth"), *constraints.minimumFeatureSize,
                              "widen the slit to at least the limit"});
    return violations;
}

}  // namespace resonant::manipulators
