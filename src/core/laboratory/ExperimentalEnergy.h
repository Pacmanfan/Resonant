#pragma once

// Experimental energy accounting (REQ-CHAT-20261126-023, RES-LAB-013): measured energies
// and powers, each with its standard uncertainty and whether it was measured directly or
// derived (P = V I, Q = m c dT: a derivation is named, never called a direct energy
// measurement), become an accounting ledger marked measured. The accounting module judges
// such a balance against the expanded measurement uncertainty instead of a numerical
// tolerance; unmeasured pathways are declared missing, so the balance is then "insufficient
// data" rather than verified.

#include "accounting/AccountingLedger.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::laboratory {

struct MeasuredTerm {
    std::string id, role;          // input, output, dissipated, stored
    std::string from, to;          // entities (the system's members and the outside); stored: `to` is the member
    std::string mechanism;
    double value = 0.0, standard = 0.0;
    bool direct = false;           // measured directly (a calorimeter, a power meter) or derived
    std::string derivation;        // how it was obtained when not direct
    std::vector<std::string> datasets;
};

struct ExperimentalBalance {
    std::string title;
    std::string basis = "interval";  // interval (J) or rate (W)
    std::vector<std::string> members;
    std::vector<MeasuredTerm> terms;
    std::vector<std::pair<std::string, std::string>> unmeasured;  // what, why
    double coverage = 2.0;
    nlohmann::json toJson() const;
    static Result<ExperimentalBalance> fromJson(const nlohmann::json& j);
};

Result<accounting::Ledger> experimentalLedger(const ExperimentalBalance& balance);

}  // namespace resonant::laboratory
