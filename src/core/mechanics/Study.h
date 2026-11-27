#pragma once

// From a project to structural analyses.
//
// A study is everything a structural calculation needs for one structure
// object, collected and checked before anything runs: the idealization and
// its mesh, the material, the supports, the actuators, sensors and targets
// attached to it, the warnings, and the assumptions every result will carry.
#include "mechanics/Analyses.h"
#include "mechanics/MechanicalComponents.h"
#include "project/Project.h"
#include "simulation/SimulationResult.h"

#include <memory>

namespace resonant::mechanics {

inline constexpr const char* kMechanicsSettingsKey = "mechanics";
inline constexpr const char* kMechanicsSolverVersion = "1";

struct MechanicsSettings {
    int modeCount = 12;
    double frequencyMin = 10.0;     // Hz, harmonic sweep
    double frequencyMax = 2000.0;   // Hz
    int frequencyPoints = 400;
    bool logSpacing = true;
    HarmonicMethod harmonicMethod = HarmonicMethod::Direct;
    int harmonicModes = 30;
    Damping damping{DampingKind::LossFactor, 0.0, 0.0, 0.01, true, 0.0};  // frequency domain
    double transientDuration = 0.02;  // s
    double stepsPerPeriod = 20.0;     // time steps per period of the highest frequency of the forces
    double timeStep = 0.0;            // s; 0 = from stepsPerPeriod
    Damping transientDamping;         // None or Rayleigh
    int snapshotCount = 60;
    double targetFrequency = 200.0;   // Hz
    double regularization = 1e-3;
    size_t memoryLimitBytes = 1024ull * 1024ull * 1024ull;

    Status validate() const;
    std::vector<double> frequencies() const;
    friend bool operator==(const MechanicsSettings& a, const MechanicsSettings& b);
};

nlohmann::json toJson(const MechanicsSettings& settings);
Result<MechanicsSettings> mechanicsSettingsFromJson(const nlohmann::json& value);
MechanicsSettings mechanicsSettingsOf(const Project& project);
void storeMechanicsSettings(Project& project, const MechanicsSettings& settings);

struct TargetPoint {
    Id objectId;
    std::string label;
    ResponsePoint point;
    Complex desired = Complex(0.0, 0.0);
    double weight = 1.0;
    bool suppress = false;
};

struct StructureStudy {
    bool allowed = true;
    std::string refusal;
    std::vector<std::string> warnings;
    std::vector<std::string> notes;        // how the object is represented
    std::vector<std::string> assumptions;  // what every result of this study assumes
    std::vector<std::pair<std::string, std::string>> properties;  // geometric and material properties for display

    Id objectId;
    std::string objectName;
    StructureSettings settings;
    std::shared_ptr<const StructuralModel> model;
    std::array<int, 3> grid{0, 0, 0};  // elements along each mesh direction (beam: n,0,0)
    double elementSize = 0.0;
    double characteristicThickness = 0.0;  // beam: largest section dimension; plate: thickness
    size_t freeDofs = 0;
    size_t estimatedBytes = 0;
    std::vector<ForceInput> forces;
    std::vector<ResponsePoint> sensors;
    std::vector<TargetPoint> targets;
    Mat4 world = Mat4::Identity();

    std::string summary() const;
    // Elements per wavelength of the structure's own wave (bending for
    // beams and plates, transverse for membranes, shear for solids) at a
    // frequency; the mesh must resolve what an analysis asks for.
    double elementsPerWavelength(double frequencyHz) const;
    std::string waveDescription() const;
};

// The objects of a project that carry a structure component.
std::vector<Id> structureObjects(const Project& project);
StructureStudy prepareStudy(const Project& project, const Id& structureId, size_t memoryLimitBytes = 1024ull * 1024ull * 1024ull);

// --- results ----------------------------------------------------------------------------

enum class Analysis { Static, Modal, Harmonic, Transient };
std::string_view toString(Analysis analysis);

class StructuralResult final : public SimulationResult {
public:
    std::string kind() const override { return "mechanical." + std::string(mechanics::toString(analysis)); }

    Analysis analysis = Analysis::Modal;
    std::shared_ptr<const StructureStudy> study;
    std::optional<StaticSolution> statics;
    std::vector<ElementStress> stresses;  // Static: per element (empty when the model has none)
    std::optional<ModalSolution> modal;
    std::optional<HarmonicSolution> harmonic;
    std::optional<TransientSolution> transient;
    std::vector<std::string> warnings;
};

// Analyses of a prepared study. Each returns a result with complete
// reproducibility metadata, or the reason it could not run.
using StructuralOutcome = Result<std::shared_ptr<const StructuralResult>>;
StructuralOutcome runStatic(std::shared_ptr<const StructureStudy> study, JobContext* context = nullptr);
StructuralOutcome runModal(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context = nullptr);
StructuralOutcome runHarmonic(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context = nullptr);
StructuralOutcome runTransient(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context = nullptr);

// The time step a transient analysis will use, and why.
double transientTimeStep(const StructureStudy& study, const MechanicsSettings& settings, std::string* basis = nullptr);

class StructuralJob final : public Job {
public:
    using Work = std::function<StructuralOutcome(JobContext&)>;
    StructuralJob(std::string name, Work work) : name_(std::move(name)), work_(std::move(work)) {}
    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override;
    std::shared_ptr<const StructuralResult> result() const;

private:
    std::string name_;
    Work work_;
    mutable std::mutex mutex_;
    std::shared_ptr<const StructuralResult> result_;
};

}  // namespace resonant::mechanics
