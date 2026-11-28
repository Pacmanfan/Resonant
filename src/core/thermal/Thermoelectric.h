#pragma once

// Thermoelectric conversion: a couple of p and n legs between two junction
// temperatures, as a generator or a cooler (REQ-CHAT-20261128-005, RES-CONV-003,
// ADR-000233).
//
// Each leg, steady and one-dimensional, with temperature-dependent Seebeck
// coefficient alpha(T), resistivity rho(T) and conductivity kappa(T):
//     d/dx (kappa dT/dx) + rho J^2 - T (d alpha / dT) (dT/dx) J = 0
// (conduction, Joule heating, the Thomson effect), the heat flux q = alpha T J -
// kappa dT/dx (Peltier transport and conduction). Finite differences on each leg,
// properties iterated (Picard); the current of a generator is iterated with its
// load. The junction heats come from the end gradients, the electrical output
// from the voltage integral, so Q_h - Q_c - P is an independent check of the
// energy balance (exact for constant properties). The entropy generated inside
// the couple, Q_c / T_c - Q_h / T_h, is reported and must not be negative.
//
// Not modelled: contact thermal resistances at the junctions, heat losses from
// the leg sides, radiation, transient operation. The junction temperatures are
// imposed.

#include "conversion/ConversionStage.h"
#include "foundation/Result.h"

#include <optional>
#include <string>
#include <vector>

namespace resonant::thermal {

// p(T) = sum_i c_i T^i (T in K).
struct TemperatureFunction {
    std::vector<double> coefficients;
    double at(double t) const;
    double derivative(double t) const;
    static TemperatureFunction constant(double value) { return {{value}}; }
};

struct ThermoelectricLeg {
    std::string name;
    double length = 0.0, area = 0.0;     // m, m^2
    TemperatureFunction seebeck;          // V/K (p-type positive, n-type negative)
    TemperatureFunction resistivity;      // ohm m
    TemperatureFunction conductivity;     // W/(m K)
    std::string provenance;
};

struct ThermoelectricCouple {
    ThermoelectricLeg p, n;
    double contactResistance = 0.0;  // ohm per couple (electrical)
    int couples = 1;                 // electrically in series, thermally in parallel
    Status validate() const;
};

enum class ThermoelectricMode { Generator, Cooler };

struct ThermoelectricOperation {
    double hotTemperature = 0.0, coldTemperature = 0.0;  // K, imposed at the junctions
    std::optional<double> loadResistance;   // generator: the external load, ohm
    std::optional<double> current;          // generator with an imposed current, or the cooler's drive current (A, > 0 pumps heat from the cold side)
    int nodes = 201;                        // per leg
};

struct ThermoelectricResult {
    double current = 0.0;              // A (generator: through the load; cooler: the drive current)
    double voltage = 0.0;              // V across the terminals
    double heatHot = 0.0;              // W into the couple at the hot junction (generator); rejected there (cooler)
    double heatCold = 0.0;             // W out at the cold junction (generator); absorbed there (cooler)
    double electricalPower = 0.0;      // W to the load (generator) or supplied (cooler), from I and V
    double joule = 0.0;                // W, in the legs and contacts
    double thomson = 0.0;              // W, the net Thomson heat absorbed in the legs
    double openCircuitVoltage = 0.0;   // V
    double internalResistance = 0.0;   // ohm
    double efficiency = 0.0;           // generator: P / Q_h
    double cop = 0.0;                  // cooler: Q_c / P
    double entropyGeneration = 0.0;    // W/K inside the couple
    double energyResidual = 0.0;       // W: the heats against the electrical power (discretization error)
    int iterations = 0;
    std::vector<double> x, temperatureP, temperatureN;  // the profiles
    conversion::ConversionStage stage;
    std::vector<std::string> assumptions;
};

Result<ThermoelectricResult> solveThermoelectric(const ThermoelectricCouple& couple, ThermoelectricMode mode, const ThermoelectricOperation& operation);

// The constant-property closed form: the maximum generator efficiency for the mean
// figure of merit Z T_mean.
double maximumGeneratorEfficiency(double zTmean, double hot, double cold);

}  // namespace resonant::thermal
