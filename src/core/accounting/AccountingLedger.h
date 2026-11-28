#pragma once

// The accounting ledger (REQ-CHAT-20261126-022, RES-ACC-004..008, -011, -012, -015,
// ADR-000226): what a physical calculation reports, in terms that any system boundary can
// classify afterwards.
//
//   entities   bodies, regions, fields, participants and elements that can hold quantities;
//              sources, sinks (dissipation to heat) and the environment, which never are members
//   holdings   a quantity held by an entity: stored energy by form (recoverable or not),
//              momentum, entropy - at the interval's ends and optionally as a history
//   exchanges  a quantity moving from one entity to another: work, heat, radiation, a
//              transfer between domains, dissipation, an impulse (the time integral of a
//              force), a momentum flux; `amount` is what the destination receives,
//              `sourceAmount` what the source gave up when they differ (a transfer's mapping)
//   generations  explicit sources inside an entity (entropy production, a reaction)
//   forces     force and torque records with their origin, and action-reaction pairs
//   missing    contributions a model cannot provide (the check is then incomplete)
//   numerical  the numerical errors the sources state (integration, mapping, solver balance)
//   contracts  scale-transfer contracts of entities using effective representations
//   checks     consistency relations evaluated by the adapters (Q factors, two force methods)
//
// The basis is an interval (amounts in J, kg m/s over [t0, t1]; holdings are states) or a
// rate (a steady state: W, kg m/s^2; holdings are storage rates, usually zero). Nothing in
// a ledger says "input" or "internal": that depends on the system (ControlVolume.h).

#include "accounting/Quantities.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace resonant::accounting {

using Vec3 = std::array<double, 3>;

enum class Basis { Interval, Rate };
std::string_view toString(Basis b);

enum class EntityKind { Body, Region, Field, Participant, Element, Source, Sink, Environment, Representation };
std::string_view toString(EntityKind k);
std::optional<EntityKind> entityKindFromString(std::string_view s);

struct Entity {
    std::string id, name;
    EntityKind kind = EntityKind::Body;
    std::string domain, object, material;
    std::string representation;  // the multiscale representation this entity is (if any)
    std::string effectiveOf;     // the representation an effective description of this entity replaces
    std::string note;
    // Sources, sinks and the environment are outside every system.
    bool external() const { return kind == EntityKind::Source || kind == EntityKind::Sink || kind == EntityKind::Environment; }
};

struct Exchange {
    std::string id, from, to;
    Family family = Family::Energy;
    std::string category;  // work, heat, radiation, transfer, dissipation, impulse, flux
    std::string mechanism, fromDomain, toDomain, region, method, provenance = "computed";
    bool isVector = false;
    double amount = 0.0;                // received by `to`
    Vec3 vector{};
    std::optional<double> sourceAmount;  // given up by `from`, when different (scalar)
    std::optional<double> uncertainty;
    bool includesDestination = false;    // the source's calculation already includes the destination's share
    std::vector<double> history;         // cumulative amount at the ledger's times (scalar)
    std::vector<Vec3> vectorHistory;     // cumulative (vector)
    std::string note;
    Vec3 value() const { return isVector ? vector : Vec3{amount, 0.0, 0.0}; }
    Vec3 sourceValue() const { return isVector ? vector : Vec3{sourceAmount.value_or(amount), 0.0, 0.0}; }
};

struct Holding {
    std::string id, entity;
    Family family = Family::Energy;
    std::string form;          // kinetic, elastic, potential, thermal, latent, hardening, field.electric, magnetization, momentum, entropy
    bool recoverable = true;   // elastic energy is; the energy locked in hardening or latent heat is not mechanically
    bool isVector = false;
    double initial = 0.0, final = 0.0;
    Vec3 initialVector{}, finalVector{};
    std::vector<double> history;  // at the ledger's times
    std::vector<Vec3> vectorHistory;
    std::string method, provenance = "computed", note;
    std::optional<double> uncertainty;  // standard uncertainty of the change (measured ledgers)
    Vec3 change() const
    {
        return isVector ? Vec3{finalVector[0] - initialVector[0], finalVector[1] - initialVector[1], finalVector[2] - initialVector[2]}
                        : Vec3{final - initial, 0.0, 0.0};
    }
};

struct Generation {
    std::string id, entity;
    Family family = Family::Entropy;
    std::string mechanism, method;
    double amount = 0.0;
    std::vector<double> history;
};

struct ForceRecord {
    std::string id;
    Family family = Family::Force;  // Force or Torque
    std::string category;  // applied, contact, constraint, elastic, viscous, pressure, gravity, electromagnetic, magnetic,
                           // acoustic-radiation, radiation-pressure, internal
    std::string model, source, target, mechanism, method;
    Vec3 vector{};
    Vec3 application{};
    std::string frame = "world";
    std::string time = "final";  // final, time-averaged, peak, steady
    std::string distribution;    // "uniform over 1 m^2", "the absorbed profile" ...
    std::string pair;            // the reaction's id
    std::optional<double> uncertainty;
    std::vector<Vec3> history;   // at the ledger's times
};

struct Missing {
    Family family = Family::Energy;
    std::string entity;  // empty: the whole ledger
    std::string what, why;
};

struct NumericalEstimate {
    Family family = Family::Energy;
    std::string source, method;
    double amount = 0.0;  // the size of the error (non-negative)
};

struct ScaleContract {
    std::string id, sourceRepresentation, targetEntity, quantity, method, averaging, resolution, conservation;
    std::optional<double> uncertainty;
    bool sourceRepresented = false;  // whether the source scale's energy is still modeled explicitly
};

enum class CheckStatus { Verified, Warning, Exceeded, NotEvaluated, InsufficientData, NotApplicable };
std::string_view toString(CheckStatus s);
std::optional<CheckStatus> checkStatusFromString(std::string_view s);

struct Check {
    std::string model, title, system;
    Family family = Family::Energy;
    std::string scope = "global";  // global, local, relation
    CheckStatus status = CheckStatus::NotEvaluated;
    bool isVector = false;
    Vec3 initial{}, final{}, input{}, output{}, impulse{}, internal{}, generated{}, dissipated{}, delta{}, residual{};
    std::optional<double> relative;  // |residual| / scale, only when the scale is meaningful
    double scale = 0.0, tolerance = 0.0, warning = 0.0, numericalError = 0.0;
    double measurementUncertainty = 0.0;  // combined standard uncertainty of the residual (measured ledgers)
    std::string unit, attribution;
    std::vector<std::string> missing, notes;
    nlohmann::json details = nlohmann::json::object();
    std::vector<double> times, residualHistory;
    double residualNorm() const;
    nlohmann::json toJson() const;
    static Result<Check> fromJson(const nlohmann::json& j);
};

struct Ledger {
    std::string id, title, sourceKind, solver, solverVersion;
    Basis basis = Basis::Interval;
    double t0 = 0.0, t1 = 0.0;
    bool measured = false;  // the amounts are measurements with uncertainties: balances are judged against them
    std::string per;  // amounts per unit of this ("m^3": a material point's volume; "m^2": a layer's area); empty: totals
    std::vector<double> times;
    std::vector<Entity> entities;
    std::vector<Exchange> exchanges;
    std::vector<Holding> holdings;
    std::vector<Generation> generations;
    std::vector<ForceRecord> forces;
    std::vector<Missing> missing;
    std::vector<NumericalEstimate> numerical;
    std::vector<ScaleContract> contracts;
    std::vector<Quantity> quantities;  // informational (a steady state's stored energy, field enhancement ...)
    std::vector<Check> checks;         // consistency relations from the adapters
    std::vector<std::string> notes;

    // The unit of a family's amounts in this ledger (J or W, kg m/s or kg m/s^2 ...).
    std::string unit(Family f) const;
    const Entity* entity(const std::string& entityId) const;
    Status validate() const;
    nlohmann::json toJson() const;
    static Result<Ledger> fromJson(const nlohmann::json& j);
};

}  // namespace resonant::accounting
