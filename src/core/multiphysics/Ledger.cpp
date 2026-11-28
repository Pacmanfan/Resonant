#include "multiphysics/Ledger.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace resonant::multiphysics {

std::string_view toString(TermKind kind)
{
    switch (kind) {
    case TermKind::Input: return "input";
    case TermKind::BoundaryOut: return "boundary-out";
    case TermKind::Dissipated: return "dissipated";
    case TermKind::Stored: return "stored";
    case TermKind::TransferOut: return "transfer-out";
    case TermKind::TransferIn: return "transfer-in";
    case TermKind::Informational: return "informational";
    }
    return "input";
}

std::string_view toString(BalanceMode mode)
{
    return mode == BalanceMode::SteadyPower ? "steady power (W)" : "energy over the interval (J)";
}

void EnergyLedger::clear()
{
    terms_.clear();
    transfers_.clear();
    solverResiduals_.clear();
}

Audit EnergyLedger::audit(double relativeTolerance, double couplingError) const
{
    Audit a;
    a.mode = mode_;
    a.tolerance = relativeTolerance;
    std::map<std::string, ParticipantBalance> byParticipant;
    std::vector<std::string> order;
    const auto balance = [&](const std::string& participant, const std::string& domain) -> ParticipantBalance& {
        auto [it, inserted] = byParticipant.try_emplace(participant);
        if (inserted) {
            it->second.participant = participant;
            it->second.domain = domain;
            order.push_back(participant);
        }
        return it->second;
    };
    for (const LedgerTerm& t : terms_) {
        ParticipantBalance& b = balance(t.participant, t.domain);
        switch (t.kind) {
        case TermKind::Input: b.input += t.value; a.externalInput += t.value; break;
        case TermKind::BoundaryOut: b.boundaryOut += t.value; a.boundaryOut += t.value; break;
        case TermKind::Dissipated: b.dissipated += t.value; a.dissipated += t.value; break;
        case TermKind::Stored: b.stored += t.value; a.stored += t.value; break;
        case TermKind::TransferOut: b.transferOut += t.value; break;
        case TermKind::TransferIn: b.transferIn += t.value; break;
        case TermKind::Informational: break;
        }
    }
    for (const TransferRecord& r : transfers_)
        if (r.energy)
            a.transferError += r.targetIntegral - r.sourceIntegral;
    double scale = 0.0;
    for (const std::string& p : order) {
        ParticipantBalance b = byParticipant[p];
        b.residual = b.input + b.transferIn - b.boundaryOut - b.dissipated - b.stored - b.transferOut;
        b.scale = std::max({std::abs(b.input), std::abs(b.transferIn), std::abs(b.boundaryOut), std::abs(b.dissipated), std::abs(b.stored),
                            std::abs(b.transferOut)});
        if (const auto it = solverResiduals_.find(p); it != solverResiduals_.end())
            b.solverResidual = it->second;
        scale = std::max(scale, b.scale);
        a.participants.push_back(b);
    }
    // System: external input = boundary out + dissipated + stored, transfers counted once (their
    // target-minus-source differences remain).
    a.systemResidual = a.externalInput + a.transferError - a.boundaryOut - a.dissipated - a.stored;
    a.relativeResidual = scale > 0.0 ? a.systemResidual / scale : 0.0;
    a.balanced = std::abs(a.relativeResidual) <= relativeTolerance;
    const auto relative = [scale](double v) {
        std::ostringstream s;
        s.precision(3);
        s << std::scientific << (scale > 0.0 ? v / scale : 0.0);
        return s.str();
    };
    if (a.transferError != 0.0)
        a.attribution.push_back("transfer (mapping) error " + relative(a.transferError) + " of the largest term");
    if (couplingError != 0.0)
        a.attribution.push_back("coupling error: the last iteration changed the transferred power by " + relative(couplingError) + " of the largest term");
    for (const ParticipantBalance& b : a.participants)
        if (b.solverResidual != 0.0)
            a.attribution.push_back("solver balance of " + b.participant + ": " + relative(b.solverResidual) + " of the largest term");
    if (a.attribution.empty())
        a.attribution.push_back("no known source of imbalance beyond rounding");
    return a;
}

nlohmann::json Audit::toJson() const
{
    nlohmann::json j;
    j["mode"] = std::string(toString(mode));
    j["externalInput"] = externalInput;
    j["boundaryOut"] = boundaryOut;
    j["dissipated"] = dissipated;
    j["stored"] = stored;
    j["transferError"] = transferError;
    j["systemResidual"] = systemResidual;
    j["relativeResidual"] = relativeResidual;
    j["tolerance"] = tolerance;
    j["balanced"] = balanced;
    j["attribution"] = attribution;
    j["participants"] = nlohmann::json::array();
    for (const ParticipantBalance& b : participants)
        j["participants"].push_back({{"participant", b.participant}, {"domain", b.domain}, {"input", b.input}, {"transferIn", b.transferIn},
                                     {"boundaryOut", b.boundaryOut}, {"dissipated", b.dissipated}, {"stored", b.stored},
                                     {"transferOut", b.transferOut}, {"residual", b.residual}, {"relativeResidual", b.relativeResidual()},
                                     {"solverResidual", b.solverResidual}});
    return j;
}

nlohmann::json EnergyLedger::toJson() const
{
    nlohmann::json j;
    j["mode"] = std::string(toString(mode_));
    j["terms"] = nlohmann::json::array();
    for (const LedgerTerm& t : terms_)
        j["terms"].push_back({{"participant", t.participant}, {"domain", t.domain}, {"kind", std::string(toString(t.kind))},
                              {"mechanism", t.mechanism}, {"value", t.value}, {"region", t.region}, {"description", t.description}});
    j["transfers"] = nlohmann::json::array();
    for (const TransferRecord& r : transfers_)
        j["transfers"].push_back({{"coupling", r.coupling}, {"model", r.model}, {"sourceParticipant", r.sourceParticipant},
                                  {"sourceDomain", r.sourceDomain}, {"destinationParticipant", r.destinationParticipant},
                                  {"destinationDomain", r.destinationDomain}, {"mechanism", r.mechanism}, {"sourceQuantity", r.sourceQuantity},
                                  {"targetQuantity", r.targetQuantity}, {"unit", r.unit}, {"region", r.region}, {"interval", r.interval},
                                  {"amount", r.amount}, {"method", r.method}, {"sourceIntegral", r.sourceIntegral},
                                  {"targetIntegral", r.targetIntegral}, {"relativeDifference", r.relativeDifference},
                                  {"approximation", r.approximation}, {"limitation", r.limitation}, {"energy", r.energy}});
    return j;
}

}  // namespace resonant::multiphysics
