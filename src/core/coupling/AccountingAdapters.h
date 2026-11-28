#pragma once

// Adapters from authoritative solver outputs to accounting ledgers (REQ-CHAT-20261126-022,
// RES-ACC-001, -005..009, -012..015, ADR-000226). Each reads what its model computed and
// recomputes none of its physics:
//   networkLedger        a lumped network's transient with element flows: kinetic and elastic
//                        holdings, the elements' work and impulses on each body, dissipation
//                        to heat, the applied force's work and impulse, force records with
//                        their action-reaction pairs; translational or rotational
//   drivenNetworkLedger  a driven steady state (rate basis) with the stored energy as a quantity
//                        and the Q = w E / P check against the modal Q
//   buildupDecayCheck    the buildup and decay of a driven resonator against its modal decay rate
//   coupledLedger        the coupled engine's terms and transfer records (with the audit's
//                        solver residuals); momentum declared missing
//   evolutionLedger      the steps of a time-stepped evolution, as histories
//   radiationLedger      a layered body in a plane wave: energy and momentum, the force by two
//                        methods, the absorbed power locally and globally
//   materialStateLedger  a material-state run's work, stored energy by form, dissipation, latent
//                        heat, entropy and the hysteron check

#include "accounting/AccountingLedger.h"
#include "electromagnetics/LayeredMedia.h"
#include "oscillators/LumpedNetwork.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <string>
#include <vector>

namespace resonant::coupling {

struct NetworkAccounting {
    bool rotational = false;              // coordinates are angles: inertias, torsional springs, torques
    accounting::Vec3 axis{1.0, 0.0, 0.0}; // the direction of the coordinates (or the rotation axis)
    std::vector<std::string> objects;     // the scene object of each coordinate (optional)
    std::string title;
};

Result<accounting::Ledger> networkLedger(const oscillators::Network& network, const oscillators::TransientRecord& record, const NetworkAccounting& options);
Result<accounting::Ledger> drivenNetworkLedger(const oscillators::Network& network, const std::vector<std::complex<double>>& force, double frequencyHz,
                                               const NetworkAccounting& options);
// The transient was driven at `frequencyHz` until `driveOff` (s) and then left free.
accounting::Check buildupDecayCheck(const oscillators::Network& network, const oscillators::TransientRecord& record, double frequencyHz,
                                    const std::vector<std::complex<double>>& force, double driveOff, double tolerance);

Result<accounting::Ledger> coupledLedger(const nlohmann::json& ledger, const nlohmann::json& audit, const std::string& title);
Result<accounting::Ledger> evolutionLedger(const nlohmann::json& evolution);
// `area`: the illuminated area (m^2) the per-area results are multiplied by.
Result<accounting::Ledger> radiationLedger(const em::LayeredSolution& solution, double area, int points, double tolerance);
// `temperature`: the temperature of an isothermal mechanical model's surroundings (K), for its entropy.
Result<accounting::Ledger> materialStateLedger(const nlohmann::json& run, double temperature = 293.15);

}  // namespace resonant::coupling
