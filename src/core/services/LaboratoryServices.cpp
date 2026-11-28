// The laboratory application services (REQ-CHAT-20261126-023, RES-LAB-017).
#include "services/Services.h"

#include "accounting/Conservation.h"
#include "coupling/LaboratoryModels.h"
#include "coupling/LaboratoryStudies.h"
#include "experiments/ExperimentProject.h"
#include "experiments/MeasurementStore.h"
#include "experiments/Processing.h"
#include "foundation/Time.h"
#include "laboratory/ErrorBudget.h"
#include "laboratory/ExperimentalEnergy.h"
#include "laboratory/LabImport.h"
#include "laboratory/Laboratory.h"
#include "materialstate/StateModel.h"
#include "optimization/Expression.h"
#include "project/ProjectSerializer.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace resonant::services {

using nlohmann::json;
namespace lab = laboratory;
namespace ex = experiments;
namespace fs = std::filesystem;

namespace {

Outcome<lab::Workspace> labOf(const Project& p)
{
    auto w = lab::workspaceOf(p);
    if (!w)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's laboratory records cannot be read: " + w.message());
    return w.take();
}

Outcome<ex::ExperimentWorkspace> experimentsOf(const Project& p)
{
    auto w = ex::loadExperiments(p);
    if (!w)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's experiments cannot be read: " + w.message());
    return w.take();
}

// A loader over a snapshot of the records and the store's root.
coupling::DatasetLoader loaderFor(std::vector<ex::MeasurementRecord> records, fs::path root)
{
    auto store = std::make_shared<std::unique_ptr<ex::MeasurementStore>>();
    auto mutex = std::make_shared<std::mutex>();
    return [records = std::move(records), root, store, mutex](const std::string& id) -> Result<ex::ProcessingItem> {
        using R = Result<ex::ProcessingItem>;
        const ex::MeasurementRecord* rec = nullptr;
        for (const auto& r : records)
            if (r.id == id)
                rec = &r;
        if (!rec)
            return R::failure("there is no dataset '" + id + "'");
        std::lock_guard lock(*mutex);
        if (!*store) {
            if (root.empty())
                return R::failure("the project is not saved: its measurement store has no place yet");
            auto s = ex::openMeasurementStore(root);
            if (!s)
                return R::failure(s.message());
            *store = s.take();
        }
        auto data = (*store)->read(rec->dataRef);
        if (!data)
            return R::failure(data.message());
        return ex::ProcessingItem{*rec, data.take()};
    };
}

fs::path storeRoot(Session& s)
{
    const std::string path = s.host().filePath();
    return path.empty() ? fs::path() : ex::measurementRootFor(path);
}

class CalibrationJob final : public Job {
public:
    CalibrationJob(json spec, std::shared_ptr<const Project> project, coupling::DatasetLoader load)
        : spec_(std::move(spec)), project_(std::move(project)), load_(std::move(load))
    {
    }
    std::string name() const override { return "Calibration of " + spec_.value("model", std::string()); }
    JobEnd run(JobContext& context) override
    {
        auto r = coupling::runCalibration(spec_, *project_, load_, &context);
        if (!r)
            return r.message() == "cancelled" ? JobEnd::cancelled() : JobEnd::failed(r.message());
        std::lock_guard lock(mutex_);
        result_ = std::make_shared<const json>(r.take());
        return JobEnd::completed();
    }
    std::shared_ptr<const json> result() const
    {
        std::lock_guard lock(mutex_);
        return result_;
    }

private:
    json spec_;
    std::shared_ptr<const Project> project_;
    coupling::DatasetLoader load_;
    mutable std::mutex mutex_;
    std::shared_ptr<const json> result_;
};

json decimated(const std::vector<double>& v, size_t maxPoints)
{
    if (maxPoints == 0 || v.size() <= maxPoints)
        return v;
    json out = json::array();
    for (size_t i = 0; i < maxPoints; ++i)
        out.push_back(v[i * (v.size() - 1) / (maxPoints - 1)]);
    return out;
}

}  // namespace

Outcome<Mutation> LaboratoryService::change(const CallContext& c, const std::string& operation, const std::string& description,
                                            const std::function<Outcome<bool>(Project&)>& apply)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    auto ok = apply(changed);
    if (!ok)
        return ok.error();
    return s_.applyCopy(c, operation, changed, description);
}

Outcome<json> LaboratoryService::models(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& m : coupling::laboratoryModels())
        out.push_back(m.toJson());
    return out;
}

Outcome<json> LaboratoryService::modelParameters(const CallContext& c, const std::string& model, const json& cfg)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto p = coupling::laboratoryModelParameters(model, cfg, s_.project());
    if (!p)
        return invalid(p.message(), "model");
    json out = json::array();
    for (const auto& x : p.value())
        out.push_back({{"id", x.id}, {"label", x.label}, {"unit", x.unit}, {"meaning", x.meaning}, {"value", x.value}, {"lower", x.lower}, {"upper", x.upper}});
    return out;
}

Outcome<json> LaboratoryService::studies(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    json out = json::array();
    for (const lab::Study& st : w.value().studies) {
        json j = st.toJson();
        j["stale"] = coupling::staleLinks(s_.project(), st);
        out.push_back(j);
    }
    return out;
}

Outcome<json> LaboratoryService::study(const CallContext& c, const std::string& id)
{
    auto all = studies(c);
    if (!all)
        return all.error();
    for (const json& s : all.value())
        if (s.value("id", std::string()) == id)
            return s;
    return notFound("there is no study '" + id + "'", "study", id);
}

Outcome<Mutation> LaboratoryService::setStudy(const CallContext& c, const json& doc)
{
    auto parsed = lab::Study::fromJson(doc);
    if (!parsed)
        return invalid(parsed.message(), "study");
    lab::Study st = parsed.take();
    if (st.created.empty())
        st.created = utcNowIso8601();
    // New links are linked at the content's current revision.
    for (lab::ModelLink& l : st.models)
        if (l.revision.empty()) {
            l.revision = coupling::currentRevision(s_.project(), l);
            if (l.revision.empty())
                return invalid("the linked " + l.kind + " '" + l.id + "' is not in the project", "study.models");
        }
    return change(c, "LaboratoryService.SetStudy", "Set laboratory study " + (st.name.empty() ? st.id : st.name), [st](Project& p) -> Outcome<bool> {
        auto w = labOf(p);
        if (!w)
            return w.error();
        if (lab::Study* existing = w.value().study(st.id))
            *existing = st;
        else
            w.value().studies.push_back(st);
        lab::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<Mutation> LaboratoryService::deleteStudy(const CallContext& c, const std::string& id)
{
    return change(c, "LaboratoryService.DeleteStudy", "Delete laboratory study " + id, [id](Project& p) -> Outcome<bool> {
        auto w = labOf(p);
        if (!w)
            return w.error();
        auto& v = w.value().studies;
        const auto before = v.size();
        v.erase(std::remove_if(v.begin(), v.end(), [&](const lab::Study& s) { return s.id == id; }), v.end());
        if (v.size() == before)
            return notFound("there is no study '" + id + "'", "study", id);
        lab::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<LabChange> LaboratoryService::importDataset(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    std::string content, fileName = spec.value("fileName", std::string());
    if (spec.contains("path")) {
        if (auto a = s_.authorize(c, Permission::Files); !a)
            return a.error();
        fs::path p = spec["path"].get<std::string>();
        if (p.is_relative() && !s_.host().filePath().empty())
            p = fs::path(s_.host().filePath()).parent_path() / p;
        std::ifstream in(p, std::ios::binary);
        if (!in)
            return notFound("the file cannot be read", "path", spec["path"].get<std::string>());
        std::ostringstream ss;
        ss << in.rdbuf();
        content = ss.str();
        if (fileName.empty())
            fileName = p.filename().string();
    } else if (spec.contains("content")) {
        content = spec["content"].get<std::string>();
    } else {
        return invalid("give the file's content or its path", "spec");
    }
    const fs::path root = storeRoot(s_);
    if (root.empty())
        return ServiceError::make(ErrorCode::FailedPrecondition, "save the project first: imported data is kept in the measurement store next to the project file");
    lab::ImportOptions o;
    o.fileName = fileName.empty() ? std::string("imported.csv") : fileName;
    o.format = spec.value("format", std::string("auto"));
    o.provenance = spec.value("provenance", std::string());
    o.domain = spec.value("domain", std::string());
    o.expectedColumns = spec.value("expectedColumns", std::vector<std::string>{});
    o.acceptPartial = spec.value("acceptPartial", false);
    o.experimentId = "laboratory";
    lab::ImportResult imported = lab::importData(content, o);
    // Ids of this content: a re-import of the same bytes is recognized, never duplicated.
    for (auto& d : imported.datasets) {
        d.record.id = d.record.id + "-" + imported.report.sha256.substr(0, 8);
    }
    imported.report.datasets.clear();
    for (const auto& d : imported.datasets)
        imported.report.datasets.push_back(d.record.id);
    auto experiments = experimentsOf(s_.project());
    if (!experiments)
        return experiments.error();
    for (const auto& d : imported.datasets)
        if (experiments.value().measurement(d.record.id))
            return ServiceError::make(ErrorCode::AlreadyExists, "this file (SHA-256 " + imported.report.sha256.substr(0, 12) + "...) is imported already as '" +
                                                                    d.record.id + "'");
    std::vector<ex::MeasurementRecord> records;
    json source = json::object();
    if (imported.report.accepted) {
        auto store = ex::openMeasurementStore(root);
        if (!store)
            return ServiceError::make(ErrorCode::Internal, store.message());
        for (auto& d : imported.datasets) {
            d.record.dataRef = store.value()->dataRefFor(d.record);
            if (Status st = store.value()->write(d.record, d.data); !st)
                return ServiceError::make(ErrorCode::Internal, "the dataset could not be stored: " + st.message());
            records.push_back(d.record);
        }
        // The original bytes, unchanged and read-only.
        const fs::path dir = root.parent_path() / "laboratory" / "sources";
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::string safe = fs::path(o.fileName).filename().string();
        for (char& ch : safe)
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '.' && ch != '-' && ch != '_')
                ch = '_';
        const fs::path file = dir / (imported.report.sha256.substr(0, 16) + "-" + safe);
        if (!fs::exists(file)) {
            std::ofstream out(file, std::ios::binary);
            out << content;
            out.close();
            fs::permissions(file, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove, ec);
        }
        source = {{"sha256", imported.report.sha256}, {"file", imported.report.file}, {"format", imported.report.format}, {"bytes", imported.report.bytes},
                  {"path", fs::relative(file, root.parent_path(), ec).generic_string()}};
    }
    const json report = imported.report.toJson();
    const std::string studyId = spec.value("study", std::string());
    auto role = lab::datasetRoleFromString(spec.value("role", std::string("raw")));
    if (!role)
        return invalid("unknown dataset role '" + spec.value("role", std::string()) + "'", "role");
    auto m = change(c, "LaboratoryService.ImportDataset", "Import " + imported.report.file, [=](Project& p) -> Outcome<bool> {
        auto ew = experimentsOf(p);
        if (!ew)
            return ew.error();
        for (const auto& r : records)
            if (Status st = ew.value().addMeasurement(r); !st)
                return ServiceError::make(ErrorCode::AlreadyExists, st.message());
        ex::storeExperiments(p, ew.value());
        auto w = labOf(p);
        if (!w)
            return w.error();
        w.value().imports.push_back(report);
        if (!source.empty())
            w.value().sources.push_back(source);
        if (!studyId.empty()) {
            lab::Study* st = w.value().study(studyId);
            if (!st)
                return notFound("there is no study '" + studyId + "'", "study", studyId);
            for (const auto& r : records)
                st->datasets.push_back({r.id, *role, "imported from " + imported.report.file});
            if (Status v = st->validate(); !v)
                return invalid(v.message(), "role");
            if (st->status == "planned" && !records.empty())
                st->status = "measured";
        }
        lab::storeWorkspace(p, w.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), report};
}

Outcome<json> LaboratoryService::datasets(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    json out = json::array();
    for (const auto& r : ew.value().measurements)
        out.push_back({{"id", r.id}, {"experiment", r.experimentId}, {"kind", std::string(ex::toString(r.kind))}, {"category", std::string(ex::toString(r.category))},
                       {"quantity", r.quantity.describe()}, {"unit", r.quantity.unit()}, {"points", r.shape.empty() ? 0 : r.shape.front()},
                       {"provenance", r.simulatedDevice ? std::string("synthetic or simulated") : r.acquisition.value("provenance", std::string("measured"))},
                       {"instrument", r.deviceId}, {"calibration", r.calibrationId}, {"sources", r.sourceIds}, {"history", r.history.size()},
                       {"created", r.createdUtc}, {"note", r.note}});
    return out;
}

Outcome<json> LaboratoryService::dataset(const CallContext& c, const std::string& id, int maxPoints)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    auto item = loaderFor(ew.value().measurements, storeRoot(s_))(id);
    if (!item)
        return notFound(item.message(), "dataset", id);
    const auto& d = item.value().data;
    const size_t limit = maxPoints > 0 ? static_cast<size_t>(maxPoints) : 0;
    json data{{"axis", decimated(d.axis, limit)}, {"real", decimated(d.real, limit)}, {"auxiliary", decimated(d.auxiliary, limit)}, {"labels", d.labels},
              {"points", d.size()}};
    if (d.isComplex()) {
        std::vector<double> re, im;
        for (const auto& v : d.complex) {
            re.push_back(v.real());
            im.push_back(v.imag());
        }
        data["complexReal"] = decimated(re, limit);
        data["complexImag"] = decimated(im, limit);
    }
    return json{{"record", ex::toJson(item.value().record)}, {"data", data}};
}

Outcome<LabChange> LaboratoryService::process(const CallContext& c, const std::string& datasetId, const json& stepJson)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto step = ex::processingStepFromJson(stepJson);
    if (!step)
        return invalid(step.message(), "step");
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    const fs::path root = storeRoot(s_);
    auto item = loaderFor(ew.value().measurements, root)(datasetId);
    if (!item)
        return notFound(item.message(), "dataset", datasetId);
    ex::ProcessingContext context;
    context.calibrations = &ew.value().calibrations;
    auto derived = ex::runStep(step.value(), {&item.value()}, context);
    if (!derived)
        return invalid(derived.message(), "step");
    ex::MeasurementRecord rec = derived.value().record;
    if (ew.value().measurement(rec.id))
        return ServiceError::make(ErrorCode::AlreadyExists, "this processing of '" + datasetId + "' exists already as '" + rec.id + "' (processing is reproducible)");
    auto store = ex::openMeasurementStore(root);
    if (!store)
        return ServiceError::make(ErrorCode::Internal, store.message());
    rec.dataRef = store.value()->dataRefFor(rec);
    if (Status st = store.value()->write(rec, derived.value().data); !st)
        return ServiceError::make(ErrorCode::Internal, st.message());
    auto m = change(c, "LaboratoryService.Process", "Process " + datasetId + " (" + step.value().operation + ")", [rec](Project& p) -> Outcome<bool> {
        auto w = experimentsOf(p);
        if (!w)
            return w.error();
        if (Status st = w.value().addMeasurement(rec); !st)
            return ServiceError::make(ErrorCode::AlreadyExists, st.message());
        ex::storeExperiments(p, w.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), ex::toJson(rec)};
}

Outcome<json> LaboratoryService::resonances(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    auto r = coupling::extractResonances(spec, loaderFor(ew.value().measurements, storeRoot(s_)));
    if (!r)
        return invalid(r.message(), "spec");
    return r.take();
}

Outcome<LabChange> LaboratoryService::compare(const CallContext& c, const json& specIn)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    json spec = specIn;
    if (spec.value("id", std::string()).empty())
        spec["id"] = "cmp-" + Id::generate().str().substr(0, 8);
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    auto record = coupling::runComparison(spec, s_.project(), loaderFor(ew.value().measurements, storeRoot(s_)), nullptr);
    if (!record)
        return invalid(record.message(), "spec");
    const json rec = record.take();
    auto m = change(c, "LaboratoryService.Compare", "Compare " + spec.value("measured", std::string()), [rec](Project& p) -> Outcome<bool> {
        auto w = labOf(p);
        if (!w)
            return w.error();
        if (w.value().record("comparisons", rec.value("id", std::string())))
            return ServiceError::make(ErrorCode::AlreadyExists, "a comparison '" + rec.value("id", std::string()) + "' exists");
        w.value().comparisons.push_back(rec);
        if (lab::Study* st = w.value().study(rec.value("study", std::string())); st && (st->status == "planned" || st->status == "measured"))
            st->status = "compared";
        lab::storeWorkspace(p, w.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), rec};
}

Outcome<json> LaboratoryService::budget(const CallContext& c, const json& doc)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto b = lab::ErrorBudget::fromJson(doc);
    if (!b)
        return invalid(b.message(), "budget");
    auto r = b.value().evaluate();
    if (!r)
        return invalid(r.message(), "budget");
    return r.value().toJson();
}

Outcome<json> LaboratoryService::propagate(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto expr = optimization::Expression::parse(spec.value("expression", std::string()));
    if (!expr)
        return invalid(expr.message(), "expression");
    std::vector<lab::InputQuantity> inputs;
    for (const json& i : spec.value("inputs", json::array()))
        inputs.push_back({i.value("id", std::string()), i.value("mean", 0.0), i.value("standard", 0.0), i.value("distribution", std::string("normal"))});
    for (const std::string& n : expr.value().names()) {
        bool found = false;
        for (const auto& i : inputs)
            found = found || i.id == n;
        if (!found)
            return invalid("the expression uses '" + n + "', which is not an input", "inputs");
    }
    Eigen::MatrixXd r;
    const json corr = spec.value("correlation", json::array());
    if (!corr.empty()) {
        if (corr.size() != inputs.size())
            return invalid("the correlation matrix does not match the inputs", "correlation");
        r.resize(static_cast<Eigen::Index>(inputs.size()), static_cast<Eigen::Index>(inputs.size()));
        for (size_t a = 0; a < inputs.size(); ++a)
            for (size_t b = 0; b < inputs.size(); ++b)
                r(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b)) = corr[a][b].get<double>();
    }
    const auto e = expr.value();
    const lab::Model f = [e, inputs](const std::vector<double>& x) -> Result<double> {
        std::map<std::string, double, std::less<>> values;
        for (size_t i = 0; i < inputs.size(); ++i)
            values[inputs[i].id] = x[i];
        return e.evaluate(values);
    };
    const std::string method = spec.value("method", std::string("monte-carlo"));
    auto p = method == "linear" ? lab::propagateLinear(f, inputs, r)
                                : lab::propagateMonteCarlo(f, inputs, r, spec.value("samples", 20000), spec.value("seed", 1u));
    if (!p)
        return invalid(p.message(), "spec");
    json out = p.value().toJson();
    out["expression"] = e.text();
    return out;
}

Outcome<json> LaboratoryService::energyBalance(const CallContext& c, const json& doc)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto b = lab::ExperimentalBalance::fromJson(doc);
    if (!b)
        return invalid(b.message(), "balance");
    auto l = lab::experimentalLedger(b.value());
    if (!l)
        return invalid(l.message(), "balance");
    accounting::Tolerances t;
    t.coverage = b.value().coverage;
    const auto report = accounting::evaluate(l.value(), accounting::wholeSystem(l.value()), t);
    return json{{"ledger", l.value().toJson()}, {"report", report.toJson()}};
}

Outcome<JobView> LaboratoryService::calibrate(const CallContext& c, const json& specIn)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    json spec = specIn;
    if (spec.value("id", std::string()).empty())
        spec["id"] = "cal-" + Id::generate().str().substr(0, 8);
    if (!coupling::findLaboratoryModel(spec.value("model", std::string())))
        return invalid("unknown laboratory model '" + spec.value("model", std::string()) + "'", "model");
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    std::shared_ptr<const Project> snapshot(std::move(copy.value().project));
    auto job = std::make_shared<CalibrationJob>(spec, snapshot, loaderFor(ew.value().measurements, storeRoot(s_)));
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = kCalibrationKind;
    record.sessionId = a.value().sessionId;
    const std::string name = job->name();
    record.onFinished = [job, revision, name](Session& session, const JobInfo& info) {
        const auto doc = job->result();
        if (!doc)
            return;
        json d = *doc;
        d["jobId"] = info.id.str();
        d["projectRevision"] = revision;
        ResultEntry entry;
        entry.kind = kCalibrationKind;
        entry.name = name;
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        entry.document = std::make_shared<const json>(std::move(d));
        const std::string resultId = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(resultId);
    };
    s_.trackJob(std::move(record));
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<json> LaboratoryService::calibrationResult(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* e = s_.result(resultId);
    if (!e || e->kind != kCalibrationKind || !e->document)
        return notFound("there is no calibration result '" + resultId + "'", "result", resultId);
    json d = *e->document;
    d["resultId"] = resultId;
    return d;
}

Outcome<LabChange> LaboratoryService::acceptCalibration(const CallContext& c, const std::string& resultId)
{
    auto r = calibrationResult(c, resultId);
    if (!r)
        return r.error();
    json rec = r.value();
    rec.erase("jobId");
    rec.erase("resultId");
    rec["accepted"] = utcNowIso8601();
    auto m = change(c, "LaboratoryService.AcceptCalibration", "Keep calibration " + rec.value("id", std::string()), [rec](Project& p) -> Outcome<bool> {
        auto w = labOf(p);
        if (!w)
            return w.error();
        if (w.value().record("calibrations", rec.value("id", std::string())))
            return ServiceError::make(ErrorCode::AlreadyExists, "the calibration '" + rec.value("id", std::string()) + "' is kept already");
        w.value().calibrations.push_back(rec);
        if (lab::Study* st = w.value().study(rec.value("study", std::string())); st && st->status != "validated")
            st->status = "calibrated";
        lab::storeWorkspace(p, w.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), rec};
}

Outcome<LabChange> LaboratoryService::validate(const CallContext& c, const std::string& calibrationId, const std::vector<std::string>& datasets,
                                               const std::string& idIn, const std::vector<std::string>& axisDatasets)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    const json* cal = w.value().record("calibrations", calibrationId);
    if (!cal)
        return notFound("there is no kept calibration '" + calibrationId + "'", "calibration", calibrationId);
    auto ew = experimentsOf(s_.project());
    if (!ew)
        return ew.error();
    const std::string id = idIn.empty() ? "val-" + Id::generate().str().substr(0, 8) : idIn;
    auto v = coupling::runValidation(*cal, datasets, id, s_.project(), loaderFor(ew.value().measurements, storeRoot(s_)), nullptr, axisDatasets);
    if (!v)
        return invalid(v.message(), "datasets");
    const json rec = v.take();
    auto m = change(c, "LaboratoryService.Validate", "Validate " + calibrationId, [rec](Project& p) -> Outcome<bool> {
        auto lw = labOf(p);
        if (!lw)
            return lw.error();
        lw.value().validations.push_back(rec);
        for (json& kept : lw.value().calibrations)
            if (kept.value("id", std::string()) == rec.value("calibration", std::string()))
                kept["validation"] = {{"id", rec.value("id", std::string())}, {"status", rec.value("status", std::string())}};
        if (lab::Study* st = lw.value().study(rec.value("study", std::string())))
            st->status = rec.value("status", std::string()) == "validated within uncertainty" ? "validated" : "inconclusive";
        lab::storeWorkspace(p, lw.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), rec};
}

Outcome<LabChange> LaboratoryService::applyCalibration(const CallContext& c, const std::string& calibrationId, const json& options)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    const json* found = w.value().record("calibrations", calibrationId);
    if (!found)
        return notFound("there is no kept calibration '" + calibrationId + "'", "calibration", calibrationId);
    const json cal = *found;
    if (cal.value("status", std::string()) == "not identifiable" || cal.value("status", std::string()) == "not converged")
        return ServiceError::make(ErrorCode::FailedPrecondition,
                                  "the calibration is '" + cal.value("status", std::string()) + "': applying it would claim values the data do not determine");
    const std::string model = cal.value("model", std::string());
    std::map<std::string, std::pair<double, double>> values;  // id -> estimate, standard error
    for (const json& p : cal["result"].value("parameters", json::array()))
        values[p.value("id", std::string())] = {p.value("estimate", 0.0), p.value("standardError", 0.0)};
    const std::string suffix = "cal-" + Id::generate().str().substr(0, 6);
    const std::string date = utcNowIso8601().substr(0, 10);
    json applied;
    auto m = change(c, "LaboratoryService.ApplyCalibration", "Apply calibration " + calibrationId, [&](Project& p) -> Outcome<bool> {
        if (model == "thermal.evolution-probe") {
            std::map<std::string, std::vector<std::pair<std::string, std::pair<double, double>>>> byMaterial;
            for (const auto& [id, v] : values) {
                const auto a = id.find('.'), b = id.find('.', a + 1);
                byMaterial[id.substr(0, a)].push_back({id.substr(b + 1), v});
            }
            json created = json::array();
            for (const auto& [mat, props] : byMaterial) {
                const Material* original = p.materials.find(Id(mat));
                if (!original)
                    return notFound("there is no material '" + mat + "'", "material", mat);
                Material copy(Id(mat + "-" + suffix), original->name() + " (calibrated " + date + ")");
                copy.setDescription(original->description() + " Calibrated copy: the original material is unchanged.");
                for (PhysicsDomain d : original->domains())
                    if (const PropertySet* set = original->properties(d))
                        for (const auto& [key, v] : *set)
                            if (Status st = copy.setProperty(d, key, v); !st)
                                return invalid(st.message(), "material");
                for (const auto& [key, v] : props) {
                    PropertyValue pv = *original->property(PhysicsDomain::Thermal, key);
                    pv.value = v.first;
                    pv.uncertainty = v.second;
                    pv.provenance = Provenance::Fitted;
                    pv.source = "fitted by the laboratory calibration " + calibrationId + " to " + cal["datasets"].dump() + " (" + cal.value("method", std::string()) + ")";
                    if (Status st = copy.setProperty(PhysicsDomain::Thermal, key, pv); !st)
                        return invalid(st.message(), "material");
                }
                if (Status st = p.materials.add(copy); !st)
                    return invalid(st.message(), "material");
                created.push_back(copy.id().str());
            }
            applied = {{"kind", "material"}, {"materials", created}};
        } else if (model == "material-state.j2-uniaxial") {
            const std::string studyId = options.value("study", std::string());
            auto ws = materialstate::workspaceOf(p);
            if (!ws)
                return invalid(ws.message(), "study");
            json source;
            for (const json& s : ws.value().studies)
                if (s.value("id", std::string()) == studyId)
                    source = s;
            if (source.is_null())
                return invalid("name the material-state study to copy (options.study)", "options.study");
            json copy = source;
            copy["id"] = studyId + "-" + suffix;
            copy["name"] = source.value("name", studyId) + " (calibrated " + date + ")";
            for (const auto& [id, v] : values)
                copy["parameters"][id] = v.first;
            copy["calibration"] = calibrationId;
            ws.value().studies.push_back(copy);
            materialstate::storeWorkspace(p, ws.value());
            applied = {{"kind", "material-state-study"}, {"study", copy["id"]}};
        } else {
            return ServiceError::make(ErrorCode::FailedPrecondition,
                                      "nothing in the project holds the parameters of " + model + ": the calibration record keeps the values");
        }
        auto lw = labOf(p);
        if (!lw)
            return lw.error();
        for (json& r : lw.value().calibrations)
            if (r.value("id", std::string()) == calibrationId)
                r["applied"].push_back(applied);
        lab::storeWorkspace(p, lw.value());
        return true;
    });
    if (!m)
        return m.error();
    return LabChange{m.value(), applied};
}

Outcome<json> LaboratoryService::records(const CallContext& c, const std::string& section)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    if (section == "studies")
        return w.value().toJson()["studies"];
    if (section == "imports")
        return w.value().imports;
    if (section == "comparisons")
        return w.value().comparisons;
    if (section == "calibrations")
        return w.value().calibrations;
    if (section == "validations")
        return w.value().validations;
    if (section == "sources")
        return w.value().sources;
    return invalid("unknown section '" + section + "' (studies, imports, comparisons, calibrations, validations, sources)", "section");
}

Outcome<json> LaboratoryService::explain(const CallContext& c, const std::string& recordId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    for (const char* section : {"comparisons", "calibrations", "validations"})
        if (const json* r = w.value().record(section, recordId))
            return coupling::explainRecord(*r, w.value(), s_.project());
    return notFound("there is no laboratory record '" + recordId + "'", "record", recordId);
}

Outcome<std::string> LaboratoryService::report(const CallContext& c, const std::string& studyId, const std::string& format)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = labOf(s_.project());
    if (!w)
        return w.error();
    auto r = coupling::laboratoryReport(studyId, w.value(), s_.project(), format.empty() ? std::string("markdown") : format);
    if (!r)
        return invalid(r.message(), "study");
    return r.take();
}

Outcome<std::string> LaboratoryService::exportDataset(const CallContext& c, const std::string& id, const std::string& format)
{
    auto d = dataset(c, id, 0);
    if (!d)
        return d.error();
    if (format == "json" || format.empty())
        return d.value().dump(1);
    if (format != "csv")
        return invalid("the export format is csv or json", "format");
    const json& rec = d.value()["record"];
    const json& data = d.value()["data"];
    std::ostringstream o;
    o << "# dataset: " << rec.value("id", std::string()) << "\n# quantity: " << rec.value("quantity", json::object()).dump() << "\n";
    o << "# exported by Resonant's laboratory; the raw record is unchanged\n";
    const auto axis = data.value("axis", std::vector<double>{});
    const auto real = data.value("real", std::vector<double>{});
    const auto re = data.value("complexReal", std::vector<double>{}), im = data.value("complexImag", std::vector<double>{});
    o << "index, axis, " << (re.empty() ? "value" : "real, imag") << "\n";
    const size_t n = re.empty() ? real.size() : re.size();
    for (size_t k = 0; k < n; ++k) {
        o << k << ", " << (k < axis.size() ? axis[k] : 0.0) << ", ";
        if (re.empty())
            o << real[k] << "\n";
        else
            o << re[k] << ", " << im[k] << "\n";
    }
    return o.str();
}

}  // namespace resonant::services
