#pragma once

// Named parameters of functional structures (RES-FUNC-003): values read from
// the scene objects they are bound to (the optimization framework's scene
// bindings), stored in the structure (free) or computed (derived); changes
// applied through the geometry kinds' own rules.

#include "foundation/Result.h"
#include "functional/FunctionalStructure.h"

#include <map>
#include <string>
#include <vector>

namespace resonant::functional {

using Values = std::map<std::string, double, std::less<>>;

struct ParameterValue {
    std::string id, name, unit, binding, group, description;
    ParameterKind kind = ParameterKind::Free;
    double value = 0.0;
    double minimum = 0.0, maximum = 0.0;  // the effective range (structure and geometry kind)
    bool editable = true;                 // false for derived parameters
    std::string objectName;               // bound: the object's name
};

// In the structure's order; derived parameters evaluated after the others.
Result<std::vector<ParameterValue>> resolveParameters(const FunctionalStructure& structure, const Project& project);
Values valuesOf(const std::vector<ParameterValue>& parameters);

// Applies values of bound parameters to the scene (geometry parameters of one
// object together, so joint rules see the final values) and of free
// parameters to the structure. Refuses derived parameters, unknown ids,
// values outside the range, and geometry the kind rejects; on failure the
// project and structure may be partly changed (apply to a copy).
Status applyParameters(FunctionalStructure& structure, Project& project, const Values& values);

}  // namespace resonant::functional
