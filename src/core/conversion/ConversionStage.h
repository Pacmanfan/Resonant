#pragma once

// Energy-conversion stages and chains (REQ-CHAT-20261128-005, RES-CONV-001, ADR-000233).
//
// Every conversion mechanism reports one stage: what enters (in one energy
// form), what leaves (the converted output, and any other output such as the
// heat a thermoelectric generator rejects), what is stored and what is
// dissipated, with its coupling coefficients, boundary conditions, assumptions,
// validity limits and uncertainty. A chain joins stages: the output of one is
// the input of the next, counted once. A chain becomes an accounting ledger
// (accounting/AccountingLedger), evaluated by the conservation checks of the
// accounting framework: energy never appears or disappears through an
// undocumented operation, and a stage whose balance is incomplete says so.

#include "accounting/AccountingLedger.h"
#include "accounting/Conservation.h"

#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace resonant::conversion {

// The energy forms of the conversion architecture.
enum class EnergyForm { Mechanical, Acoustic, Thermal, Electrical, Electromagnetic, Optical, Elastic, Dissipative };
std::string_view toString(EnergyForm form);
std::optional<EnergyForm> energyFormFromString(std::string_view text);

struct StageOutput {
    std::string name;  // "heat rejected at the cold side", "reflected wave", ...
    EnergyForm form = EnergyForm::Thermal;
    double amount = 0.0;  // W or J, like the stage
};

struct ConversionStage {
    std::string id, name;
    std::string mechanism;  // piezoelectric, thermoelectric-generator, magnetostrictive-harvester, photovoltaic, acoustic-absorption, ...
    std::string model;      // the implementation that computed it
    EnergyForm inputForm = EnergyForm::Mechanical, outputForm = EnergyForm::Electrical;
    double input = 0.0;        // W (rate) or J (interval)
    double output = 0.0;       // the converted output, passed to the next stage
    std::vector<StageOutput> otherInputs;   // energy entering in another form (a cooler's absorbed heat)
    std::vector<StageOutput> otherOutputs;  // energy leaving in another form, not dissipated
    double storedChange = 0.0;   // W or J (a steady periodic state: 0)
    double storedPeak = 0.0;     // J, the peak stored energy (reported; not part of a rate balance)
    double dissipated = 0.0;     // W or J, to heat inside the stage
    std::map<std::string, double> coupling;  // the coupling coefficients (k^2, theta, ZT, ...)
    std::vector<std::string> boundaryConditions, assumptions, validity;
    std::optional<double> uncertainty;  // relative standard uncertainty of the output
    std::vector<std::string> missing;   // contributions the model does not compute (the balance is then incomplete)

    double otherInput() const;
    double otherOutput() const;
    // input + other inputs - output - other outputs - dissipated - stored change.
    double residual() const;
    double efficiency() const;  // output / input (0 without input)
    nlohmann::json toJson() const;
};

struct ConversionChain {
    std::string id, title;
    bool rate = true;  // time-averaged powers (W); else energies over an interval (J)
    std::vector<ConversionStage> stages;
};

struct StageCheck {
    std::string stage;
    accounting::CheckStatus status = accounting::CheckStatus::NotEvaluated;
    double residual = 0.0;
    std::optional<double> relative;
    std::vector<std::string> missing;
};

struct LinkCheck {
    std::string from, to;
    double leaving = 0.0, entering = 0.0;  // the output of `from`, the input of `to`
    bool joined = false;
    std::string note;
};

struct ChainReport {
    std::vector<StageCheck> stages;
    std::vector<LinkCheck> links;
    accounting::CheckStatus status = accounting::CheckStatus::NotEvaluated;  // the worst of the stages and links
    double overallEfficiency = 0.0;  // the last output over the first input
    accounting::Ledger ledger;
    nlohmann::json toJson() const;
};

// The ledger of a chain: a source feeding the first stage, each stage a body,
// each output the next stage's input (the sending stage's output as the source
// amount), dissipation and other outputs to the surroundings, the last output
// into the load.
accounting::Ledger chainLedger(const ConversionChain& chain);
// Each stage's balance through the accounting framework, and every join.
ChainReport evaluateChain(const ConversionChain& chain, const accounting::Tolerances& tolerances = {});

}  // namespace resonant::conversion
