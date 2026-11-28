#pragma once

// The multiscale material model (RES-MSC-001..003, ADR-000224): representations of
// materials at their scales, the relationships between them and the derivation
// records that give library property values their lineage. Stored in the project
// (extension "multiscale"); no physics here - the calculations are in the domains
// and coupling/MultiscaleDerivations.

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {
class Material;
class Project;
}

namespace resonant::multiscale {

inline constexpr const char* kExtension = "multiscale";
inline constexpr const char* kFormat = "resonant.multiscale";

// From the smallest to the largest.
enum class Scale { Electronic, Atomistic, Molecular, Microstructure, Effective, Continuum };
std::string_view toString(Scale scale);
std::optional<Scale> scaleFromString(std::string_view text);
int rank(Scale scale);  // 0 electronic ... 5 continuum

struct State {
    std::optional<double> temperatureK;
    std::optional<double> pressurePa;
    friend bool operator==(const State&, const State&) = default;
};

struct Representation {
    std::string id, name, description;
    Scale scale = Scale::Continuum;
    // crystal, molecule, rve.voxel, electronic-structure, effective-material, continuum-material
    std::string kind;
    std::string materialId;                 // the library material it describes, if any
    nlohmann::json data = nlohmann::json::object();  // the kind's structural data
    std::vector<std::string> constituents;  // library material ids of its phases or species
    State state;
    std::vector<std::string> assumptions;
    nlohmann::json boundaryConditions = nlohmann::json::object();
    std::string provenance = "user";        // synthetic (procedural), imported, computed, user
    std::string source;
    std::string validation = "unvalidated"; // unvalidated, verified (numerically checked), validated (against a reference)
    std::string validationNote;
    std::string uncertainty;                // what is known about it, in words
    std::string modelCard;                  // the card of its kind

    // The content key of what it physically is (kind, scale, data, constituents, state,
    // boundary conditions): changes when the structure changes, not when it is renamed.
    std::string revision() const;
    nlohmann::json toJson() const;
    static Result<Representation> fromJson(const nlohmann::json& document);
};

// "representation:<id>", "material:<id>" or "derivation:<id>".
struct Relationship {
    std::string from, to;
    std::string kind;  // derived-from, constituent-of, homogenizes, computed-from, describes
    friend bool operator==(const Relationship&, const Relationship&) = default;
};

struct DerivedProperty {
    std::string domain, key, component = "isotropic", unit;
    double value = 0.0;
    std::optional<double> uncertainty;
};

struct Derivation {
    std::string id;
    std::string method, modelCard, solver, solverVersion;
    std::string representation, representationRevision;
    Scale sourceScale = Scale::Microstructure, targetScale = Scale::Continuum;
    std::string material;  // the library material the properties were assigned to
    std::vector<DerivedProperty> properties;
    nlohmann::json inputs = nlohmann::json::object();
    nlohmann::json boundaryConditions = nlohmann::json::object();
    nlohmann::json convergence = nlohmann::json::object();
    nlohmann::json resolution = nlohmann::json::object();
    nlohmann::json sampling = nlohmann::json::object();
    State conditions;
    std::map<std::string, std::string> constituentKeys;  // material id -> its content key when derived
    std::string created, calculation, parent;           // UTC; the result it came from; a previous derivation

    nlohmann::json toJson() const;
    static Result<Derivation> fromJson(const nlohmann::json& document);
};

struct MultiscaleModel {
    std::vector<Representation> representations;
    std::vector<Relationship> relationships;
    std::vector<Derivation> derivations;

    const Representation* find(std::string_view id) const;
    const Derivation* derivation(std::string_view id) const;
    nlohmann::json toJson() const;
    static Result<MultiscaleModel> fromJson(const nlohmann::json& document);
};

// Problems: duplicate or empty ids, unknown scales or kinds for a scale, relationships to
// nothing, derivations naming a missing representation or material.
std::vector<std::string> validateModel(const MultiscaleModel& model, const Project& project);

Result<MultiscaleModel> modelOf(const Project& project);  // empty when the project has none
void storeModel(Project& project, const MultiscaleModel& model);

// The content key of a library material (its properties and values).
std::string materialContentKey(const Material& material);

// A library property whose derivation no longer describes it.
struct StaleProperty {
    std::string material, domain, key, derivation;
    std::vector<std::string> reasons;  // representation changed / missing, constituent changed, value overridden
};
std::vector<StaleProperty> staleProperties(const MultiscaleModel& model, const Project& project);

// The lineage of every property of a material: provenance, source, uncertainty, the
// derivation and its representation, the chain below it, and staleness.
nlohmann::json lineageOf(const MultiscaleModel& model, const Project& project, const std::string& materialId);

}  // namespace resonant::multiscale
