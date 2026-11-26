// Materials. A material has identity (id, name, description) and one
// property set per physics domain it applies to. Each domain has its own
// list of property keys (materials/Material.cpp), so acoustic, mechanical
// and electromagnetic properties are never forced into one structure, and a
// material simply has no set for a domain it does not describe.
//
// Every value records where it comes from (Provenance) and, optionally, the
// frequency and temperature range in which it is valid and a table of
// frequency-dependent values.
#pragma once

#include "foundation/Id.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {

enum class Provenance {
    Measured,              // measured by the user or a laboratory
    ManufacturerSupplied,  // from a data sheet
    LiteratureDerived,     // from a cited publication or handbook
    Theoretical,           // calculated from a model
    Illustrative,          // a nominal value for demonstration; not authoritative
};

std::string_view toString(Provenance provenance);
std::optional<Provenance> provenanceFromString(std::string_view text);

struct ValidityRange {
    std::optional<double> minFrequencyHz;
    std::optional<double> maxFrequencyHz;
    std::optional<double> minTemperatureK;
    std::optional<double> maxTemperatureK;

    friend bool operator==(const ValidityRange&, const ValidityRange&) = default;
};

struct FrequencySample {
    double frequencyHz = 0.0;
    double value = 0.0;
    friend bool operator==(const FrequencySample&, const FrequencySample&) = default;
};

struct PropertyValue {
    double value = 0.0;  // SI units of the property's definition
    Provenance provenance = Provenance::Illustrative;
    std::string source;  // citation, data sheet, measurement note, or limitation
    ValidityRange validity;
    // Optional frequency dependence, ascending in frequency. When present,
    // `value` is the nominal value and valueAt() interpolates the table.
    std::vector<FrequencySample> frequencyTable;

    // The value at a frequency: linear interpolation in the table, clamped
    // to its ends; `value` when there is no table.
    double valueAt(double frequencyHz) const;

    friend bool operator==(const PropertyValue&, const PropertyValue&) = default;
};

// Definition of one property of a domain.
struct PropertyDef {
    std::string key;    // "density"
    std::string label;  // "Density"
    std::string unit;   // "kg/m^3"
    double minValue = 0.0;
    bool exclusiveMin = false;
};

// The property keys a domain defines.
const std::vector<PropertyDef>& propertyDefinitions(PhysicsDomain domain);
const PropertyDef* findPropertyDefinition(PhysicsDomain domain, std::string_view key);

using PropertySet = std::map<std::string, PropertyValue, std::less<>>;

class Material {
public:
    Material() = default;
    Material(Id id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}

    const Id& id() const { return id_; }
    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }
    const std::string& description() const { return description_; }
    void setDescription(std::string description) { description_ = std::move(description); }

    // The domains this material has a property set for.
    std::vector<PhysicsDomain> domains() const;
    bool appliesTo(PhysicsDomain domain) const { return sets_.contains(domain); }

    const PropertySet* properties(PhysicsDomain domain) const;
    const PropertyValue* property(PhysicsDomain domain, std::string_view key) const;

    // Sets a property, creating the domain's set if needed. Fails for a key
    // the domain does not define or a value outside the definition's range.
    Status setProperty(PhysicsDomain domain, std::string_view key, PropertyValue value);
    void removeProperty(PhysicsDomain domain, std::string_view key);
    void addDomain(PhysicsDomain domain) { sets_[domain]; }
    void removeDomain(PhysicsDomain domain) { sets_.erase(domain); }

    Status validate() const;

    friend bool operator==(const Material&, const Material&) = default;

private:
    Id id_;
    std::string name_;
    std::string description_;
    std::map<PhysicsDomain, PropertySet> sets_;
};

class MaterialLibrary {
public:
    // Adds a material. Fails when the id is null or already present, or the
    // material is invalid.
    Status add(Material material);
    // Replaces the material with the same id.
    Status update(const Material& material);
    std::optional<Material> remove(const Id& id);

    const Material* find(const Id& id) const;
    // For editing a material in place; the caller keeps it valid.
    Material* findMutable(const Id& id);
    const Material* findByName(std::string_view name) const;
    const std::vector<Material>& materials() const { return materials_; }
    bool empty() const { return materials_.empty(); }
    void clear() { materials_.clear(); }

    friend bool operator==(const MaterialLibrary&, const MaterialLibrary&) = default;

private:
    std::vector<Material> materials_;
};

// The initial example materials (air, water, aluminum, steel, copper, glass,
// acrylic, concrete, rubber, wood). Every value is marked Illustrative: a
// nominal room-temperature figure of the kind found in general references,
// suitable for trying the application and NOT an authoritative constant.
// Real work must replace them with measured or cited values.
MaterialLibrary createExampleMaterialLibrary();

}  // namespace resonant
