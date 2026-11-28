#pragma once

// The thermal properties of a material, read from its thermal property set
// (RES-CEME-010): density, conductivity, specific heat and the linear
// expansion coefficient, with the provenance of the values.
#include "foundation/Result.h"
#include "materials/Material.h"

#include <string>

namespace resonant::thermal {

struct ThermalMaterial {
    std::string name;
    double density = 0.0;              // kg/m^3
    double conductivity = 0.0;         // W/(m K)
    double specificHeat = 0.0;         // J/(kg K)
    double expansion = 0.0;            // 1/K, linear
    bool expansionStated = false;
    std::string provenance;            // the provenance notes of the values

    double volumetricHeatCapacity() const { return density * specificHeat; }  // J/(m^3 K)
    double diffusivity() const { return conductivity / volumetricHeatCapacity(); }  // m^2/s
};

// Fails when the material has no thermal property set or lacks density,
// conductivity or specific heat.
Result<ThermalMaterial> thermalMaterial(const Material& material);

}  // namespace resonant::thermal
