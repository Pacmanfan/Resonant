#pragma once

// Resolving a region's property (RES-FAM-008, -012 of the request): the value
// every analysis uses, with where it came from. In order:
//   1. a property field of the region (at the position s through its layer);
//   2. a value stated on the region (a measured batch, with its provenance);
//   3. the effective value of the region's composition by its chosen method
//      (with fraction fields evaluated at s), with the bounds and validity;
//   4. the region's library material.
// Otherwise the property is missing - never substituted. A composition without
// a method resolves through the region's material when it has one; without
// one it is missing, and the message gives the bounds.

#include "matter/MatterArchitecture.h"
#include "matter/Mixing.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <optional>
#include <string>

namespace resonant {
class MaterialLibrary;
}

namespace resonant::matter {

struct ResolvedProperty {
    std::string key;  // "domain.key"
    double value = std::numeric_limits<double>::quiet_NaN();
    std::string unit;
    std::string origin;  // field, region, composition, material, missing
    std::string provenance;
    std::string source;
    std::optional<double> uncertainty;
    std::optional<EffectiveEstimate> effective;
    std::string message;  // why it is missing, or what refines it
    bool ok() const { return origin != "missing"; }
    nlohmann::json toJson() const;
};

// The unit of a "domain.key" property ("" when the key is not a library property).
std::string unitOf(const std::string& key);

ResolvedProperty resolve(const Architecture& architecture, const FunctionalRegion& region, const std::string& key, double s, const MaterialLibrary& library);

// Every library property of a constituent ("domain.key"), resolved for a composition.
ResolvedProperty constituentProperty(const Constituent& c, const std::string& key, const MaterialLibrary& library);

}  // namespace resonant::matter
