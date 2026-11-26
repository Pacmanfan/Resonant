// The propagation medium of the analytical acoustic models: a homogeneous
// fluid at rest. It is chosen explicitly for a simulation; it is NOT taken
// from the material of any solid object in the scene.
#pragma once

#include "foundation/Result.h"

#include <nlohmann/json_fwd.hpp>

#include <string>

namespace resonant::acoustics {

struct Medium {
    std::string name = "Air";
    double density = 1.204;            // kg/m^3
    double soundSpeed = 343.2;         // m/s
    double attenuation = 0.0;          // Np/m, amplitude attenuation at the simulated frequency
    double temperatureK = 293.15;      // K, the temperature the values refer to
    // Where the numbers come from and what they assume, shown to the user
    // and stored with every result.
    std::string basis;

    double impedance() const { return density * soundSpeed; }  // Pa s/m (rayl)
    double wavenumber(double frequencyHz) const;                // rad/m
    double wavelength(double frequencyHz) const;                // m

    Status validate() const;

    friend bool operator==(const Medium&, const Medium&) = default;
};

// Dry air as an ideal gas at 101 325 Pa:
//   c   = sqrt(gamma * R_specific * T),  gamma = 1.4, R_specific = 287.05 J/(kg K)
//   rho = p / (R_specific * T)
// A theoretical model: humidity, pressure and real-gas effects are ignored
// and attenuation is left at zero. Valid for roughly 250 K to 330 K.
Medium airAtTemperature(double temperatureK);

// Fresh water with fixed nominal values (rho = 998 kg/m^3, c = 1482 m/s near
// 293 K). Illustrative: the temperature, salinity and pressure dependence of
// water is not modelled, so the temperature is informational only.
Medium waterNominal();

nlohmann::json toJson(const Medium& medium);
Result<Medium> mediumFromJson(const nlohmann::json& value);

}  // namespace resonant::acoustics
