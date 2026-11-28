#pragma once

// Model cards (RES-MSC-004): what each representation kind and calculation method
// represents, at which scale, with which assumptions, equations, parameters, excluded
// effects and valid conditions, and how to read its results.

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace resonant::multiscale {

struct ModelCard {
    std::string id, title;
    std::string system;  // the physical system represented
    std::string scale;
    std::vector<std::string> assumptions;
    std::vector<std::string> methods;     // equations or methods
    std::vector<std::string> parameters;  // what it needs
    std::vector<std::string> excluded;    // effects it leaves out
    std::string validity;
    std::string interpretation;
    nlohmann::json toJson() const;
};

const std::vector<ModelCard>& modelCards();
const ModelCard* findModelCard(std::string_view id);

}  // namespace resonant::multiscale
