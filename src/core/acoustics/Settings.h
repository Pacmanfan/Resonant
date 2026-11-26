// Project-level acoustic settings: the propagation medium, the options of
// the propagation model, the analysis frequency and the memory limit. They
// are stored in the project's "extensions" under the key "acoustics", so
// they are saved and loaded with the project.
#pragma once

#include "acoustics/Medium.h"
#include "acoustics/Propagation.h"
#include "project/Project.h"
#include "simulation/SimulationResult.h"

namespace resonant::acoustics {

struct AcousticSettings {
    Medium medium = airAtTemperature(293.15);
    PropagationOptions propagation;
    double frequencyHz = 40000.0;   // the frequency field simulations are run at
    size_t memoryLimitBytes = 1024ull * 1024ull * 1024ull;

    Status validate() const;
    friend bool operator==(const AcousticSettings&, const AcousticSettings&) = default;
};

inline constexpr const char* kSettingsKey = "acoustics";

nlohmann::json toJson(const AcousticSettings& settings);
Result<AcousticSettings> settingsFromJson(const nlohmann::json& value);

// The project's settings; defaults when it has none or they are invalid.
AcousticSettings settingsOf(const Project& project);
void storeSettings(Project& project, const AcousticSettings& settings);

// A summary record of a result, kept in the project so the history of runs
// survives saving (the sampled field itself is not stored in the project
// file).
nlohmann::json runRecord(const SimulationResult& result);

}  // namespace resonant::acoustics
