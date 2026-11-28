#pragma once

// Exploration and transformation pathways of architectures (REQ-CHAT-20261127-001,
// RES-FAM-012, -017, ADR-000228).
//
//   mechanisms       the physical mechanisms between domains, each with its status:
//                    implemented by an architecture analysis, by the coupled engine
//                    (its catalogue's status), or not implemented - and the properties
//                    it needs
//   mechanismsFor    Direction A: for a desired input and output domain, the direct
//                    mechanisms and two-stage cascades, with the regions and library
//                    materials that have what they need
//   explore          Direction B: what an architecture can do - the applicable analyses,
//                    the implemented pathways, missing properties, and risks computed with
//                    their numbers; findings apart from hypotheses; changes nothing
//   evaluatePathway  a function's stages, each evaluated by its model with input, output,
//                    stored and dissipated power and efficiency, its balance checked by the
//                    accounting framework; a chain efficiency only when every stage is

#include "foundation/Result.h"
#include "matter/MatterArchitecture.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {
class Project;
class MaterialLibrary;
}  // namespace resonant

namespace resonant::coupling {

struct Mechanism {
    std::string id, title;
    std::string from, to;  // domains
    std::string status;    // implemented, engine-validated, engine-experimental, engine-external, not-implemented
    std::string model;     // the analysis or engine coupling model
    std::vector<std::string> needs;
    std::vector<std::string> materialClasses;
    std::string note;
    bool implemented() const { return status == "implemented" || status == "engine-validated" || status == "engine-experimental"; }
    nlohmann::json toJson() const;
};
const std::vector<Mechanism>& mechanisms();
const Mechanism* findMechanism(const std::string& id);

nlohmann::json mechanismsFor(const std::string& from, const std::string& to, const matter::Architecture* architecture, const Project& project);

nlohmann::json explore(const matter::Architecture& architecture, const Project& project);

Result<nlohmann::json> evaluatePathway(const matter::Architecture& architecture, const std::string& functionId, const nlohmann::json& options,
                                       const MaterialLibrary& library);

}  // namespace resonant::coupling
