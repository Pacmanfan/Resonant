#pragma once

// Lumped oscillator networks (RES-RSN-004, RES-RSN-005, RES-RSN-009):
//     M x'' + C x' + K x = f(t)
// with resonators (a mass, a stiffness, dampers by mechanism) and couplings
// (springs and dampers between two coordinates), or general matrices built by
// a domain (an acoustic network of inertances and compliances). The physics of
// each domain lives in the domain that assembles the network; this module is
// the linear algebra and the energy bookkeeping, and includes no domain.
//
// Energy: kinetic 1/2 v^T M v; potential split into each resonator's own
// spring 1/2 k_i x_i^2 and each coupling's 1/2 k_c (x_i - x_j)^2 (the coupling
// energy belongs to the coupling, never to both resonators); dissipation per
// damper; supply integral f . v. supplied = delta stored + dissipated.

#include "foundation/Result.h"

#include <Eigen/Dense>

#include <complex>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::oscillators {

inline constexpr const char* kNetworkModelVersion = "1";

struct Damper {
    std::string mechanism;  // e.g. "radiation", "viscous", "structural", "wall"
    int i = 0, j = -1;      // between i and j, or to ground (j = -1)
    double c = 0.0;         // N s/m (or the domain's analogue)
};

struct Spring {
    std::string name;
    int i = 0, j = -1;      // between i and j, or to ground (j = -1)
    double k = 0.0;
};

struct Network {
    std::vector<std::string> names;   // coordinates (resonators)
    std::vector<double> masses;       // diagonal mass (or inertance) per coordinate
    std::vector<Spring> springs;      // grounded springs are each resonator's own stiffness
    std::vector<Damper> dampers;
    std::string unitsNote = "SI mechanical analogue";
    // Optional full matrices (a domain's network); when set they replace masses / springs.
    Eigen::MatrixXd massMatrix, stiffnessMatrix;
    std::map<std::string, Eigen::MatrixXd> dampingByMechanism;

    int size() const;
    Eigen::MatrixXd M() const;
    Eigen::MatrixXd K() const;
    Eigen::MatrixXd C() const;
    std::map<std::string, Eigen::MatrixXd> dampingMatrices() const;
    Status validate() const;
};

struct NetworkMode {
    double frequencyHz = 0.0;       // damped natural frequency Im(s) / 2 pi
    double undampedHz = 0.0;        // |s| / 2 pi
    double decayRate = 0.0;         // -Re(s), 1/s
    double quality = 0.0;           // |s| / (2 decay); infinity undamped
    std::vector<double> shape;      // real part of the mode shape (displacement), max |.| = 1
    std::vector<double> participation;  // fraction of the mode's kinetic energy in each coordinate
    std::string symmetry;           // "symmetric", "antisymmetric" or "" (two coordinates only)
    std::map<std::string, double> qualityByMechanism;  // modal Q of each damping mechanism (light damping)
};

// Complex natural modes from the state-space matrix, ascending frequency; rigid
// (zero-frequency) modes are left out and counted in `rigidModes`.
Result<std::vector<NetworkMode>> naturalModes(const Network& network, int* rigidModes = nullptr);

struct DrivenPoint {
    double frequencyHz = 0.0;
    std::vector<std::complex<double>> displacement;  // per coordinate
    double suppliedPower = 0.0;                      // time-averaged, W
    std::map<std::string, double> dissipated;        // per mechanism, W
    double kineticEnergy = 0.0, potentialEnergy = 0.0;  // time-averaged, J
};
// The steady response to harmonic forces (peak amplitudes, e^{+jwt}).
Result<std::vector<DrivenPoint>> drivenResponse(const Network& network, const std::vector<std::complex<double>>& force,
                                                const std::vector<double>& frequenciesHz);

struct TransientOptions {
    double duration = 0.0;
    double dt = 0.0;                 // 0: 1/200 of the shortest period
    std::vector<double> x0, v0;      // initial state (default zero)
    // f(t) per coordinate; empty = free vibration.
    std::function<std::vector<double>(double)> force;
    int samples = 400;               // stored samples (decimated)
};

struct TransientRecord {
    std::vector<double> time;
    std::vector<std::vector<double>> displacement;  // [coordinate][sample]
    std::vector<std::vector<double>> energy;        // [coordinate][sample]: kinetic + own spring
    std::vector<double> couplingEnergy;             // the coupling springs, per sample
    std::vector<double> supplied, dissipated;       // cumulative, per sample
    std::map<std::string, double> dissipatedByMechanism;  // total
    double residual = 0.0;                          // max |supplied - stored - dissipated + stored(0)| / max energy
    double dt = 0.0;
    long steps = 0;
};
// Fourth-order Runge-Kutta; energies accumulated with the same stages.
Result<TransientRecord> integrate(const Network& network, const TransientOptions& options, JobContext* context = nullptr);

// --- two coupled resonators ------------------------------------------------------------

struct EnergyExchange {
    double maximumTransferFraction = 0.0;  // max E_2 / E_total(0) after exciting resonator 1 alone
    double transferTime = 0.0;             // s, first maximum of E_2
    double remainingFraction = 0.0;        // E_1 / E_total(0) at that time
    double beatPeriod = 0.0;               // s, 2 pi / (w_a - w_s) from the modes
};
// Free vibration from resonator `from` displaced, energy exchange to `to`.
Result<EnergyExchange> energyExchange(const Network& network, int from, int to, double duration, JobContext* context = nullptr);

// Coupled-mode theory (the rotating-wave approximation) for two resonators of
// angular frequencies w1, w2, energy decay rates and a coupling rate kappa:
// eigenfrequencies (w1 + w2)/2 +- sqrt(((w1 - w2)/2)^2 + kappa^2) and the
// maximum transfer 4 kappa^2 / (4 kappa^2 + (w1 - w2)^2). An approximation, labelled as such.
struct CoupledModeEstimate {
    double lowerHz = 0.0, upperHz = 0.0;
    double splittingHz = 0.0;
    double maximumTransferFraction = 0.0;
    double couplingRate = 0.0;  // kappa, rad/s
    std::string model = "coupled-mode theory (rotating-wave approximation)";
};
// kappa for two masses m1, m2 on stiffnesses k1, k2 joined by kc: kc / (2 sqrt(m1 m2) sqrt(w1 w2)).
CoupledModeEstimate coupledModeEstimate(double m1, double k1, double m2, double k2, double kc);

}  // namespace resonant::oscillators
