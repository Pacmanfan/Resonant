#pragma once

// Structural analyses on a StructuralModel: static, modal, harmonic
// (frequency response) and transient (time integration).
//
// Time convention for harmonic quantities: e^{+j omega t}, peak amplitudes.
// Displacement in m, velocity j omega u, acceleration -omega^2 u.
#include "mechanics/Eigensolver.h"
#include "mechanics/StructuralModel.h"
#include "dsp/Waveform.h"
#include "simulation/Job.h"

#include <complex>
#include <optional>

namespace resonant::mechanics {

using Complex = std::complex<double>;

// A force applied to the structure: `pattern` (full dofs) is the load for a
// force of 1 N; the force is amplitude * w(t - delay) in the time domain and
// amplitude * e^{j phase} at every frequency in the frequency domain.
struct ForceInput {
    Id objectId;
    std::string label;
    Eigen::VectorXd pattern;
    double amplitude = 1.0;  // N, peak
    double phase = 0.0;      // rad
    double delay = 0.0;      // s; a real time shift in transient analysis, exp(-j omega delay) in harmonic analysis
    dsp::Waveform waveform;
    Vec3 position = Vec3::Zero();  // where it acts (node position)
    Vec3 direction = Vec3::UnitZ();

    Complex phasor(double omega) const { return std::polar(amplitude, phase - omega * delay); }
};

// A response quantity: selector^T u, the displacement of a node along a
// world direction.
struct ResponsePoint {
    Id objectId;
    std::string label;
    Eigen::VectorXd selector;
    Vec3 position = Vec3::Zero();
    Vec3 direction = Vec3::UnitZ();
    size_t node = 0;
};

enum class DampingKind {
    None,
    Rayleigh,    // C = alpha M + beta K
    LossFactor,  // complex stiffness K (1 + j eta): frequency domain only
    ModalRatio   // the same damping ratio zeta in every mode: modal superposition only
};
std::string_view toString(DampingKind kind);

struct Damping {
    DampingKind kind = DampingKind::None;
    double alpha = 0.0;  // 1/s
    double beta = 0.0;   // s
    double eta = 0.0;    // loss factor, for elements whose material states none
    bool materialLossFactors = true;  // LossFactor: use each material's own loss factor where it states one
    double zeta = 0.0;   // ModalRatio
    // The damping ratio of a mode at angular frequency omega (for records and
    // modal superposition): Rayleigh alpha / (2 omega) + beta omega / 2;
    // loss factor eta / 2 (equivalent at resonance); modal zeta.
    double ratioAt(double omega) const;
    std::string describe() const;
    Status validate() const;
};

// --- static ----------------------------------------------------------------------------

struct StaticSolution {
    Eigen::VectorXd displacement;  // full dofs
    double compliance = 0.0;       // f^T u, J
    double maxTranslation = 0.0;   // m
};
Result<StaticSolution> solveStatic(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& forces);

// --- modal -----------------------------------------------------------------------------

struct Mode {
    double frequencyHz = 0.0;
    double omega = 0.0;
    Eigen::VectorXd shape;  // full dofs, mass-normalized (phi^T M phi = 1): the amplitude is arbitrary
    double residual = 0.0;
    Vec3 effectiveMassFraction = Vec3::Zero();  // per world direction (translational participation)
};

struct ModalSolution {
    std::vector<Mode> modes;
    std::string method;
    Vec3 totalMass = Vec3::Zero();         // of the free dofs, per direction
    Vec3 cumulativeMassFraction = Vec3::Zero();
};
Result<ModalSolution> solveModal(const StructuralModel& model, const SystemMatrices& system, int count, JobContext* context = nullptr,
                                 bool forceIterative = false);

// --- harmonic ----------------------------------------------------------------------------

enum class HarmonicMethod {
    Direct,  // the dynamic stiffness K - omega^2 M + j omega C (+ j K_eta) factorized at every frequency: exact for the model
    Modal    // superposition of the lowest modes: truncated
};

struct HarmonicSettings {
    std::vector<double> frequenciesHz;
    Damping damping;
    HarmonicMethod method = HarmonicMethod::Direct;
    int modeCount = 30;           // Modal
    bool keepFields = true;       // keep the full complex displacement at every frequency (if within the limit)
    size_t fieldMemoryLimit = 256u * 1024u * 1024u;
};

struct HarmonicSolution {
    std::vector<double> frequenciesHz;
    // [response point][frequency]: complex displacement, m
    std::vector<std::vector<Complex>> responses;
    std::vector<Eigen::VectorXcd> fields;  // full dofs per frequency, or empty
    std::string fieldsNote;
    std::vector<double> modeFrequenciesHz;  // Modal: the modes used
    std::string method;
};
Result<HarmonicSolution> solveHarmonic(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& forces,
                                       const std::vector<ResponsePoint>& points, const HarmonicSettings& settings,
                                       JobContext* context = nullptr);

// Dynamic stiffness at one angular frequency (free dofs), for reuse.
Eigen::SparseMatrix<Complex> dynamicStiffness(const SystemMatrices& system, double omega, const Damping& damping);

// --- transient -----------------------------------------------------------------------------

struct TransientSettings {
    double duration = 0.01;   // s
    double timeStep = 1e-5;   // s
    Damping damping;          // None or Rayleigh
    int snapshotCount = 40;
    int energySamples = 400;
};

struct TransientSolution {
    double dt = 0.0;
    long steps = 0;
    std::vector<double> time;  // s, time[n] = n dt, n = 0..steps
    // [response point][step]
    std::vector<std::vector<double>> displacement, velocity, acceleration;
    std::vector<double> snapshotTimes;
    std::vector<Eigen::VectorXd> snapshots;  // full displacement vectors
    std::vector<double> energyTimes, kinetic, strain, externalWork, dissipated;
    double elapsedSeconds = 0.0;
};
Result<TransientSolution> solveTransient(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& forces,
                                         const std::vector<ResponsePoint>& points, const TransientSettings& settings,
                                         JobContext* context = nullptr, const Eigen::VectorXd* initialDisplacement = nullptr);

// Period elongation of the average-acceleration Newmark method for a mode
// at angular frequency omega: (T_numerical - T) / T, from its exact
// discrete relation omega_numerical dt = 2 atan(omega dt / 2).
double newmarkPeriodError(double omega, double dt);

// The response selector of a node along a world direction.
ResponsePoint responseAt(const StructuralModel& model, size_t node, const Vec3& direction, std::string label = {});
// A force pattern: 1 N at a node along a world direction (its components
// along the node's translational dofs; a component without a dof is lost
// and reported by `lostFraction`).
Eigen::VectorXd forcePattern(const StructuralModel& model, size_t node, const Vec3& direction, double* lostFraction = nullptr);
// 1 N spread evenly over the nodes within `radius` of a point (at least the nearest node).
Eigen::VectorXd distributedPattern(const StructuralModel& model, const Vec3& point, double radius, const Vec3& direction,
                                   double* lostFraction = nullptr, size_t* nodeCount = nullptr);
size_t nearestNode(const StructuralModel& model, const Vec3& point, double* distance = nullptr);

}  // namespace resonant::mechanics
