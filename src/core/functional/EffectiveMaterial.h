#pragma once

// The effective material of an organized structure (RES-META-002, RES-META-010,
// RES-META-011): what an effective-medium analysis derived, how, where it is
// valid and from what. A record, not a material: the services turn it into a
// library material with provenance "homogenized" (services/FunctionalServices.cpp).

#include "foundation/Id.h"
#include "foundation/Result.h"
#include "materials/Material.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace resonant::functional {

struct EffectiveProperty {
    std::string domain;     // acoustic, mechanical, electromagnetic, thermal (the library's domains)
    std::string key;        // the library's property key, e.g. thermalConductivity
    std::string component;  // isotropic, parallel (along the layers), perpendicular (across them)
    double value = 0.0;     // SI; the nominal value (at the lowest frequency of a table)
    std::vector<std::pair<double, double>> frequencyTable;  // (Hz, value), ascending; empty: not dispersive
    std::string note;
};

struct EffectiveMaterial {
    std::string name;
    // homogenized (a mixing or averaging rule), retrieved (from computed scattering),
    // equivalent-model (a reduced model's parameter, e.g. a circuit or lumped lattice)
    std::string kind = "homogenized";
    std::string method;          // the model id
    std::vector<EffectiveProperty> properties;
    double validFromHz = 0.0, validToHz = 0.0;  // both 0: static (steady)
    double scaleRatio = 0.0;     // cell / wavelength (or cell / length) at the top of the validity range
    std::string validity;        // the condition in words
    nlohmann::json derivation = nlohmann::json::object();  // structure, cell, constituents, comparison with the explicit model
    std::vector<std::string> limitations;

    Status validate() const;
    nlohmann::json toJson() const;
    static Result<EffectiveMaterial> fromJson(const nlohmann::json& document);
};

// The library material of an effective material: provenance "homogenized", the
// validity range, frequency tables for dispersive values and the derivation in each
// value's source. `components` chooses, per "domain.key", which principal component an
// isotropic library value carries (default: the first the analysis lists); `reference`
// names the result the material comes from.
Result<Material> toLibraryMaterial(const EffectiveMaterial& effective, const Id& id, const std::string& name,
                                   const nlohmann::json& components, const std::string& reference);

}  // namespace resonant::functional
