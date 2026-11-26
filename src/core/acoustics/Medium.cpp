#include "acoustics/Medium.h"

#include "foundation/Units.h"

#include <nlohmann/json.hpp>

#include <cmath>

namespace resonant::acoustics {

double Medium::wavenumber(double frequencyHz) const
{
    return 2.0 * units::pi * frequencyHz / soundSpeed;
}

double Medium::wavelength(double frequencyHz) const
{
    return soundSpeed / frequencyHz;
}

Status Medium::validate() const
{
    if (!(std::isfinite(density) && density > 0.0))
        return Status::failure("the medium's density must be greater than zero");
    if (!(std::isfinite(soundSpeed) && soundSpeed > 0.0))
        return Status::failure("the medium's speed of sound must be greater than zero");
    if (!(std::isfinite(attenuation) && attenuation >= 0.0))
        return Status::failure("the medium's attenuation must not be negative");
    if (!(std::isfinite(temperatureK) && temperatureK > 0.0))
        return Status::failure("the medium's temperature must be greater than zero kelvin");
    return Status::success();
}

Medium airAtTemperature(double temperatureK)
{
    constexpr double gamma = 1.4;
    constexpr double specificGasConstant = 287.05;  // J/(kg K), dry air
    constexpr double pressure = 101325.0;           // Pa
    Medium medium;
    medium.name = "Air";
    medium.temperatureK = temperatureK;
    medium.soundSpeed = std::sqrt(gamma * specificGasConstant * temperatureK);
    medium.density = pressure / (specificGasConstant * temperatureK);
    medium.attenuation = 0.0;
    medium.basis = "Theoretical: dry air as an ideal gas at 101 325 Pa (gamma 1.4, R 287.05 J/(kg K)). "
                   "Humidity and absorption are not modelled.";
    return medium;
}

Medium waterNominal()
{
    Medium medium;
    medium.name = "Water";
    medium.temperatureK = 293.15;
    medium.soundSpeed = 1482.0;
    medium.density = 998.0;
    medium.attenuation = 0.0;
    medium.basis = "Illustrative nominal values for fresh water near 293 K; not authoritative. "
                   "Temperature, salinity and pressure dependence are not modelled.";
    return medium;
}

nlohmann::json toJson(const Medium& medium)
{
    return {{"name", medium.name},         {"density", medium.density},
            {"soundSpeed", medium.soundSpeed}, {"attenuation", medium.attenuation},
            {"temperatureK", medium.temperatureK}, {"basis", medium.basis}};
}

Result<Medium> mediumFromJson(const nlohmann::json& value)
{
    if (!value.is_object())
        return Result<Medium>::failure("the medium must be an object");
    Medium medium;
    try {
        medium.name = value.at("name").get<std::string>();
        medium.density = value.at("density").get<double>();
        medium.soundSpeed = value.at("soundSpeed").get<double>();
        medium.attenuation = value.value("attenuation", 0.0);
        medium.temperatureK = value.value("temperatureK", 293.15);
        medium.basis = value.value("basis", std::string());
    } catch (const nlohmann::json::exception& error) {
        return Result<Medium>::failure(std::string("invalid medium: ") + error.what());
    }
    if (Status status = medium.validate(); !status)
        return Result<Medium>::failure(status.message());
    return medium;
}

}  // namespace resonant::acoustics
