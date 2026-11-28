#pragma once

// Accounting systems and control volumes (REQ-CHAT-20261126-022, RES-ACC-004, ADR-000226).
// A system is a set of member entities; an exchange whose ends are both members is
// internal, one entering a member from outside is an input, one leaving a member to a sink
// is dissipated and to anything else an output. The record is the same for every system:
// two coupled resonators exchange energy internally when both are members and across the
// boundary when only one is.

#include "accounting/AccountingLedger.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::accounting {

enum class SystemKind { Closed, Open, ControlVolume, Object, Assembly, Subsystem };
std::string_view toString(SystemKind k);

struct AccountingSystem {
    std::string id, name;
    SystemKind kind = SystemKind::Assembly;
    std::vector<std::string> members;  // entity ids
    std::string frame = "world";
    std::string description;

    bool contains(const std::string& entity) const;
    nlohmann::json toJson() const;
    static Result<AccountingSystem> fromJson(const nlohmann::json& j);
};

// Every entity of the ledger that is not a source, sink or the environment.
AccountingSystem wholeSystem(const Ledger& ledger);
// Members that are not in the ledger, or are sources, sinks or the environment.
Status validateFor(const AccountingSystem& system, const Ledger& ledger);

enum class Role { Internal, Input, Output, Dissipated, Outside };
std::string_view toString(Role r);
Role classify(const Exchange& exchange, const Ledger& ledger, const AccountingSystem& system);

}  // namespace resonant::accounting
