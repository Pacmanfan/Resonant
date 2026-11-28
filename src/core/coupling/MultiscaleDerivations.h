#pragma once

// The multiscale derivation pipeline (RES-MSC-006, -007, -009, -011, ADR-000224): methods
// that derive higher-scale properties from a representation, with their source and target
// scales, applicability checks, execution (convergence, sampling, uncertainty, whether the
// result is reliable) and the plan that assigns the derived properties to a library
// material with a derivation record. The physics is in the domains (thermal, electromagnetics
// for the constituents, atomistic) and multiscale (the homogenization solver).

#include "foundation/Result.h"
#include "materials/Material.h"
#include "multiscale/Representation.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {
class JobContext;
class Project;
}

namespace resonant::coupling {

inline constexpr const char* kDerivationFormat = "resonant.multiscale-derivation";

struct DerivationMethod {
    std::string id, title, modelCard, description;
    multiscale::Scale source = multiscale::Scale::Microstructure, target = multiscale::Scale::Continuum;
    std::vector<std::string> kinds;       // representation kinds it accepts
    std::vector<std::string> properties;  // "domain.key" it can derive
    nlohmann::json settings;              // the settings it takes, with defaults and meaning
    nlohmann::json toJson() const;
};

const std::vector<DerivationMethod>& derivationMethods();
const DerivationMethod* findDerivationMethod(const std::string& id);

// Why the method cannot derive from this representation (empty: it can). An unsupported
// scale transition, a kind it does not accept, missing constituent data, a force field
// that does not cover the species.
std::vector<std::string> applicability(const DerivationMethod& method, const multiscale::Representation& representation, const Project& project,
                                       const nlohmann::json& settings);

// Runs the method: the derivation document (format resonant.multiscale-derivation).
Result<nlohmann::json> derive(const multiscale::Representation& representation, const Project& project, const std::string& method,
                              const nlohmann::json& settings, JobContext* context);

// Assigning a derivation document's properties to a library material.
// options: {"materialId": an existing material to update, or "newMaterial": {"id", "name"},
//           "properties": ["domain.key", ...] (default all), "components": {"domain.key": "zz" | "mean" | ...}}
struct Assignment {
    Material material;
    bool created = false;
    multiscale::Derivation derivation;
    std::vector<multiscale::Relationship> relationships;
    std::vector<std::string> warnings;
};
Result<Assignment> planAssignment(const nlohmann::json& derivationDocument, const Project& project, const multiscale::MultiscaleModel& model,
                                  const nlohmann::json& options, const std::string& calculation);

}  // namespace resonant::coupling
