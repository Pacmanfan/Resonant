#pragma once

// Solver capability declarations (RES-FAM-010): what an analysis that consumes
// architectures can represent - physics, material models, interface models,
// patterns, dimensionality, scales, couplings, nonlinearities, limitations,
// inputs and outputs - and the check of a stack against it. Anything the
// declaration does not cover is refused with the reason; a field that a
// solver's input could hold is not thereby supported. The declarations of the
// actual analyses are in coupling/MatterAnalyses (they describe the domains).

#include "matter/MatterArchitecture.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {
class MaterialLibrary;
}

namespace resonant::matter {

struct SolverCapability {
    std::string id, title;
    std::string solver;                       // the solver (or closed form) it runs
    std::vector<std::string> physics;         // mechanical, thermal, electrical, electromagnetic, electromechanical
    std::vector<std::string> materialModels;  // isotropic, orthotropic-in-plane, principal-conductivity, graded, composition-effective, piezoelectric
    std::vector<std::string> interfaceModels; // the interface model kinds it represents
    std::vector<std::string> patterns;        // the layer patterns it represents (uniform always)
    std::string dimensionality;
    std::vector<std::string> scales;
    std::vector<std::string> couplings;
    std::vector<std::string> nonlinearities;
    std::vector<std::string> limitations;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<std::string> requiredProperties;  // "domain.key" every layer must have
    bool supports(const std::string& materialModel) const;
    bool acceptsInterface(const std::string& kind) const;
    nlohmann::json toJson() const;
};

struct Compatibility {
    bool compatible = true;
    std::vector<Issue> issues;  // severity "refused" or "warning"
    nlohmann::json toJson() const;
    std::string summary() const;  // the refusals, one per line
};

// Checks every layer (its properties, anisotropy, fields, composition, pattern) and
// every interface between consecutive layers against the declaration.
Compatibility checkStack(const Architecture& architecture, const Stack& stack, const SolverCapability& capability, const MaterialLibrary& library);

}  // namespace resonant::matter
