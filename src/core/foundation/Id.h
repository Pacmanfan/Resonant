// Stable unique identifiers for persisted objects (scene objects, materials,
// projects, simulations). An Id is an opaque string; generated ids are
// random 128-bit values in UUID version 4 text form.
#pragma once

#include <compare>
#include <functional>
#include <string>

namespace resonant {

class Id {
public:
    Id() = default;
    explicit Id(std::string value) : value_(std::move(value)) {}

    // A new random id. Thread-safe.
    static Id generate();

    bool isNull() const { return value_.empty(); }
    const std::string& str() const { return value_; }

    friend bool operator==(const Id& a, const Id& b) { return a.value_ == b.value_; }
    friend auto operator<=>(const Id& a, const Id& b) { return a.value_ <=> b.value_; }

private:
    std::string value_;
};

}  // namespace resonant

template <>
struct std::hash<resonant::Id> {
    size_t operator()(const resonant::Id& id) const noexcept { return std::hash<std::string>()(id.str()); }
};
