#pragma once

// From a project to a numerical problem.
//
// The region object selected for the run defines the computational domain:
// a plane region gives a two-dimensional calculation in that plane, a volume
// region a three-dimensional one. Emitters become volume-velocity sources,
// probes and targets become recorded positions, and objects with geometry
// become rigid, pressure-release or fluid-like material cells, from their
// own definitions (geometry/SolidModel.h).
//
// Nothing here reduces the accuracy that was asked for: when the request
// cannot be met within the limits, the plan is refused with the reason and
// what to change.
#include "acoustics/FieldGrid.h"
#include "fdtd/FdtdRun.h"
#include "dsp/Spectrum.h"
#include "project/Project.h"
#include "solvers/SolverRegistry.h"

namespace resonant::fdtd {

using dsp::Spectrum;
using dsp::Window;
using dsp::SpectralPeak;
using dsp::computeSpectrum;
using dsp::findPeaks;

inline constexpr const char* kFdtdSolverId = "acoustic.fdtd.staggered";
inline constexpr const char* kFdtdSolverVersion = "1";
inline constexpr const char* kFdtdSettingsKey = "fdtd";

struct FdtdSettings {
    // Cells per wavelength of the highest frequency of the signal in the
    // slowest medium of the domain.
    double pointsPerWavelength = 12.0;
    // Time step as a fraction of the stability limit, 0 < fraction <= 1.
    double courantFraction = 0.95;
    // The signal every emitter radiates. For the continuous, tone-burst and
    // Gaussian kinds the carrier frequency and phase are each emitter's own;
    // a Ricker wavelet, a sweep and a sampled signal are used as defined.
    // Each emitter's amplitude and time delay always apply.
    Waveform waveform;
    // Simulated time, s. 0 = automatic (see FdtdPlan::durationBasis).
    double duration = 0.0;
    std::array<BoundaryKind, 6> boundary{BoundaryKind::Absorbing, BoundaryKind::Absorbing, BoundaryKind::Absorbing,
                                         BoundaryKind::Absorbing, BoundaryKind::Absorbing, BoundaryKind::Absorbing};
    double boundaryImpedance = 415.0;  // Pa s/m, for impedance faces
    int absorbingCells = 12;
    int snapshotCount = 40;        // 0 = none
    double harmonicPeriods = 8.0;  // length of the steady-state analysis window (continuous signals)
    size_t memoryLimitBytes = 1024ull * 1024ull * 1024ull;

    Status validate() const;
    friend bool operator==(const FdtdSettings&, const FdtdSettings&) = default;
};

nlohmann::json toJson(const FdtdSettings& settings);
Result<FdtdSettings> fdtdSettingsFromJson(const nlohmann::json& value);
FdtdSettings fdtdSettingsOf(const Project& project);
void storeFdtdSettings(Project& project, const FdtdSettings& settings);

// What a run will do and cost, worked out before it starts.
struct FdtdPlan {
    bool allowed = true;
    std::string refusal;                // why not, and what to change
    std::vector<std::string> warnings;  // the run is possible but its result needs care
    std::vector<std::string> assumptions;  // what the calculation represents and what it leaves out

    int dimensions = 2;
    double dx = 0.0, dt = 0.0;
    double stabilityLimit = 0.0;        // s, the largest stable time step
    double courantNumber = 0.0;         // c_max dt / dx
    int nx = 0, ny = 0, nz = 1;         // cells of the region
    std::array<int, 3> totalCells{0, 0, 1};  // including absorbing layers
    size_t cellCount = 0;
    long steps = 0;
    double duration = 0.0;
    std::string durationBasis;
    double highestFrequency = 0.0;      // Hz, what the grid has to resolve
    double analysisFrequency = 0.0;     // Hz, of the steady-state field; 0 = none
    double slowestSpeed = 0.0, fastestSpeed = 0.0;
    double shortestWavelength = 0.0;
    double pointsPerWavelength = 0.0;   // actual
    double phaseVelocityError = 0.0;    // fraction, worst direction, at the highest frequency
    size_t stateBytes = 0, recordingBytes = 0;
    double cellUpdates = 0.0;           // cells * steps
    double estimatedSeconds = 0.0;      // rough: cellUpdates / kAssumedUpdateRate
    size_t sourceCount = 0, excludedSources = 0;
    size_t probeCount = 0;
    size_t rigidCells = 0, pressureReleaseCells = 0, materialCells = 0;
    std::vector<std::string> objects;   // how each object of the scene is represented

    acoustics::GridFrame frame;         // cell centres of the region
    std::string summary() const;
};

// Cell updates per second assumed for the time estimate: a single core of a
// desktop processor running the reference implementation. The estimate is
// rough (within a factor of about three).
inline constexpr double kAssumedUpdateRate = 6e7;

struct Prepared {
    FdtdPlan plan;
    Problem problem;                         // empty grid when the plan is refused
    std::vector<std::uint8_t> materialMap;   // copy of the interior material indices
    std::vector<std::string> materialNames;
    std::vector<acoustics::PointSource> sources;  // the emitters that are simulated, as they were
};

// Plans a run of `regionId` and, when it is allowed, builds the grid.
// `medium` is the project's propagation medium.
Prepared prepare(const Project& project, const Id& regionId, const acoustics::Medium& medium, const FdtdSettings& settings);

// What the problem contains, for the solver assessment
// (solvers::SolverRegistry::assess). `plan` may be a refused plan.
solvers::ProblemTraits traitsOf(const Project& project, const Id& regionId, const acoustics::Medium& medium,
                                const FdtdSettings& settings, const FdtdPlan& plan);

// The result of a run.
class FdtdResult final : public SimulationResult {
public:
    std::string kind() const override { return "acoustic.fdtd"; }

    FdtdPlan plan;
    FdtdSettings settings;
    acoustics::Medium medium;
    RunOutput output;
    std::vector<std::uint8_t> materialMap;
    std::vector<std::string> materialNames;
    // The steady-state complex field at plan.analysisFrequency, for
    // continuous signals; null otherwise.
    std::shared_ptr<const acoustics::FieldResult> harmonicField;

    // Snapshot `index` as a field whose real part is the instantaneous
    // pressure at that time (the imaginary part is zero).
    std::shared_ptr<const acoustics::FieldResult> snapshotField(size_t index) const;
    // Spectrum of a probe's recorded pressure.
    Spectrum probeSpectrum(size_t probe, Window window = Window::Hann) const;
    // Energy at the end over the largest energy during the run.
    double finalEnergyFraction() const;
};

class FdtdJob final : public Job {
public:
    FdtdJob(std::string name, Prepared prepared, FdtdSettings settings, acoustics::Medium medium, ResultMetadata metadata);

    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override;
    std::shared_ptr<const FdtdResult> result() const;

private:
    std::string name_;
    Prepared prepared_;
    FdtdSettings settings_;
    acoustics::Medium medium_;
    ResultMetadata metadata_;
    mutable std::mutex mutex_;
    std::shared_ptr<const FdtdResult> result_;
};

// Runs synchronously (tests, scripts).
Result<std::shared_ptr<const FdtdResult>> simulate(const std::string& name, Prepared prepared, const FdtdSettings& settings,
                                                   const acoustics::Medium& medium, ResultMetadata metadata = {},
                                                   JobContext* context = nullptr);

}  // namespace resonant::fdtd
