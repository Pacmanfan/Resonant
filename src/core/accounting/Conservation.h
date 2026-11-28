#pragma once

// Conservation monitoring (REQ-CHAT-20261126-022, RES-ACC-010..013, -015, ADR-000226): a
// registry of conservation laws and consistency relations, and the evaluation of a ledger
// for a system: one balance check per conserved family,
//
//     residual = delta stored - (input + external impulse - output + generated - dissipated)
//
// A measured ledger (laboratory balances) is judged against the expanded uncertainty of its
// residual (the terms' standard uncertainties, uncorrelated) instead of the tolerance:
// consistent within k u, a warning within 2 k u, exceeded beyond.
//
// with a relative residual only when its scale is meaningful, a status (verified, warning,
// exceeded, not evaluated, insufficient data, not applicable), and an attribution of the
// residual to the numerical errors the sources state - never to dissipation. A check with
// declared missing contributions is "insufficient data", whatever its residual; a system
// counting an effective description and the representation it replaces is not evaluated.
// The report adds the breakdown, the flow graph (actual amounts), the forces with their
// resultants, and the histories over time.

#include "accounting/AccountingLedger.h"
#include "accounting/ControlVolume.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::accounting {

struct CheckModel {
    std::string id, title;
    std::string kind;  // conservation, consistency
    Family family = Family::Energy;
    std::string domain;
    std::vector<std::string> sources, sinks, fluxes;
    std::string equation, requiredData, method, assumptions;
    nlohmann::json toJson() const;
};
const std::vector<CheckModel>& checkModels();
const CheckModel* findCheckModel(std::string_view id);

struct Tolerances {
    double relative = 1e-6;   // verified at or below
    double warning = 1e-3;    // a warning at or below, exceeded above
    double absolute = 1e-15;  // SI: below this scale no relative residual is meaningful
    double coverage = 2.0;    // measured ledgers: the coverage factor of the expanded measurement uncertainty
    nlohmann::json toJson() const;
    static Tolerances fromJson(const nlohmann::json& j);
};

Check balance(const Ledger& ledger, const AccountingSystem& system, Family family, const Tolerances& tolerances);

struct Report {
    AccountingSystem system;
    Basis basis = Basis::Interval;
    std::vector<Check> checks;  // balances, then the ledger's consistency relations
    nlohmann::json breakdown = nlohmann::json::array();
    nlohmann::json flows = nlohmann::json::object();
    nlohmann::json forces = nlohmann::json::object();
    nlohmann::json series = nlohmann::json::array();
    nlohmann::json classified = nlohmann::json::array();  // every exchange with its role for this system
    nlohmann::json toJson() const;
};

Report evaluate(const Ledger& ledger, const AccountingSystem& system, const Tolerances& tolerances);

// The worst status of a set (exceeded > insufficient data > warning > not evaluated > verified > not applicable).
CheckStatus worst(const std::vector<Check>& checks);

}  // namespace resonant::accounting
