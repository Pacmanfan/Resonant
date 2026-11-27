#pragma once

// Linear-elastic, isotropic material for structural analysis.
//
// Only E, nu and rho are independent; every other constant is derived from
// them, so they cannot contradict each other. Anisotropic (orthotropic,
// fibre-reinforced) materials need a constitutive tensor and are not
// represented: such a material is treated with the single modulus it
// stores, and its provenance note says so.
#include "foundation/Result.h"
#include "materials/Material.h"

#include <string>

namespace resonant::mechanics {

struct ElasticMaterial {
    std::string name;
    double density = 0.0;        // kg/m^3
    double youngsModulus = 0.0;  // Pa
    double poissonsRatio = 0.0;  // -
    double lossFactor = 0.0;     // structural loss factor eta, 0 when the material states none
    bool lossFactorStated = false;
    std::string provenance;      // the source / provenance notes of the values, for records

    Status validate() const;

    double shearModulus() const { return youngsModulus / (2.0 * (1.0 + poissonsRatio)); }
    double bulkModulus() const { return youngsModulus / (3.0 * (1.0 - 2.0 * poissonsRatio)); }
    double lameLambda() const
    {
        return youngsModulus * poissonsRatio / ((1.0 + poissonsRatio) * (1.0 - 2.0 * poissonsRatio));
    }
    // Speed of a longitudinal wave in a thin rod: sqrt(E / rho).
    double barSpeed() const;
    // Bulk waves in an unbounded solid.
    double longitudinalSpeed() const;  // sqrt((lambda + 2 G) / rho)
    double shearSpeed() const;         // sqrt(G / rho)
    // Bending stiffness of a plate of thickness h: E h^3 / (12 (1 - nu^2)).
    double plateBendingStiffness(double thickness) const;
};

// The mechanical properties of a material record: density, Young's modulus
// and Poisson's ratio are required; the loss factor is used when present.
Result<ElasticMaterial> elasticMaterialOf(const Material& material);

}  // namespace resonant::mechanics
