// Sampling a field over a simulation region: the grid, the preflight
// estimate, the background job and the stored result.
#pragma once

#include "acoustics/FieldEvaluator.h"
#include "simulation/Job.h"
#include "simulation/SimulationResult.h"

#include <complex>

namespace resonant::acoustics {

// A regular grid of sample points in world coordinates: a rectangle (nw = 1)
// or a box. Sample (iu, iv, iw) is at
//   origin + iu/(nu-1) axisU + iv/(nv-1) axisV + iw/(nw-1) axisW.
struct GridFrame {
    Vec3 origin = Vec3::Zero();
    Vec3 axisU = Vec3::UnitX();
    Vec3 axisV = Vec3::UnitY();
    Vec3 axisW = Vec3::Zero();
    int nu = 2;
    int nv = 2;
    int nw = 1;

    bool isVolume() const { return nw > 1; }
    size_t pointCount() const { return static_cast<size_t>(nu) * static_cast<size_t>(nv) * static_cast<size_t>(nw); }
    size_t index(int iu, int iv, int iw) const
    {
        return static_cast<size_t>(iu) + static_cast<size_t>(nu) * (static_cast<size_t>(iv) + static_cast<size_t>(nv) * static_cast<size_t>(iw));
    }
    Vec3 point(int iu, int iv, int iw) const;
    // Largest distance between neighbouring samples along any sampled axis.
    double maxSpacing() const;
};

// The world-space grid of a region object.
GridFrame gridOf(const RegionSettings& region, const Mat4& regionWorld);

struct FieldEstimate {
    size_t points = 0;
    size_t bytes = 0;            // memory of the stored field
    double evaluations = 0.0;    // points * sources: the amount of work
    double spacing = 0.0;        // m, largest sample spacing
    double wavelength = 0.0;     // m
    double samplesPerWavelength = 0.0;
    bool allowed = true;         // false: refused, see `refusal`
    std::string refusal;
    std::vector<std::string> warnings;
};

inline constexpr size_t kDefaultFieldMemoryLimit = 1024ull * 1024ull * 1024ull;  // 1 GiB

// What computing a field would cost, before anything is allocated. A grid
// whose stored field would exceed `memoryLimitBytes` is refused.
FieldEstimate estimateField(const GridFrame& frame, size_t sourceCount, const Medium& medium, double frequencyHz,
                            size_t memoryLimitBytes = kDefaultFieldMemoryLimit);

struct FieldStatistics {
    double maxMagnitude = 0.0;       // Pa (peak), over samples that are not near a source
    Vec3 maxPoint = Vec3::Zero();    // world position of that sample
    double meanMagnitude = 0.0;      // Pa
    size_t nearSourceSamples = 0;    // samples excluded because they are inside a source's limit
};

// A complex pressure field sampled on a grid, with everything needed to
// know what it is and to reproduce it.
class FieldResult final : public SimulationResult {
public:
    std::string kind() const override { return "acoustic.field"; }

    GridFrame frame;
    std::vector<std::complex<float>> pressure;  // peak amplitude, Pa, indexed by GridFrame::index
    std::vector<std::uint8_t> nearSource;       // 1 where the sample is inside a source's limit
    double frequencyHz = 0.0;
    Medium medium;
    std::vector<PointSource> sources;           // the sources as they were when the run started
    size_t excludedSources = 0;                 // sources at another frequency, left out
    std::vector<std::string> assumptions;
    FieldStatistics statistics;
};

// Computes a FieldResult on worker threads. The evaluator is a snapshot:
// editing the scene while the job runs, or afterwards, does not affect it.
class FieldJob final : public Job {
public:
    FieldJob(std::string name, std::shared_ptr<const FieldEvaluator> evaluator, GridFrame frame,
             ResultMetadata metadata, unsigned threads = 0);

    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override;

    // The result once the job completed; nullptr before that or when it
    // failed or was cancelled.
    std::shared_ptr<const FieldResult> result() const;

private:
    std::string name_;
    std::shared_ptr<const FieldEvaluator> evaluator_;
    GridFrame frame_;
    ResultMetadata metadata_;
    unsigned threads_;
    mutable std::mutex mutex_;
    std::shared_ptr<const FieldResult> result_;
};

// Computes a field on the calling thread (used by tests and small tools).
std::shared_ptr<const FieldResult> computeField(const std::string& name, const FieldEvaluator& evaluator,
                                                const GridFrame& frame, ResultMetadata metadata = {});

// --- point reports ---------------------------------------------------------------

struct PointReport {
    Id objectId;
    std::string label;
    Vec3 position = Vec3::Zero();
    Complex pressure = Complex(0.0, 0.0);
    bool nearSource = false;
    double rms = 0.0;                 // Pa
    double splDb = 0.0;               // dB re referencePressure
    double referencePressure = 0.0;   // Pa
    Vec3 activeIntensity = Vec3::Zero();  // W/m^2
    bool intensityIsExact = true;     // false: directivity gradients were neglected
    bool intensityAvailable = true;   // false: the model provides no particle velocity; the intensity is not calculated
    std::optional<Complex> desired;   // for targets
};

// Field quantities at every probe (and, with includeTargets, every point
// target) of a scene.
std::vector<PointReport> reportPoints(const Scene& scene, const FieldEvaluator& evaluator, bool includeTargets);

// Machine-readable exports of point reports.
//   CSV: one header line, one line per point; columns are documented in
//        docs/acoustics/ACOUSTIC_ENGINE.md.
//   JSON: {"format": "resonant-point-report", "version": 1, "frequencyHz", "medium", "points": [...]}
std::string pointReportsToCsv(const std::vector<PointReport>& reports);
nlohmann::json pointReportsToJson(const std::vector<PointReport>& reports, const FieldEvaluator& evaluator);

}  // namespace resonant::acoustics
