#pragma once

// Typed quantities of the accounting framework (REQ-CHAT-20261126-022, RES-ACC-003,
// ADR-000226). A family is a physical quantity, not just a dimension: torque and energy
// share kg m^2 s^-2, entropy and heat capacity J/K, pressure and momentum flux Pa, yet
// none of them combine. Families of conserved quantities (energy, linear and angular
// momentum, mass, charge) are what ledgers balance; entropy is balanced with its
// production; force, torque and power are rates or interactions, not conserved.
// A quantity keeps its meaning and context: kind (stored.kinetic, transfer, dissipated,
// force.applied ...), domain, value or vector, region, frame, time or interval, source,
// object, material, provenance, uncertainty and validity.

#include "foundation/Dimensions.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::accounting {

enum class Family {
    Energy, Power, Force, Torque, LinearMomentum, AngularMomentum, Mass, Charge, Entropy, EntropyRate,
    HeatCapacity, Temperature, Pressure, Stress, MomentumFlux, EnergyDensity, PowerDensity, EnergyFlux, ForceDensity,
    Velocity, Acceleration, AngularVelocity, MomentOfInertia, Frequency, Length, Time
};

struct FamilyInfo {
    Family family;
    std::string id;     // "energy"
    std::string name;   // "Energy"
    std::string unit;   // the SI unit ("J")
    Dimension dimension;
    bool conserved = false;
    bool vector = false;   // naturally a vector (force, momentum, torque, angular momentum, velocity ...)
    std::string note;
};

const std::vector<FamilyInfo>& families();
const FamilyInfo& info(Family f);
std::string_view toString(Family f);
std::optional<Family> familyFromString(std::string_view id);
// The family of a quantity exchanged per unit time in a rate (steady) ledger: Energy -> Power;
// a momentum's rate keeps its family (it is a momentum flow, kg m/s^2, not a force).
std::string rateUnit(Family f);

struct Quantity {
    std::string id;
    Family family = Family::Energy;
    std::string kind;         // semantic: "stored.kinetic", "transfer", "dissipated.viscous", "force.applied" ...
    std::string domain;       // "mechanical", "electromagnetic", "thermal", "acoustic", "general"
    double value = 0.0;       // SI; for a vector its magnitude is not implied: see `components`
    bool isVector = false;
    std::array<double, 3> components{};  // SI, in `frame`
    std::string frame = "world";
    std::string region, object, material, source;
    std::optional<double> time;                         // an instant
    std::optional<std::array<double, 2>> interval;      // or an interval [t0, t1]
    std::string provenance = "computed";
    std::optional<double> uncertainty;                  // standard uncertainty, SI
    std::string validity;

    std::string unit() const { return info(family).unit; }
    double magnitude() const;
    nlohmann::json toJson() const;
    static Result<Quantity> fromJson(const nlohmann::json& j);
};

// A scalar quantity from a value in any unit of the family's dimension.
Result<Quantity> make(Family f, double value, std::string_view unit, std::string kind = {});
// The value in a display unit (same dimension).
Result<double> valueIn(const Quantity& q, std::string_view unit);
// The sum of two quantities of the same family (and, for vectors, the same frame); its kind is
// theirs if they agree, else the family's id. Fails across families even with equal dimensions.
Result<Quantity> add(const Quantity& a, const Quantity& b);
// a - b, with the same rules.
Result<Quantity> subtract(const Quantity& a, const Quantity& b);

}  // namespace resonant::accounting
