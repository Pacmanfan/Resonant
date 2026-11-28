#pragma once

// Spatial property fields (RES-FAM-007): a property of a region that varies
// along one axis of a frame - constant, linear, power-law, exponential or
// tabulated - with its unit and bounds, marked idealized (a mathematical
// distribution) or realizable (measured or produced by a named process).
// Positions are normalized to the region's extent along the axis (0 at the
// start, 1 at the end), so a field follows its layer when the layer changes
// thickness.

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace resonant::matter {

struct PropertyField {
    std::string id;
    std::string region;
    // "domain.key" (thermal.thermalConductivity), or "fraction:<constituent>" for a
    // graded composition.
    std::string property;
    std::string axis = "stack";  // stack (through the thickness), x, y, z of the object frame
    std::string kind = "linear"; // constant, linear, power-law, exponential, table
    double from = 0.0, to = 0.0; // the values at s = 0 and s = 1
    double exponent = 1.0;       // power-law: from + (to - from) s^exponent
    std::vector<std::pair<double, double>> table;  // (s, value), s increasing in [0, 1]
    std::string unit;
    bool realizable = false;     // false: an idealized distribution
    std::string process;         // realizable: the process that produces it
    std::string provenance = "unstated";
    std::string note;

    // The value at s in [0, 1].
    double at(double s) const;
    double minimum() const;
    double maximum() const;
    // Integrals over s in [0, 1]: the mean (parallel combination) and the harmonic mean
    // (series combination, 1 / mean of 1/v), by adaptive Simpson to 1e-12 relative.
    double mean() const;
    double harmonicMean() const;
    // n sublayers of equal thickness, each with the value at its midpoint.
    std::vector<double> discretize(int n) const;
    // The relative change in the series (harmonic) combination between n and 2n midpoint
    // sublayers: an estimate of the discretization error with n.
    double discretizationError(int n) const;

    Status validate() const;
    nlohmann::json toJson() const;
    static Result<PropertyField> fromJson(const nlohmann::json& j);
};

const std::vector<std::string>& fieldKinds();

}  // namespace resonant::matter
