#pragma once

// The Yee solver driven from a project.
//
// The domain is a box given by the caller (the application takes it from a
// field region): a plane gives a two-dimensional TM calculation (currents
// and fields along the plane's normal), a volume a three-dimensional one.
// Represented: dipole and monopole antennas (feed current on one edge, arms
// and ground plane as thin perfect-conductor edges), electric-dipole sources
// (a one-cell current element), and objects with geometry (good conductors
// as perfect-conductor cells, other materials with their permittivity,
// permeability and conductivity). Left out and reported: loops, patches,
// horns, helices, magnetic dipoles, plane waves and arrays.
#include "electromagnetics/EmStudy.h"
#include "electromagnetics/Yee.h"

namespace resonant::em {

inline constexpr const char* kEmFullWaveKey = "electromagneticFullWave";
inline constexpr const char* kEmFullWaveSolverId = "em.fdtd.yee";

struct DomainBox {
    Vec3 corner = Vec3::Zero();  // world position of the (0, 0, 0) corner
    std::array<Vec3, 3> axis{Vec3::UnitX(), Vec3::UnitY(), Vec3::UnitZ()};  // unit vectors
    Vec3 size = Vec3(0.3, 0.3, 0.3);  // m along the axes
    bool volume = true;               // false: a plane spanned by axis 0 and 1
};

struct EmFullWaveSettings {
    double cellsPerWavelength = 20.0;   // at the run frequency in the slowest material
    double courantFraction = 0.95;
    dsp::Waveform waveform;             // Continuous / ToneBurst / GaussianPulse take each radiator's frequency
    double duration = 0.0;              // s; 0 = automatic
    std::array<YeeBoundary, 6> boundary{YeeBoundary::Absorbing, YeeBoundary::Absorbing, YeeBoundary::Absorbing,
                                        YeeBoundary::Absorbing, YeeBoundary::Absorbing, YeeBoundary::Absorbing};
    int absorbingCells = 10;
    double harmonicPeriods = 8.0;
    int snapshotCount = 20;
    size_t memoryLimitBytes = 1024ull * 1024ull * 1024ull;

    Status validate() const;
    friend bool operator==(const EmFullWaveSettings&, const EmFullWaveSettings&) = default;
};
nlohmann::json toJson(const EmFullWaveSettings& settings);
Result<EmFullWaveSettings> emFullWaveSettingsFromJson(const nlohmann::json& value);
EmFullWaveSettings emFullWaveSettingsOf(const Project& project);
void storeEmFullWaveSettings(Project& project, const EmFullWaveSettings& settings);

struct EmFullWavePlan {
    bool allowed = true;
    std::string refusal;
    std::vector<std::string> warnings, assumptions, objects;
    int dimensions = 3;
    double frequencyHz = 0.0;
    double cell = 0.0, dt = 0.0, stabilityLimit = 0.0;
    int nx = 0, ny = 0, nz = 1;
    long steps = 0;
    double duration = 0.0;
    std::string durationBasis;
    double cellsPerWavelength = 0.0;
    size_t stateBytes = 0;
    double cellUpdates = 0.0, estimatedSeconds = 0.0;
    size_t sourceCount = 0, pecCells = 0, materialCells = 0, wireEdges = 0;
    SampleGrid grid;  // the region's nodes
    std::string summary() const;
};

struct EmPrepared {
    EmFullWavePlan plan;
    YeeSpec spec;
    std::vector<YeeSource> sources;
};

EmPrepared prepareFullWave(const Project& project, const DomainBox& domain, const EmFullWaveSettings& settings);

class EmFullWaveResult final : public SimulationResult {
public:
    std::string kind() const override { return "em.fullwave"; }
    EmFullWavePlan plan;
    EmFullWaveSettings settings;
    YeeOutput output;
    std::shared_ptr<EmFieldResult> harmonicField;  // E at the region nodes (no H), continuous signals only
};

Result<std::shared_ptr<const EmFullWaveResult>> simulateFullWave(const std::string& name, EmPrepared prepared, const EmFullWaveSettings& settings,
                                                                 ResultMetadata metadata = {}, JobContext* context = nullptr);

class EmFullWaveJob final : public Job {
public:
    EmFullWaveJob(std::string name, EmPrepared prepared, EmFullWaveSettings settings, ResultMetadata metadata)
        : name_(std::move(name)), prepared_(std::move(prepared)), settings_(std::move(settings)), metadata_(std::move(metadata)) {}
    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override;
    std::shared_ptr<const EmFullWaveResult> result() const;

private:
    std::string name_;
    EmPrepared prepared_;
    EmFullWaveSettings settings_;
    ResultMetadata metadata_;
    mutable std::mutex mutex_;
    std::shared_ptr<const EmFullWaveResult> result_;
};

}  // namespace resonant::em
