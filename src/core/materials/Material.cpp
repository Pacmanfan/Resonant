#include "materials/Material.h"

#include <algorithm>
#include <cmath>

namespace resonant {

std::string_view toString(Provenance provenance)
{
    switch (provenance) {
    case Provenance::Measured: return "measured";
    case Provenance::ManufacturerSupplied: return "manufacturer";
    case Provenance::LiteratureDerived: return "literature";
    case Provenance::Theoretical: return "theoretical";
    case Provenance::Illustrative: return "illustrative";
    case Provenance::Homogenized: return "homogenized";
    }
    return "illustrative";
}

std::optional<Provenance> provenanceFromString(std::string_view text)
{
    if (text == "measured") return Provenance::Measured;
    if (text == "manufacturer") return Provenance::ManufacturerSupplied;
    if (text == "literature") return Provenance::LiteratureDerived;
    if (text == "theoretical") return Provenance::Theoretical;
    if (text == "illustrative") return Provenance::Illustrative;
    if (text == "homogenized") return Provenance::Homogenized;
    return std::nullopt;
}

double PropertyValue::valueAt(double frequencyHz) const
{
    if (frequencyTable.empty())
        return value;
    if (frequencyHz <= frequencyTable.front().frequencyHz)
        return frequencyTable.front().value;
    if (frequencyHz >= frequencyTable.back().frequencyHz)
        return frequencyTable.back().value;
    const auto upper = std::lower_bound(
        frequencyTable.begin(), frequencyTable.end(), frequencyHz,
        [](const FrequencySample& sample, double f) { return sample.frequencyHz < f; });
    const auto lower = upper - 1;
    const double t = (frequencyHz - lower->frequencyHz) / (upper->frequencyHz - lower->frequencyHz);
    return lower->value + t * (upper->value - lower->value);
}

const std::vector<PropertyDef>& propertyDefinitions(PhysicsDomain domain)
{
    static const std::vector<PropertyDef> acoustic = {
        {"density", "Density", "kg/m^3", 0.0, true},
        {"soundSpeed", "Speed of sound", "m/s", 0.0, true},
        {"attenuation", "Attenuation coefficient", "Np/m", 0.0, false},
        {"absorption", "Absorption coefficient", "", 0.0, false},
        {"referenceTemperature", "Reference temperature", "K", 0.0, true},
    };
    static const std::vector<PropertyDef> mechanical = {
        {"density", "Density", "kg/m^3", 0.0, true},
        {"youngsModulus", "Young's modulus", "Pa", 0.0, true},
        {"poissonsRatio", "Poisson's ratio", "", -1.0, true},
        {"lossFactor", "Damping loss factor", "", 0.0, false},
    };
    static const std::vector<PropertyDef> electromagnetic = {
        {"relativePermittivity", "Relative permittivity", "", 0.0, true},
        {"relativePermeability", "Relative permeability", "", 0.0, true},
        {"conductivity", "Electrical conductivity", "S/m", 0.0, false},
        {"lossTangent", "Dielectric loss tangent", "", 0.0, false},
    };
    // Heat conduction and thermal expansion (REQ-CHAT-20261126-016, RES-CEME-010).
    static const std::vector<PropertyDef> thermal = {
        {"density", "Density", "kg/m^3", 0.0, true},
        {"thermalConductivity", "Thermal conductivity", "W/(m K)", 0.0, true},
        {"specificHeat", "Specific heat capacity", "J/(kg K)", 0.0, true},
        {"thermalExpansion", "Linear thermal expansion coefficient", "1/K", -1.0, false},
    };
    switch (domain) {
    case PhysicsDomain::Acoustic: return acoustic;
    case PhysicsDomain::Mechanical: return mechanical;
    case PhysicsDomain::Electromagnetic: return electromagnetic;
    case PhysicsDomain::Thermal: return thermal;
    }
    return acoustic;
}

const PropertyDef* findPropertyDefinition(PhysicsDomain domain, std::string_view key)
{
    const auto& defs = propertyDefinitions(domain);
    const auto it = std::find_if(defs.begin(), defs.end(), [key](const PropertyDef& def) { return def.key == key; });
    return it == defs.end() ? nullptr : &*it;
}

namespace {

Status checkPropertyValue(PhysicsDomain domain, const PropertyDef& def, const PropertyValue& value)
{
    const auto check = [&](double v) -> Status {
        if (!std::isfinite(v))
            return Status::failure(def.label + " must be a finite number");
        if (def.exclusiveMin ? v <= def.minValue : v < def.minValue)
            return Status::failure(def.label + " must be " + (def.exclusiveMin ? "greater than " : "at least ")
                                   + std::to_string(def.minValue));
        if (domain == PhysicsDomain::Mechanical && def.key == "poissonsRatio" && v > 0.5)
            return Status::failure("Poisson's ratio must be at most 0.5");
        return Status::success();
    };
    if (Status status = check(value.value); !status)
        return status;
    double previousFrequency = -1.0;
    for (const FrequencySample& sample : value.frequencyTable) {
        if (!(sample.frequencyHz > previousFrequency) || sample.frequencyHz < 0.0)
            return Status::failure(def.label + ": frequency table must be ascending and non-negative");
        previousFrequency = sample.frequencyHz;
        if (Status status = check(sample.value); !status)
            return status;
    }
    const ValidityRange& range = value.validity;
    if (range.minFrequencyHz && range.maxFrequencyHz && *range.minFrequencyHz > *range.maxFrequencyHz)
        return Status::failure(def.label + ": validity frequency range is inverted");
    if (range.minTemperatureK && range.maxTemperatureK && *range.minTemperatureK > *range.maxTemperatureK)
        return Status::failure(def.label + ": validity temperature range is inverted");
    return Status::success();
}

}  // namespace

std::vector<PhysicsDomain> Material::domains() const
{
    std::vector<PhysicsDomain> result;
    for (const auto& [domain, set] : sets_)
        result.push_back(domain);
    return result;
}

const PropertySet* Material::properties(PhysicsDomain domain) const
{
    const auto it = sets_.find(domain);
    return it == sets_.end() ? nullptr : &it->second;
}

const PropertyValue* Material::property(PhysicsDomain domain, std::string_view key) const
{
    const PropertySet* set = properties(domain);
    if (!set)
        return nullptr;
    const auto it = set->find(key);
    return it == set->end() ? nullptr : &it->second;
}

Status Material::setProperty(PhysicsDomain domain, std::string_view key, PropertyValue value)
{
    const PropertyDef* def = findPropertyDefinition(domain, key);
    if (!def)
        return Status::failure("the " + std::string(toString(domain)) + " domain has no property '" + std::string(key)
                               + "'");
    if (Status status = checkPropertyValue(domain, *def, value); !status)
        return status;
    sets_[domain][std::string(key)] = std::move(value);
    return Status::success();
}

void Material::removeProperty(PhysicsDomain domain, std::string_view key)
{
    const auto it = sets_.find(domain);
    if (it == sets_.end())
        return;
    const auto property = it->second.find(key);
    if (property != it->second.end())
        it->second.erase(property);
}

Status Material::validate() const
{
    if (id_.isNull())
        return Status::failure("material has no id");
    if (name_.empty())
        return Status::failure("material has no name");
    for (const auto& [domain, set] : sets_) {
        for (const auto& [key, value] : set) {
            const PropertyDef* def = findPropertyDefinition(domain, key);
            if (!def)
                return Status::failure("material '" + name_ + "': unknown " + std::string(toString(domain))
                                       + " property '" + key + "'");
            if (Status status = checkPropertyValue(domain, *def, value); !status)
                return Status::failure("material '" + name_ + "': " + status.message());
        }
    }
    return Status::success();
}

Status MaterialLibrary::add(Material material)
{
    if (Status status = material.validate(); !status)
        return status;
    if (find(material.id()))
        return Status::failure("a material with id " + material.id().str() + " already exists");
    materials_.push_back(std::move(material));
    return Status::success();
}

Status MaterialLibrary::update(const Material& material)
{
    if (Status status = material.validate(); !status)
        return status;
    const auto it = std::find_if(materials_.begin(), materials_.end(),
                                 [&](const Material& m) { return m.id() == material.id(); });
    if (it == materials_.end())
        return Status::failure("no material with id " + material.id().str());
    *it = material;
    return Status::success();
}

std::optional<Material> MaterialLibrary::remove(const Id& id)
{
    const auto it = std::find_if(materials_.begin(), materials_.end(), [&](const Material& m) { return m.id() == id; });
    if (it == materials_.end())
        return std::nullopt;
    Material removed = std::move(*it);
    materials_.erase(it);
    return removed;
}

const Material* MaterialLibrary::find(const Id& id) const
{
    const auto it = std::find_if(materials_.begin(), materials_.end(), [&](const Material& m) { return m.id() == id; });
    return it == materials_.end() ? nullptr : &*it;
}

Material* MaterialLibrary::findMutable(const Id& id)
{
    const auto it = std::find_if(materials_.begin(), materials_.end(), [&](const Material& m) { return m.id() == id; });
    return it == materials_.end() ? nullptr : &*it;
}

const Material* MaterialLibrary::findByName(std::string_view name) const
{
    const auto it = std::find_if(materials_.begin(), materials_.end(),
                                 [&](const Material& m) { return m.name() == name; });
    return it == materials_.end() ? nullptr : &*it;
}

}  // namespace resonant
