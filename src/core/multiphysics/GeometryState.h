#pragma once

// Geometry state of a coupled run (RES-CEME-004, docs/architecture/PHYSICAL_SYSTEM_MODEL.md).
//
// The reference geometry (the regions' dimensions as designed, at the
// reference temperature) and the current geometry (after deformation) are
// kept apart. A participant that works in the reference configuration (small
// strain mechanics, conduction) reads `reference`; one that needs the
// deformed shape (wave propagation) reads `current`. Every change increments
// the revision; data computed at an older revision is stale.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace resonant::multiphysics {

struct GeometryChange {
    std::string region;
    std::string dimension;  // "thickness"
    double value = 0.0;     // m, the new current value (absolute, not an increment)
};

class GeometryState {
public:
    void setReference(const std::string& region, const std::string& dimension, double value);
    double reference(const std::string& region, const std::string& dimension) const;
    double current(const std::string& region, const std::string& dimension) const;

    // Applies changes with relaxation: current = old + w (new - old). Returns the largest
    // change relative to the reference value. Increments the revision when anything changed.
    Result<double> apply(const std::vector<GeometryChange>& changes, double relaxation = 1.0);
    std::uint64_t revision() const { return revision_; }
    bool stale(std::uint64_t computedAt) const { return computedAt != revision_; }
    nlohmann::json toJson() const;

private:
    std::map<std::string, std::map<std::string, double>> reference_, current_;
    std::uint64_t revision_ = 0;
};

}  // namespace resonant::multiphysics
