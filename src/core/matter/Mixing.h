#pragma once

// Effective properties of compositions (RES-FAM-008): a combination is given
// only by a named method that applies to the kind of property, always with the
// rigorous bounds and the method's validity. There is no unqualified average.
//
//   density        volume average (exact)
//   specific heat  mass-weighted average (exact for a mechanical mixture)
//   transport      conductivity, permittivity, permeability: Wiener bounds (any
//                  microstructure), Hashin-Shtrikman bounds (isotropic),
//                  Maxwell Garnett (dilute spheres in a matrix), Bruggeman
//                  (symmetric random mixture)
//   elastic        Young's modulus with Poisson's ratio through the bulk and shear
//                  moduli: Voigt, Reuss and Hill; Hashin-Shtrikman-Walpole bounds;
//                  Gibson-Ashby scaling of open-cell solids
//   expansion      Turner (uniform hydrostatic stress), within the constituents' range
//
// Other properties (loss factors, loss tangents, sound speeds) have no estimator:
// they depend on the microstructure and must be measured or derived.

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::matter {

enum class PropertyKind { Density, SpecificHeat, Transport, Elastic, Expansion, Other };
PropertyKind propertyKindOf(std::string_view key);  // "thermal.thermalConductivity" -> Transport
std::string_view toString(PropertyKind kind);

struct PhaseProperty {
    std::string id;
    double fraction = 0.0;  // volume fraction
    double value = 0.0;     // the property (Young's modulus for elastic)
    double poisson = 0.0;   // elastic: Poisson's ratio
    double density = 0.0;   // specific heat: the mass weighting
    double bulkModulus = 0.0;  // expansion: the stiffness weighting
};

struct EffectiveEstimate {
    std::string key;
    std::string method;
    double value = std::numeric_limits<double>::quiet_NaN();
    double poisson = std::numeric_limits<double>::quiet_NaN();  // elastic
    double lower = std::numeric_limits<double>::quiet_NaN(), upper = std::numeric_limits<double>::quiet_NaN();
    std::string bounds;  // what the bounds are
    double absoluteLower = std::numeric_limits<double>::quiet_NaN(), absoluteUpper = std::numeric_limits<double>::quiet_NaN();
    std::string absoluteBounds;
    bool exact = false;  // density, specific heat
    bool valid = true;
    std::string validity;
    std::vector<std::string> assumptions;
    nlohmann::json toJson() const;
};

// The methods for a kind of property (the first is the default when one is exact).
std::vector<std::string> methodsFor(PropertyKind kind);

// Combines the phases (volume fractions summing to 1) by the method. `matrix` is the
// index of the matrix phase (Maxwell Garnett, Gibson-Ashby), -1 when not needed.
Result<EffectiveEstimate> combine(std::string_view key, const std::vector<PhaseProperty>& phases, std::string_view method, int matrix = -1);

// The building blocks, for tests and for the bounds of any estimate.
std::pair<double, double> wienerBounds(const std::vector<PhaseProperty>& phases);           // series, parallel
std::pair<double, double> hashinShtrikmanBounds(const std::vector<PhaseProperty>& phases);  // transport, isotropic 3D
double maxwellGarnett(const std::vector<PhaseProperty>& phases, int matrix);
double bruggeman(const std::vector<PhaseProperty>& phases);
struct ElasticModuli {
    double bulk = 0.0, shear = 0.0;
    double youngs() const;
    double poisson() const;
};
ElasticModuli elasticFrom(double youngs, double poisson);
// Hashin-Shtrikman-Walpole bounds on the bulk and shear moduli (lower, upper).
std::pair<ElasticModuli, ElasticModuli> walpoleBounds(const std::vector<PhaseProperty>& phases);

}  // namespace resonant::matter
