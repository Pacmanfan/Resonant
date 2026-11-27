#pragma once

// Measurement uncertainty, in the manner of the GUM (JCGM 100): a budget of
// standard-uncertainty components, combined by root-sum-square under the
// stated assumption that they are uncorrelated, and expanded with a
// coverage factor (k = 2 by default, about 95 % for a normal distribution).
//
// The kind of each component is kept: an instrument specification, a
// calibration, a statistical (Type A) estimate, a model uncertainty and a
// numerical-simulation uncertainty are different things, and a comparison
// with a simulation reports them separately.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace resonant::experiments {

enum class UncertaintySource {
    SensorCalibration, InstrumentAccuracy, Quantization, Noise, Position, Orientation, Clock, Temperature,
    Repeatability, Material, Model, NumericalSimulation, Other
};
enum class UncertaintyType { InstrumentSpecification, Calibration, Statistical, Model, NumericalSimulation };

std::string_view toString(UncertaintySource source);
std::string_view toString(UncertaintyType type);
Result<UncertaintySource> uncertaintySourceFromString(std::string_view text);
Result<UncertaintyType> uncertaintyTypeFromString(std::string_view text);

struct UncertaintyComponent {
    UncertaintySource source = UncertaintySource::Other;
    UncertaintyType type = UncertaintyType::InstrumentSpecification;
    double relative = 0.0;  // standard uncertainty as a fraction of the value
    double absolute = 0.0;  // standard uncertainty in the quantity's unit
    std::string distribution = "normal";  // how the standard uncertainty was obtained: normal, rectangular, ...
    double degreesOfFreedom = 0.0;        // 0 = not stated (taken as large)
    std::string description;
};

struct UncertaintyBudget {
    std::vector<UncertaintyComponent> components;
    double coverageFactor = 2.0;
    std::string correlation = "components assumed uncorrelated (root-sum-square)";

    bool empty() const { return components.empty(); }
    // sqrt(sum (u_rel value)^2 + sum u_abs^2)
    double combinedStandard(double value) const;
    double expanded(double value) const { return coverageFactor * combinedStandard(value); }
    // Combined relative part only.
    double combinedRelative() const;
    // Combined standard uncertainty of the components of one type.
    double combinedStandard(double value, UncertaintyType type) const;
    void add(UncertaintyComponent component) { components.push_back(std::move(component)); }
    std::string describe(double value, std::string_view unit) const;
};

nlohmann::json toJson(const UncertaintyBudget& budget);
Result<UncertaintyBudget> uncertaintyBudgetFromJson(const nlohmann::json& value);

// Common components.
UncertaintyComponent quantizationComponent(double step);                // rectangular, step / sqrt(12)
UncertaintyComponent statisticalComponent(double standardError, double degreesOfFreedom, std::string description);

// For a product or quotient of independent quantities the relative
// components add in quadrature: the components of both budgets.
UncertaintyBudget combineForProduct(const UncertaintyBudget& a, const UncertaintyBudget& b);

}  // namespace resonant::experiments
