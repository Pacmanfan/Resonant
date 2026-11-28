#pragma once

// Material state of a coupled run (RES-CEME-003, docs/materials/MATERIAL_STATE_ARCHITECTURE.md).
//
// Intrinsic definitions stay in the material library. A physical system may
// attach laws to a region's property ("electromagnetic.lossTangent"): a
// constant, linear in temperature, or a table over temperature, each with
// its validity range and provenance. The state is a temperature field (on a
// participant's grid, in world coordinates); a property at a point is the
// law evaluated at the temperature interpolated there. Evaluating outside a
// law's validity returns the law's value at the nearest valid temperature
// and records the extrapolation as a warning - it is never silent. Every
// state change increments the revision participants compare against.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "multiphysics/FieldData.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace resonant::multiphysics {

enum class LawKind { Constant, LinearInTemperature, Table };
std::string_view toString(LawKind kind);

struct PropertyLaw {
    std::string region;    // region id in the physical system
    std::string property;  // "<domain>.<key>", e.g. "electromagnetic.lossTangent"
    LawKind kind = LawKind::Constant;
    double value = 0.0;                  // Constant; LinearInTemperature: the value at `reference`
    double reference = 293.15;           // K
    double slope = 0.0;                  // per K: v(T) = value + slope (T - reference)
    std::vector<std::pair<double, double>> table;  // (K, value), ascending in K
    double validMin = 0.0, validMax = 1e9;         // K
    std::string provenance = "illustrative";
    std::string source;

    Status validate() const;
    nlohmann::json toJson() const;
    static Result<PropertyLaw> fromJson(const nlohmann::json& j);
};

struct LawValue {
    double value = 0.0;
    bool extrapolated = false;  // the temperature was outside the validity; the value is clamped there
};
LawValue evaluate(const PropertyLaw& law, double temperatureK);

class MaterialState {
public:
    void setReferenceTemperature(double k) { reference_ = k; }
    double referenceTemperature() const { return reference_; }
    void addLaw(PropertyLaw law) { laws_.push_back(std::move(law)); }
    const std::vector<PropertyLaw>& laws() const { return laws_; }
    const PropertyLaw* law(const std::string& region, const std::string& property) const;

    // The temperature field (empty until a thermal participant provides one: the
    // reference temperature everywhere).
    bool hasTemperature() const { return temperature_.has_value(); }
    const FieldData* temperature() const { return temperature_ ? &*temperature_ : nullptr; }
    double temperatureAt(const Vec3& point) const;
    // Sets the field with relaxation: T = T_old + w (T_new - T_old) (w = 1: replace; a new
    // support replaces outright). Returns the largest change in K. Increments the revision.
    Result<double> updateTemperature(const FieldData& field, double relaxation = 1.0);

    // A property at a point: the region's law at the local temperature, or `intrinsic`.
    double property(const std::string& region, const std::string& property, const Vec3& point, double intrinsic);
    std::uint64_t revision() const { return revision_; }
    const std::set<std::string>& warnings() const { return warnings_; }

    // History of the temperature field, one snapshot per update (the latest last), at most `limit`.
    const std::vector<FieldData>& history() const { return history_; }
    void setHistoryLimit(size_t n) { historyLimit_ = n; }

private:
    double reference_ = 293.15;
    std::vector<PropertyLaw> laws_;
    std::optional<FieldData> temperature_;
    std::uint64_t revision_ = 0;
    std::set<std::string> warnings_;
    std::vector<FieldData> history_;
    size_t historyLimit_ = 20;
};

}  // namespace resonant::multiphysics
