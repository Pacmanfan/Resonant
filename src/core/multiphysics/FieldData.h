#pragma once

// Field data exchanged between participants (RES-CEME-002): values of one
// quantity on a support, with their time character and where they came from.
//
// The support is a rectilinear cell grid in world meters (edges per axis,
// cell-centred values), which covers 1D (one cell across), 2D and 3D.
// Participants with other discretizations (finite-element meshes) convert at
// their boundary and say how.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "multiphysics/Quantity.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace resonant::multiphysics {

struct GridSupport {
    std::array<std::vector<double>, 3> edges;  // m, ascending, count + 1 per axis
    std::string frame = "world";

    size_t count(int axis) const { return edges[static_cast<size_t>(axis)].empty() ? 0 : edges[static_cast<size_t>(axis)].size() - 1; }
    size_t cellCount() const { return count(0) * count(1) * count(2); }
    size_t index(size_t i, size_t j, size_t k) const { return i + count(0) * (j + count(1) * k); }
    double width(int axis, size_t i) const { return edges[static_cast<size_t>(axis)][i + 1] - edges[static_cast<size_t>(axis)][i]; }
    double cellVolume(size_t cell) const;
    Vec3 cellCentre(size_t cell) const;
    double volume() const;  // of the whole grid
    Status validate() const;
    bool sameAs(const GridSupport& other) const;

    static GridSupport uniform(const Vec3& origin, const Vec3& size, size_t nx, size_t ny, size_t nz);
    nlohmann::json toJson() const;
};

enum class TimeCharacter { Steady, Harmonic, Instant };
std::string_view toString(TimeCharacter time);

struct FieldData {
    QuantityDescriptor quantity;
    GridSupport support;
    std::vector<double> values;  // cell-major: values[cell * components + c]
    TimeCharacter time = TimeCharacter::Steady;
    double frequencyHz = 0.0;    // Harmonic: the frequency the quantity was averaged or computed at
    double timeSeconds = 0.0;    // Instant: the time
    std::string producer;        // participant id
    std::string solver;          // solver id and version
    std::uint64_t materialRevision = 0, geometryRevision = 0;

    int components() const { return quantity.components(); }
    double value(size_t cell, int component = 0) const { return values[cell * static_cast<size_t>(components()) + static_cast<size_t>(component)]; }
    // Integral of one component over the support (value x cell volume).
    double integral(int component = 0) const;
    double maxAbs() const;
    bool finite() const;
    Status validate() const;
    nlohmann::json summary() const;  // descriptor, support, integral and range; no values
};

}  // namespace resonant::multiphysics
