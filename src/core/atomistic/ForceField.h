#pragma once

// Force fields (RES-MSC-008): interaction models are scientific models with an
// identity, a version, a parameter source, an applicability and limitations - not
// tuning parameters. The 12-6 Lennard-Jones form with Lorentz-Berthelot mixing.

#include "atomistic/Crystal.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::atomistic {

struct SpeciesParameters {
    double epsilon = 0.0;  // J
    double sigma = 0.0;    // m
    double mass = 0.0;     // kg
};

struct ForceField {
    std::string id, name, version;
    std::string form = "lennard-jones-12-6";
    std::map<std::string, SpeciesParameters> species;  // the applicability: only these species
    double cutoffSigma = 10.0;                         // default statics cutoff, in the largest sigma (tail-corrected: 2e-6 in the FCC energy)
    std::string source;                                // where the parameters come from
    std::string validity;
    std::vector<std::string> limitations;

    // Lorentz-Berthelot: eps_ij = sqrt(eps_i eps_j), sigma_ij = (sigma_i + sigma_j) / 2.
    SpeciesParameters pair(const std::string& a, const std::string& b) const;
    double maxSigma() const;
    nlohmann::json toJson() const;
};

const std::vector<ForceField>& forceFields();
const ForceField* findForceField(std::string_view id);

// Species of the structure the force field does not cover.
std::vector<std::string> incompatibleSpecies(const ForceField& field, const Crystal& structure);

}  // namespace resonant::atomistic
