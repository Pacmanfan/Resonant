#pragma once

// External atomistic, molecular and electronic-structure engines (RES-MSC-010). Each is
// a candidate integration through an adapter run as a separate process (never linked);
// none is integrated in this build. Detection looks for the engine's executables on PATH.

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::multiscale {

struct ExternalSolver {
    std::string id, name, scale, license;
    std::vector<std::string> executables;  // looked for on PATH
    std::string pythonModule;              // its Python package, if any (not probed by the core)
    std::vector<std::string> provides;
    std::string install;                   // how it would be installed
    std::string status = "not integrated"; // no adapter exists in this build
    bool found = false;                    // an executable was found on PATH
    std::string foundPath{};
    nlohmann::json toJson() const;
};

// The candidates with their detection on this machine.
std::vector<ExternalSolver> externalSolvers();

}  // namespace resonant::multiscale
