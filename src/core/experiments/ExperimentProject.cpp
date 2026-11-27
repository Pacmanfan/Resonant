#include "experiments/ExperimentProject.h"

#include <algorithm>

namespace resonant::experiments {

Experiment* ExperimentWorkspace::experiment(const std::string& id)
{
    for (auto& e : experiments)
        if (e.id == id)
            return &e;
    return nullptr;
}

const Experiment* ExperimentWorkspace::experiment(const std::string& id) const
{
    for (const auto& e : experiments)
        if (e.id == id)
            return &e;
    return nullptr;
}

const MeasurementRecord* ExperimentWorkspace::measurement(const std::string& id) const
{
    for (const auto& m : measurements)
        if (m.id == id)
            return &m;
    return nullptr;
}

std::vector<const MeasurementRecord*> ExperimentWorkspace::measurementsOf(const std::string& experimentId) const
{
    std::vector<const MeasurementRecord*> out;
    for (const auto& m : measurements)
        if (m.experimentId == experimentId)
            out.push_back(&m);
    return out;
}

Status ExperimentWorkspace::addMeasurement(MeasurementRecord record)
{
    if (record.id.empty())
        return Status::failure("a measurement record needs an id");
    if (measurement(record.id))
        return Status::failure("measurement " + record.id + " exists already; records are never replaced");
    measurements.push_back(std::move(record));
    return Status::success();
}

Status ExperimentWorkspace::addExperiment(Experiment e)
{
    if (e.id.empty())
        return Status::failure("an experiment needs an id");
    if (experiment(e.id))
        return Status::failure("experiment " + e.id + " exists already");
    experiments.push_back(std::move(e));
    return Status::success();
}

nlohmann::json toJson(const ExperimentWorkspace& w)
{
    nlohmann::json experiments = nlohmann::json::array(), measurements = nlohmann::json::array(), spatial = nlohmann::json::array(),
                   comparisons = nlohmann::json::array();
    for (const auto& e : w.experiments)
        experiments.push_back(toJson(e));
    for (const auto& m : w.measurements)
        measurements.push_back(toJson(m));
    for (const auto& s : w.spatial)
        spatial.push_back(toJson(s));
    for (const auto& c : w.comparisons)
        comparisons.push_back({{"id", c.id},
                               {"experimentId", c.experimentId},
                               {"measurementId", c.measurementId},
                               {"simulatedLabel", c.simulatedLabel},
                               {"kind", c.kind},
                               {"metrics", c.metrics},
                               {"createdUtc", c.createdUtc},
                               {"note", c.note}});
    return {{"version", kExperimentsVersion},
            {"experiments", experiments},
            {"calibrations", w.calibrations.toJson()},
            {"measurements", measurements},
            {"spatial", spatial},
            {"comparisons", comparisons}};
}

Result<ExperimentWorkspace> experimentWorkspaceFromJson(const nlohmann::json& v)
{
    using R = Result<ExperimentWorkspace>;
    ExperimentWorkspace w;
    if (v.is_null())
        return w;
    if (!v.is_object())
        return R::failure("extensions.experiments must be an object");
    const int version = v.value("version", 1);
    if (version > kExperimentsVersion)
        return R::failure("the experiments were saved by a newer Resonant (format " + std::to_string(version) + ")");
    for (const auto& e : v.value("experiments", nlohmann::json::array())) {
        auto experiment = experimentFromJson(e);
        if (!experiment)
            return R::failure(experiment.message());
        if (Status ok = w.addExperiment(experiment.take()); !ok)
            return R::failure(ok.message());
    }
    auto calibrations = CalibrationLibrary::fromJson(v.value("calibrations", nlohmann::json::array()));
    if (!calibrations)
        return R::failure(calibrations.message());
    w.calibrations = calibrations.take();
    for (const auto& m : v.value("measurements", nlohmann::json::array())) {
        auto record = measurementRecordFromJson(m);
        if (!record)
            return R::failure(record.message());
        if (Status ok = w.addMeasurement(record.take()); !ok)
            return R::failure(ok.message());
    }
    for (const auto& s : v.value("spatial", nlohmann::json::array())) {
        auto dataset = spatialDatasetFromJson(s);
        if (!dataset)
            return R::failure(dataset.message());
        w.spatial.push_back(dataset.take());
    }
    for (const auto& c : v.value("comparisons", nlohmann::json::array()))
        w.comparisons.push_back({c.value("id", std::string()), c.value("experimentId", std::string()),
                                 c.value("measurementId", std::string()), c.value("simulatedLabel", std::string()),
                                 c.value("kind", std::string()), c.value("metrics", nlohmann::json::object()),
                                 c.value("createdUtc", std::string()), c.value("note", std::string())});
    return w;
}

Result<ExperimentWorkspace> loadExperiments(const Project& project)
{
    auto it = project.extensions.find(kExperimentsKey);
    if (it == project.extensions.end())
        return ExperimentWorkspace{};
    return experimentWorkspaceFromJson(*it);
}

void storeExperiments(Project& project, const ExperimentWorkspace& w)
{
    if (w.experiments.empty() && w.measurements.empty() && w.calibrations.all().empty() && w.spatial.empty() && w.comparisons.empty()) {
        project.extensions.erase(kExperimentsKey);
        return;
    }
    project.extensions[kExperimentsKey] = toJson(w);
}

}  // namespace resonant::experiments
