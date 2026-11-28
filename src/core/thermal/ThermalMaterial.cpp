#include "thermal/ThermalMaterial.h"

namespace resonant::thermal {

Result<ThermalMaterial> thermalMaterial(const Material& material)
{
    const PropertySet* set = material.properties(PhysicsDomain::Thermal);
    if (!set)
        return Result<ThermalMaterial>::failure("the material '" + material.name() + "' has no thermal properties");
    ThermalMaterial out;
    out.name = material.name();
    std::string missing;
    const auto read = [&](const char* key, double& target) {
        const PropertyValue* v = material.property(PhysicsDomain::Thermal, key);
        if (!v) {
            missing += missing.empty() ? key : std::string(", ") + key;
            return false;
        }
        target = v->value;
        out.provenance += std::string(key) + ": " + std::string(toString(v->provenance)) + (v->source.empty() ? "" : " (" + v->source + ")") + "; ";
        return true;
    };
    read("density", out.density);
    read("thermalConductivity", out.conductivity);
    read("specificHeat", out.specificHeat);
    if (!missing.empty())
        return Result<ThermalMaterial>::failure("the material '" + material.name() + "' lacks the thermal properties " + missing);
    if (const PropertyValue* a = material.property(PhysicsDomain::Thermal, "thermalExpansion")) {
        out.expansion = a->value;
        out.expansionStated = true;
    }
    return out;
}

}  // namespace resonant::thermal
