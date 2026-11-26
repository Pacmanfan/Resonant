#include "acoustics/Settings.h"

#include <cmath>

namespace resonant::acoustics {

Status AcousticSettings::validate() const
{
    if (Status status = medium.validate(); !status)
        return status;
    if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0))
        return Status::failure("the simulation frequency must be greater than zero");
    if (!(std::isfinite(propagation.minimumDistance) && propagation.minimumDistance > 0.0))
        return Status::failure("the minimum distance must be greater than zero");
    if (memoryLimitBytes < 1024 * 1024)
        return Status::failure("the memory limit must be at least 1 MiB");
    if (modelId != kFreeFieldModelId && modelId != kReflectorModelId)
        return Status::failure("unknown field model '" + modelId + "'");
    if (!(std::isfinite(reflectorElementsPerWavelength) && reflectorElementsPerWavelength >= 2.0
          && reflectorElementsPerWavelength <= 40.0))
        return Status::failure("the reflector surface resolution must be between 2 and 40 elements per wavelength");
    return Status::success();
}

nlohmann::json toJson(const AcousticSettings& settings)
{
    return {{"medium", toJson(settings.medium)},
            {"propagation",
             {{"includeAttenuation", settings.propagation.includeAttenuation},
              {"includeDirectivity", settings.propagation.includeDirectivity},
              {"minimumDistance", settings.propagation.minimumDistance}}},
            {"frequencyHz", settings.frequencyHz},
            {"memoryLimitBytes", settings.memoryLimitBytes},
            {"model", settings.modelId},
            {"reflectorElementsPerWavelength", settings.reflectorElementsPerWavelength}};
}

Result<AcousticSettings> settingsFromJson(const nlohmann::json& value)
{
    AcousticSettings settings;
    if (!value.is_object())
        return Result<AcousticSettings>::failure("acoustic settings must be an object");
    try {
        if (value.contains("medium")) {
            auto medium = mediumFromJson(value["medium"]);
            if (!medium)
                return Result<AcousticSettings>::failure(medium.message());
            settings.medium = medium.take();
        }
        if (value.contains("propagation")) {
            const auto& p = value["propagation"];
            settings.propagation.includeAttenuation = p.value("includeAttenuation", true);
            settings.propagation.includeDirectivity = p.value("includeDirectivity", true);
            settings.propagation.minimumDistance = p.value("minimumDistance", 1e-4);
        }
        settings.frequencyHz = value.value("frequencyHz", 40000.0);
        settings.memoryLimitBytes = value.value("memoryLimitBytes", settings.memoryLimitBytes);
        settings.modelId = value.value("model", std::string(kFreeFieldModelId));
        settings.reflectorElementsPerWavelength = value.value("reflectorElementsPerWavelength", 6.0);
    } catch (const nlohmann::json::exception& error) {
        return Result<AcousticSettings>::failure(std::string("invalid acoustic settings: ") + error.what());
    }
    if (Status status = settings.validate(); !status)
        return Result<AcousticSettings>::failure(status.message());
    return settings;
}

AcousticSettings settingsOf(const Project& project)
{
    const auto it = project.extensions.find(kSettingsKey);
    if (it == project.extensions.end())
        return AcousticSettings();
    auto parsed = settingsFromJson(*it);
    return parsed ? parsed.take() : AcousticSettings();
}

void storeSettings(Project& project, const AcousticSettings& settings)
{
    nlohmann::json value = toJson(settings);
    // Keep whatever else is stored under the key (the run history).
    if (const auto it = project.extensions.find(kSettingsKey); it != project.extensions.end() && it->is_object())
        for (const auto& [key, entry] : it->items())
            if (!value.contains(key))
                value[key] = entry;
    project.extensions[kSettingsKey] = std::move(value);
}

nlohmann::json runRecord(const SimulationResult& result)
{
    return {{"id", result.id.str()}, {"name", result.name}, {"kind", result.kind()}, {"metadata", toJson(result.metadata)}};
}

}  // namespace resonant::acoustics
