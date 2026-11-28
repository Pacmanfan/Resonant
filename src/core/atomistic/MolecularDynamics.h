#pragma once

// NVE molecular dynamics of a pair potential (RES-MSC-009): velocity Verlet in an
// orthorhombic periodic cell (minimum image), a shifted-force cutoff (energy and force
// continuous at rc), Maxwell-Boltzmann initial velocities (seeded, zero total momentum).
// Observables: kinetic, potential and total energy, temperature, the radial distribution
// function and the mean squared displacement. Energy drift and its time-step scaling say
// whether a run is reliable; one short run is not a converged average.

#include "atomistic/ForceField.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace resonant::atomistic {

struct MdSettings {
    double timeStep = 0.0;      // s
    int steps = 1000;
    double temperature = 0.0;   // K, of the initial velocities
    std::uint64_t seed = 1;
    int sampleEvery = 10;
    double cutoffSigma = 2.5;
    int rdfBins = 100;
};

struct MdResult {
    std::vector<double> time, kinetic, potential, total, temperature;  // s, J, J, J, K
    std::vector<double> rdfRadius, rdf;                                // m, g(r)
    std::vector<double> msd;                                           // m^2, at the sample times
    double energyDrift = 0.0;     // max |E(t) - E(0)| / mean kinetic energy
    double meanTemperature = 0.0;
    int atoms = 0;
};

Result<MdResult> runNve(const Crystal& structure, const ForceField& field, const MdSettings& settings,
                        const std::function<bool()>& cancelled = {});

}  // namespace resonant::atomistic
