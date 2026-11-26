#include "acoustics/FieldGrid.h"

#include "acoustics/Conventions.h"
#include "acoustics/InverseDesign.h"
#include "foundation/Time.h"
#include "foundation/Version.h"
#include "project/ProjectSerializer.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <thread>

namespace resonant::acoustics {

Vec3 GridFrame::point(int iu, int iv, int iw) const
{
    const auto fraction = [](int index, int count) { return count > 1 ? static_cast<double>(index) / (count - 1) : 0.0; };
    return origin + fraction(iu, nu) * axisU + fraction(iv, nv) * axisV + fraction(iw, nw) * axisW;
}

double GridFrame::maxSpacing() const
{
    double spacing = 0.0;
    if (nu > 1) spacing = std::max(spacing, axisU.norm() / (nu - 1));
    if (nv > 1) spacing = std::max(spacing, axisV.norm() / (nv - 1));
    if (nw > 1) spacing = std::max(spacing, axisW.norm() / (nw - 1));
    return spacing;
}

GridFrame gridOf(const RegionSettings& region, const Mat4& regionWorld)
{
    GridFrame frame;
    frame.origin = transformPoint(regionWorld, region.localOrigin());
    frame.axisU = transformDirection(regionWorld, region.localAxisU());
    frame.axisV = transformDirection(regionWorld, region.localAxisV());
    frame.axisW = transformDirection(regionWorld, region.localAxisW());
    frame.nu = region.countU;
    frame.nv = region.countV;
    frame.nw = region.isVolume() ? region.countW : 1;
    return frame;
}

FieldEstimate estimateField(const GridFrame& frame, size_t sourceCount, const Medium& medium, double frequencyHz,
                            size_t memoryLimitBytes)
{
    FieldEstimate estimate;
    estimate.points = frame.pointCount();
    estimate.bytes = estimate.points * (sizeof(std::complex<float>) + sizeof(std::uint8_t));
    estimate.evaluations = static_cast<double>(estimate.points) * static_cast<double>(sourceCount);
    estimate.spacing = frame.maxSpacing();
    estimate.wavelength = medium.wavelength(frequencyHz);
    estimate.samplesPerWavelength = estimate.spacing > 0.0 ? estimate.wavelength / estimate.spacing : 0.0;

    if (estimate.bytes > memoryLimitBytes) {
        estimate.allowed = false;
        std::ostringstream text;
        text << "The field would need " << std::fixed << std::setprecision(1) << estimate.bytes / (1024.0 * 1024.0)
             << " MiB for " << estimate.points << " samples, more than the limit of "
             << memoryLimitBytes / (1024.0 * 1024.0) << " MiB. Reduce the resolution or the region size.";
        estimate.refusal = text.str();
    }
    if (sourceCount == 0)
        estimate.warnings.push_back("There are no active sources at this frequency; the field would be zero.");
    if (estimate.samplesPerWavelength > 0.0 && estimate.samplesPerWavelength < 2.0)
        estimate.warnings.push_back("The sample spacing is larger than half a wavelength: the interference pattern "
                                    "will be aliased in the picture. The values at the sample points are still "
                                    "correct.");
    else if (estimate.samplesPerWavelength > 0.0 && estimate.samplesPerWavelength < 6.0)
        estimate.warnings.push_back("Fewer than 6 samples per wavelength: the picture of the field will look coarse.");
    if (estimate.samplesPerWavelength > 200.0)
        estimate.warnings.push_back("More than 200 samples per wavelength: this resolution adds cost without adding "
                                    "information.");
    if (estimate.evaluations > 2e9)
        estimate.warnings.push_back("This is a large calculation (more than 2 billion source evaluations) and may "
                                    "take minutes.");
    return estimate;
}

namespace {

// Fills rows [begin, end) of the slowest index (iw for a volume, iv for a plane).
void fillStatistics(FieldResult& result)
{
    FieldStatistics stats;
    double sum = 0.0;
    size_t counted = 0;
    for (int iw = 0; iw < result.frame.nw; ++iw) {
        for (int iv = 0; iv < result.frame.nv; ++iv) {
            for (int iu = 0; iu < result.frame.nu; ++iu) {
                const size_t index = result.frame.index(iu, iv, iw);
                if (result.nearSource[index]) {
                    ++stats.nearSourceSamples;
                    continue;
                }
                const double magnitude = std::abs(result.pressure[index]);
                sum += magnitude;
                ++counted;
                if (magnitude > stats.maxMagnitude) {
                    stats.maxMagnitude = magnitude;
                    stats.maxPoint = result.frame.point(iu, iv, iw);
                }
            }
        }
    }
    stats.meanMagnitude = counted ? sum / static_cast<double>(counted) : 0.0;
    result.statistics = stats;
}

std::shared_ptr<FieldResult> prepareResult(const std::string& name, const FieldEvaluator& evaluator,
                                           const GridFrame& frame, ResultMetadata metadata)
{
    auto result = std::make_shared<FieldResult>();
    result->id = Id::generate();
    result->name = name;
    result->frame = frame;
    result->frequencyHz = evaluator.frequencyHz();
    result->medium = evaluator.medium();
    result->sources = evaluator.sources();
    result->excludedSources = evaluator.excludedSourceCount();
    result->assumptions = evaluator.assumptions();
    if (result->excludedSources > 0)
        result->assumptions.push_back(std::to_string(result->excludedSources)
                                      + " active source(s) at a different frequency were left out of this "
                                        "single-frequency solution.");
    metadata.simulationId = result->id;
    metadata.domain = PhysicsDomain::Acoustic;
    metadata.solverId = evaluator.solverId();
    metadata.solverVersion = evaluator.solverVersion();
    metadata.softwareVersion = kApplicationVersion;
    metadata.startedUtc = utcNowIso8601();
    metadata.assumptions = result->assumptions;
    result->metadata = std::move(metadata);
    result->pressure.assign(frame.pointCount(), std::complex<float>(0.0f, 0.0f));
    result->nearSource.assign(frame.pointCount(), 0);
    return result;
}

void evaluateSlab(FieldResult& result, const FieldEvaluator& evaluator, int slab)
{
    const GridFrame& frame = result.frame;
    const bool volume = frame.isVolume();
    const int iw = volume ? slab : 0;
    const int ivBegin = volume ? 0 : slab;
    const int ivEnd = volume ? frame.nv : slab + 1;
    for (int iv = ivBegin; iv < ivEnd; ++iv) {
        for (int iu = 0; iu < frame.nu; ++iu) {
            const FieldSample sample = evaluator.sample(frame.point(iu, iv, iw));
            const size_t index = frame.index(iu, iv, iw);
            result.pressure[index] = std::complex<float>(static_cast<float>(sample.pressure.real()),
                                                         static_cast<float>(sample.pressure.imag()));
            result.nearSource[index] = sample.nearSource ? 1 : 0;
        }
    }
}

}  // namespace

FieldJob::FieldJob(std::string name, std::shared_ptr<const FieldEvaluator> evaluator, GridFrame frame,
                   ResultMetadata metadata, unsigned threads)
    : name_(std::move(name)), evaluator_(std::move(evaluator)), frame_(frame), metadata_(std::move(metadata)),
      threads_(threads)
{
}

JobEnd FieldJob::run(JobContext& context)
{
    const auto started = std::chrono::steady_clock::now();
    std::shared_ptr<FieldResult> result;
    try {
        result = prepareResult(name_, *evaluator_, frame_, metadata_);
    } catch (const std::bad_alloc&) {
        return JobEnd::failed("not enough memory for the field; reduce the resolution or the region size");
    }

    // Independent slabs (rows of a plane, layers of a volume) are evaluated
    // in parallel; each worker takes the next slab from a shared counter.
    const int slabs = frame_.isVolume() ? frame_.nw : frame_.nv;
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workerCount = std::min<unsigned>(threads_ ? threads_ : hardware, static_cast<unsigned>(slabs));
    std::atomic<int> next{0};
    std::atomic<int> done{0};
    std::atomic<bool> stop{false};

    const auto work = [&] {
        for (;;) {
            if (stop.load())
                return;
            const int slab = next.fetch_add(1);
            if (slab >= slabs)
                return;
            evaluateSlab(*result, *evaluator_, slab);
            done.fetch_add(1);
        }
    };
    std::vector<std::thread> workers;
    for (unsigned i = 1; i < workerCount; ++i)
        workers.emplace_back(work);

    // This thread works too, and reports progress between slabs.
    int reported = -1;
    for (;;) {
        if (context.cancelRequested())
            stop.store(true);
        if (stop.load())
            break;
        const int slab = next.fetch_add(1);
        if (slab >= slabs)
            break;
        evaluateSlab(*result, *evaluator_, slab);
        const int finished = done.fetch_add(1) + 1;
        if (finished * 100 / slabs != reported) {
            reported = finished * 100 / slabs;
            context.reportProgress(static_cast<double>(finished) / slabs,
                                   std::to_string(finished) + " of " + std::to_string(slabs)
                                       + (frame_.isVolume() ? " layers" : " rows"));
        }
    }
    for (std::thread& worker : workers)
        worker.join();
    if (stop.load() || context.cancelRequested())
        return JobEnd::cancelled();

    fillStatistics(*result);
    result->metadata.elapsedSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    {
        std::lock_guard lock(mutex_);
        result_ = result;
    }
    std::ostringstream summary;
    summary << frame_.pointCount() << " samples, " << evaluator_->sources().size() << " source(s), peak "
            << result->statistics.maxMagnitude << " Pa";
    return JobEnd::completed(summary.str());
}

std::shared_ptr<const FieldResult> FieldJob::result() const
{
    std::lock_guard lock(mutex_);
    return result_;
}

std::shared_ptr<const FieldResult> computeField(const std::string& name, const FieldEvaluator& evaluator,
                                                const GridFrame& frame, ResultMetadata metadata)
{
    const auto started = std::chrono::steady_clock::now();
    auto result = prepareResult(name, evaluator, frame, std::move(metadata));
    const int slabs = frame.isVolume() ? frame.nw : frame.nv;
    for (int slab = 0; slab < slabs; ++slab)
        evaluateSlab(*result, evaluator, slab);
    fillStatistics(*result);
    result->metadata.elapsedSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return result;
}

std::vector<PointReport> reportPoints(const Scene& scene, const FieldEvaluator& evaluator, bool includeTargets)
{
    std::vector<PointReport> reports;
    const double reference = evaluator.medium().name == "Water" ? kReferencePressureWater : kReferencePressureAir;
    scene.forEach([&](const SceneObject& object) {
        const bool probe = componentOf<ProbeComponent>(object) != nullptr;
        const auto* target = componentOf<TargetComponent>(object);
        if (!probe && !(includeTargets && target && target->settings.kind == TargetKind::Point))
            return;
        PointReport report;
        report.objectId = object.id();
        report.label = object.state.name;
        report.position = transformPoint(scene.worldMatrix(object), Vec3::Zero());
        const FieldSample sample = evaluator.sample(report.position);
        report.pressure = sample.pressure;
        report.nearSource = sample.nearSource;
        report.rms = rmsPressure(sample.pressure);
        report.referencePressure = reference;
        report.splDb = soundPressureLevel(sample.pressure, reference);
        report.intensityAvailable = evaluator.providesVelocity();
        report.activeIntensity = evaluator.activeIntensity(report.position);
        report.intensityIsExact = evaluator.velocityIsExact();
        if (target)
            report.desired = target->settings.suppress
                                 ? Complex(0.0, 0.0)
                                 : std::polar(target->settings.desiredAmplitudePa, target->settings.desiredPhaseRad);
        reports.push_back(std::move(report));
    });
    return reports;
}

std::string pointReportsToCsv(const std::vector<PointReport>& reports)
{
    std::ostringstream out;
    out << std::setprecision(12);
    out << "name,x_m,y_m,z_m,pressure_real_Pa,pressure_imag_Pa,magnitude_Pa_peak,phase_rad,rms_Pa,spl_dB,"
           "spl_reference_Pa,intensity_x_W_m2,intensity_y_W_m2,intensity_z_W_m2,intensity_exact,near_source,"
           "intensity_available\n";
    for (const PointReport& r : reports) {
        std::string name = r.label;
        for (char& c : name)
            if (c == ',' || c == '\n' || c == '"')
                c = ' ';
        out << name << ',' << r.position.x() << ',' << r.position.y() << ',' << r.position.z() << ','
            << r.pressure.real() << ',' << r.pressure.imag() << ',' << std::abs(r.pressure) << ','
            << std::arg(r.pressure) << ',' << r.rms << ',' << r.splDb << ',' << r.referencePressure << ','
            << r.activeIntensity.x() << ',' << r.activeIntensity.y() << ',' << r.activeIntensity.z() << ','
            << (r.intensityIsExact ? 1 : 0) << ',' << (r.nearSource ? 1 : 0) << ',' << (r.intensityAvailable ? 1 : 0) << '\n';
    }
    return out.str();
}

nlohmann::json pointReportsToJson(const std::vector<PointReport>& reports, const FieldEvaluator& evaluator)
{
    nlohmann::json points = nlohmann::json::array();
    for (const PointReport& r : reports) {
        nlohmann::json point = {{"id", r.objectId.str()},
                                {"name", r.label},
                                {"position", resonant::toJson(r.position)},
                                {"pressure", {r.pressure.real(), r.pressure.imag()}},
                                {"magnitudePa", std::abs(r.pressure)},
                                {"phaseRad", std::arg(r.pressure)},
                                {"rmsPa", r.rms},
                                {"splDb", r.splDb},
                                {"splReferencePa", r.referencePressure},
                                {"activeIntensity", resonant::toJson(r.activeIntensity)},
                                {"intensityIsExact", r.intensityIsExact},
                                {"intensityAvailable", r.intensityAvailable},
                                {"nearSource", r.nearSource}};
        if (r.desired)
            point["desiredPressure"] = {r.desired->real(), r.desired->imag()};
        points.push_back(std::move(point));
    }
    return {{"format", "resonant-point-report"},
            {"version", 1},
            {"frequencyHz", evaluator.frequencyHz()},
            {"medium", toJson(evaluator.medium())},
            {"model", evaluator.solverId()},
            {"timeConvention", kTimeConvention},
            {"amplitudes", "peak"},
            {"points", std::move(points)}};
}

}  // namespace resonant::acoustics
