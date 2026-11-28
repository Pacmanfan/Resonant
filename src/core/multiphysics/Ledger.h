#pragma once

// Energy accounting of a coupled run (RES-CEME-009,
// docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md).
//
// Participants add their terms; the engine adds one transfer record per
// exchange between participants. Per participant
//
//   residual = input + transferIn - boundaryOut - dissipated - stored - transferOut
//
// and for the system the same sum, in which every transfer appears once on
// each side, so energy moving between domains is not counted twice: what is
// left of the transfers is the difference between their source and target
// integrals (the transfer error). Steady states are accounted in watts,
// transient ones in joules over the interval. Informational terms (for example
// elastic strain energy in a quasi-static linear analysis, which draws on no
// modeled energy) are reported but not summed.
#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::multiphysics {

enum class TermKind { Input, BoundaryOut, Dissipated, Stored, TransferOut, TransferIn, Informational };
std::string_view toString(TermKind kind);

enum class BalanceMode { SteadyPower, EnergyOverInterval };
std::string_view toString(BalanceMode mode);

struct LedgerTerm {
    std::string participant, domain;
    TermKind kind = TermKind::Input;
    std::string mechanism;    // "incident plane wave", "convection at -z", "dielectric and conduction loss"
    double value = 0.0;       // W (steady) or J (interval), positive in the direction its kind names
    std::string region;
    std::string description;
};

struct TransferRecord {
    std::string coupling;  // the coupling's id in the physical system
    std::string model;     // the coupling model
    std::string sourceParticipant, sourceDomain, destinationParticipant, destinationDomain;
    std::string mechanism;
    std::string sourceQuantity, targetQuantity, unit;
    std::string region;
    std::string interval = "steady";
    double amount = 0.0;            // W or J: the target-side integral (what the destination receives)
    std::string method;             // conservative / interpolating
    double sourceIntegral = 0.0, targetIntegral = 0.0, relativeDifference = 0.0;
    std::string approximation, limitation;
    bool energy = true;             // false for transfers of intensive quantities (temperature): not in the balance
};

struct ParticipantBalance {
    std::string participant, domain;
    double input = 0.0, transferIn = 0.0, boundaryOut = 0.0, dissipated = 0.0, stored = 0.0, transferOut = 0.0;
    double residual = 0.0;
    double scale = 0.0;           // the largest term, for relative residuals
    double solverResidual = 0.0;  // what the participant's own solver reports (its discretization's balance)
    double relativeResidual() const { return scale > 0.0 ? residual / scale : 0.0; }
};

struct Audit {
    BalanceMode mode = BalanceMode::SteadyPower;
    std::vector<ParticipantBalance> participants;
    double externalInput = 0.0, boundaryOut = 0.0, dissipated = 0.0, stored = 0.0;
    double transferError = 0.0;   // sum over energy transfers of (target - source) integral
    double systemResidual = 0.0;  // external input - boundary out - dissipated - stored (+ transfer error accounted)
    double relativeResidual = 0.0;
    double tolerance = 0.0;
    bool balanced = false;        // |relative residual| <= tolerance
    std::vector<std::string> attribution;  // where the imbalance is known to come from
    nlohmann::json toJson() const;
};

class EnergyLedger {
public:
    explicit EnergyLedger(BalanceMode mode = BalanceMode::SteadyPower, double interval = 0.0) : mode_(mode), interval_(interval) {}
    BalanceMode mode() const { return mode_; }
    double interval() const { return interval_; }  // s, for EnergyOverInterval
    // What a steady power (W) is multiplied by to enter this ledger: 1, or the interval (J).
    double powerScale() const { return mode_ == BalanceMode::EnergyOverInterval ? interval_ : 1.0; }
    void add(LedgerTerm term) { terms_.push_back(std::move(term)); }
    void add(TransferRecord record) { transfers_.push_back(std::move(record)); }
    void setSolverResidual(const std::string& participant, double residual) { solverResiduals_[participant] = residual; }
    void clear();
    const std::vector<LedgerTerm>& terms() const { return terms_; }
    const std::vector<TransferRecord>& transfers() const { return transfers_; }

    // `couplingError`: the change of the last iteration, as a power or energy (0 when not iterative).
    Audit audit(double relativeTolerance, double couplingError = 0.0) const;
    nlohmann::json toJson() const;

private:
    BalanceMode mode_;
    double interval_ = 0.0;
    std::vector<LedgerTerm> terms_;
    std::vector<TransferRecord> transfers_;
    std::map<std::string, double> solverResiduals_;
};

}  // namespace resonant::multiphysics
