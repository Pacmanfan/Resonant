#include "experiments/ExperimentArchive.h"

#include "experiments/Processing.h"
#include "foundation/Time.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

namespace fs = std::filesystem;

namespace resonant::experiments {

namespace {

// A store file (HDF5) or directory (binary format), by its extension.
std::optional<StoreFormat> formatOfRef(const std::string& ref)
{
    if (ref.size() > 3 && ref.substr(ref.size() - 3) == ".h5")
        return StoreFormat::Hdf5;
    if (ref.size() > 4 && ref.substr(ref.size() - 4) == ".rmd")
        return StoreFormat::Binary;
    return std::nullopt;
}

double maxDifference(const MeasurementData& a, const MeasurementData& b, double& scale)
{
    double d = 0.0;
    scale = 0.0;
    if (a.real.size() != b.real.size() || a.complex.size() != b.complex.size() || a.axis.size() != b.axis.size())
        return std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < a.real.size(); ++i) {
        d = std::max(d, std::abs(a.real[i] - b.real[i]));
        scale = std::max(scale, std::abs(b.real[i]));
    }
    for (size_t i = 0; i < a.complex.size(); ++i) {
        d = std::max(d, std::abs(a.complex[i] - b.complex[i]));
        scale = std::max(scale, std::abs(b.complex[i]));
    }
    for (size_t i = 0; i < a.axis.size(); ++i)
        d = std::max(d, std::abs(a.axis[i] - b.axis[i]));
    return d;
}

}  // namespace

Result<ArchiveReport> exportExperiment(const ExperimentWorkspace& w, const std::string& experimentId, const MeasurementStore& store,
                                       const fs::path& directory)
{
    using R = Result<ArchiveReport>;
    const Experiment* e = w.experiment(experimentId);
    if (!e)
        return R::failure("no experiment '" + experimentId + "'");
    std::error_code ec;
    if (fs::exists(directory, ec))
        return R::failure(directory.string() + " exists already; an archive is written to a new directory");
    fs::create_directories(directory / "data", ec);
    if (ec)
        return R::failure("cannot create " + directory.string() + ": " + ec.message());
    ExperimentWorkspace part;
    part.experiments.push_back(*e);
    // Calibrations referenced by the experiment or its records, with their history.
    std::set<std::string> calibrationIds;
    for (const auto& s : e->sensors)
        calibrationIds.insert(s.calibrationId);
    for (const auto& a : e->actuators)
        calibrationIds.insert(a.calibrationId);
    for (const auto& b : e->channels)
        calibrationIds.insert(b.calibrationId);
    for (const auto& id : e->synchronization.timingCalibrationIds)
        calibrationIds.insert(id);
    ArchiveReport report;
    report.directory = directory;
    for (const auto* m : w.measurementsOf(experimentId)) {
        part.measurements.push_back(*m);
        calibrationIds.insert(m->calibrationId);
        for (const auto& step : m->history)
            if (step.operation == "calibrate")
                calibrationIds.insert(step.parameters.value("calibrationId", std::string()));
        if (m->dataRef.empty())
            continue;
        const fs::path from = store.root() / m->dataRef;
        const fs::path to = directory / "data" / m->dataRef;
        if (!fs::exists(from, ec)) {
            report.missing.push_back(m->id);
            continue;
        }
        fs::create_directories(to.parent_path(), ec);
        fs::copy(from, to, fs::copy_options::recursive, ec);
        if (ec)
            return R::failure("copying " + m->dataRef + ": " + ec.message());
        ++report.dataSets;
    }
    calibrationIds.erase("");
    std::set<std::string> added;
    for (const auto& id : calibrationIds)
        for (const auto* c : w.calibrations.history(id))
            if (added.insert(c->id).second)
                (void)part.calibrations.add(*c);
    for (const auto& s : w.spatial)
        if (s.experimentId == experimentId)
            part.spatial.push_back(s);
    for (const auto& c : w.comparisons)
        if (c.experimentId == experimentId)
            part.comparisons.push_back(c);
    nlohmann::json document = {{"format", kArchiveFormat},
                               {"version", kArchiveVersion},
                               {"exportedUtc", utcNowIso8601()},
                               {"storeFormat", toString(store.format())},
                               {"experiments", toJson(part)}};
    std::ofstream(directory / "experiment.json") << document.dump(1);
    return report;
}

Result<ImportedArchive> importExperiment(const fs::path& directory)
{
    using R = Result<ImportedArchive>;
    std::ifstream in(directory / "experiment.json");
    if (!in)
        return R::failure(directory.string() + " is not an experiment archive (no experiment.json)");
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(in);
    } catch (const std::exception& ex) {
        return R::failure("experiment.json is not valid JSON: " + std::string(ex.what()));
    }
    if (document.value("format", std::string()) != kArchiveFormat)
        return R::failure("experiment.json is not a Resonant experiment archive");
    if (document.value("version", 0) > kArchiveVersion)
        return R::failure("the archive was written by a newer Resonant");
    auto workspace = experimentWorkspaceFromJson(document.value("experiments", nlohmann::json::object()));
    if (!workspace)
        return R::failure(workspace.message());
    ImportedArchive a;
    a.workspace = workspace.take();
    a.directory = directory;
    const std::string format = document.value("storeFormat", std::string("hdf5"));
    auto store = openMeasurementStore(directory / "data", format == "binary" ? StoreFormat::Binary : StoreFormat::Hdf5);
    if (!store)
        return R::failure(store.message());
    a.store = store.take();
    return a;
}

Status mergeArchive(ExperimentWorkspace& target, MeasurementStore& targetStore, const ImportedArchive& archive)
{
    for (const auto& e : archive.workspace.experiments)
        if (target.experiment(e.id))
            return Status::failure("the project already has experiment '" + e.id + "'");
    for (const auto& m : archive.workspace.measurements)
        if (target.measurement(m.id))
            return Status::failure("the project already has measurement '" + m.id + "'");
    std::error_code ec;
    for (const auto& m : archive.workspace.measurements) {
        if (m.dataRef.empty())
            continue;
        const auto expected = formatOfRef(m.dataRef);
        if (expected && *expected != targetStore.format())
            return Status::failure("the archive's data is in the " + std::string(toString(*expected)) + " format, the project's store in " +
                                   std::string(toString(targetStore.format())));
        const fs::path to = targetStore.root() / m.dataRef;
        if (fs::exists(to, ec))
            return Status::failure("the project's store already holds " + m.dataRef);
    }
    for (const auto& m : archive.workspace.measurements) {
        if (m.dataRef.empty())
            continue;
        const fs::path from = archive.directory / "data" / m.dataRef;
        const fs::path to = targetStore.root() / m.dataRef;
        fs::create_directories(to.parent_path(), ec);
        fs::copy(from, to, fs::copy_options::recursive, ec);
        if (ec)
            return Status::failure("copying " + m.dataRef + ": " + ec.message());
    }
    for (const auto& e : archive.workspace.experiments)
        (void)target.addExperiment(e);
    for (const auto& m : archive.workspace.measurements)
        (void)target.addMeasurement(m);
    for (const auto& c : archive.workspace.calibrations.all())
        if (!target.calibrations.find(c.id))
            (void)target.calibrations.add(c);
    for (const auto& s : archive.workspace.spatial)
        target.spatial.push_back(s);
    for (const auto& c : archive.workspace.comparisons)
        target.comparisons.push_back(c);
    return Status::success();
}

Result<ReproductionReport> reproduceProcessing(const ExperimentWorkspace& w, const std::string& experimentId, const MeasurementStore& store)
{
    using R = Result<ReproductionReport>;
    const Experiment* e = w.experiment(experimentId);
    if (!e)
        return R::failure("no experiment '" + experimentId + "'");
    ProcessingContext ctx;
    ctx.calibrations = &w.calibrations;
    for (const auto& s : e->sensors) {
        ctx.sensorQuantity[s.id] = s.measures;
        if (!s.calibrationId.empty())
            ctx.sensorCalibration[s.id] = s.calibrationId;
    }
    for (const auto& b : e->channels)
        ctx.bindingGain[b.id] = b.externalGain;
    ReproductionReport report;
    std::map<std::string, ProcessingItem> cache;
    auto load = [&](const std::string& id) -> const ProcessingItem* {
        if (auto it = cache.find(id); it != cache.end())
            return &it->second;
        const MeasurementRecord* r = w.measurement(id);
        if (!r)
            return nullptr;
        auto data = store.read(r->dataRef);
        if (!data)
            return nullptr;
        return &(cache[id] = ProcessingItem{*r, data.take()});
    };
    for (const auto* m : w.measurementsOf(experimentId)) {
        if (m->category == DataCategory::Raw || m->history.empty())
            continue;
        ++report.compared;
        std::vector<const ProcessingItem*> sources;
        for (const auto& id : m->sourceIds) {
            const ProcessingItem* s = load(id);
            if (!s) {
                report.problems.push_back(m->id + ": source " + id + " is missing");
                break;
            }
            sources.push_back(s);
        }
        if (sources.size() != m->sourceIds.size())
            continue;
        ProcessingStep step = m->history.back();
        step.description.clear();
        auto again = runStep(step, sources, ctx);
        if (!again) {
            report.problems.push_back(m->id + ": " + again.message());
            continue;
        }
        const ProcessingItem* stored = load(m->id);
        if (!stored) {
            report.problems.push_back(m->id + ": its stored data is missing");
            continue;
        }
        double scale = 0.0;
        const double d = maxDifference(again.value().data, stored->data, scale);
        const double relative = scale > 0.0 ? d / scale : d;
        report.maxRelativeDifference = std::max(report.maxRelativeDifference, relative);
        if (again.value().record.id != m->id)
            report.problems.push_back(m->id + ": re-running gives the id " + again.value().record.id);
        else if (relative <= 1e-9)
            ++report.reproduced;
        else
            report.problems.push_back(m->id + ": differs by " + std::to_string(relative) + " (relative)");
    }
    return report;
}

}  // namespace resonant::experiments
