// The accounting application services (REQ-CHAT-20261126-022, RES-ACC-016).
#include "services/Services.h"

#include "accounting/Conservation.h"
#include "accounting/Quantities.h"
#include "coupling/AccountingStudies.h"
#include "foundation/Dimensions.h"
#include "project/ProjectSerializer.h"

#include <mutex>

namespace resonant::services {

using nlohmann::json;

namespace {

class AccountingJob final : public Job {
public:
    AccountingJob(std::string name, std::shared_ptr<const Project> project, json study)
        : name_(std::move(name)), project_(std::move(project)), study_(std::move(study))
    {
    }
    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override
    {
        auto r = coupling::runAccountingStudy(study_, *project_, &context);
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
    std::string name_;
    std::shared_ptr<const Project> project_;
    json study_;
    mutable std::mutex mutex_;
    std::shared_ptr<const json> result_;
};

json workspaceOf(const Project& p)
{
    json w = p.extensions.value(AccountingService::kExtension, json::object());
    if (!w.is_object() || w.empty())
        w = {{"format", "resonant.accounting"}, {"version", 1}, {"studies", json::array()}, {"records", json::array()}};
    if (!w.contains("studies"))
        w["studies"] = json::array();
    if (!w.contains("records"))
        w["records"] = json::array();
    return w;
}

void audit(Session& s, const CallContext& c, const std::string& operation, const std::vector<std::string>& resources)
{
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = resources;
    r.revisionBefore = r.revisionAfter = s.projectRevision();
    r.succeeded = true;
    s.record(std::move(r));
}

}  // namespace

Outcome<Mutation> AccountingService::change(const CallContext& c, const std::string& operation, const std::string& description,
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

Outcome<json> AccountingService::families(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& f : accounting::families())
        out.push_back({{"id", f.id}, {"name", f.name}, {"unit", f.unit}, {"dimension", f.dimension.toString()}, {"conserved", f.conserved},
                       {"vector", f.vector}, {"note", f.note}});
    return out;
}

Outcome<json> AccountingService::convert(const CallContext& c, double value, const std::string& from, const std::string& to)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto r = convertUnit(value, from, to);
    if (!r)
        return invalid(r.message(), "unit");
    return json{{"value", r.value()}, {"unit", to}, {"from", from}, {"dimension", parseUnit(to).value().dimension.toString()}};
}

Outcome<json> AccountingService::checkModels(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& m : accounting::checkModels())
        out.push_back(m.toJson());
    return out;
}

Outcome<json> AccountingService::sourceKinds(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    return coupling::accountingSourceKinds();
}

Outcome<json> AccountingService::studies(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    return workspaceOf(s_.project())["studies"];
}

Outcome<Mutation> AccountingService::setStudy(const CallContext& c, const json& study)
{
    if (Status v = coupling::validateAccountingStudy(study); !v)
        return invalid(v.message(), "study");
    const std::string id = study["id"].get<std::string>();
    return change(c, "AccountingService.SetStudy", "Set accounting study " + study.value("name", id), [study, id](Project& p) -> Outcome<bool> {
        json w = workspaceOf(p);
        bool replaced = false;
        for (json& s : w["studies"])
            if (s.value("id", std::string()) == id) {
                s = study;
                replaced = true;
            }
        if (!replaced)
            w["studies"].push_back(study);
        p.extensions[kExtension] = w;
        return true;
    });
}

Outcome<Mutation> AccountingService::deleteStudy(const CallContext& c, const std::string& id)
{
    return change(c, "AccountingService.DeleteStudy", "Delete accounting study " + id, [id](Project& p) -> Outcome<bool> {
        json w = workspaceOf(p);
        json kept = json::array();
        for (const json& s : w["studies"])
            if (s.value("id", std::string()) != id)
                kept.push_back(s);
        if (kept.size() == w["studies"].size())
            return notFound("there is no accounting study '" + id + "'", "study", id);
        w["studies"] = kept;
        p.extensions[kExtension] = w;
        return true;
    });
}

Outcome<JobView> AccountingService::run(const CallContext& c, const std::string& studyId, const json& inline_)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    json study = inline_;
    if (!studyId.empty()) {
        study = json();
        const json w = workspaceOf(s_.project());  // a local: iterating a temporary's member would dangle
        for (const json& s : w["studies"])
            if (s.value("id", std::string()) == studyId)
                study = s;
        if (study.is_null())
            return notFound("there is no accounting study '" + studyId + "'", "study", studyId);
    } else if (study.is_object() && !study.contains("id")) {
        study["id"] = "inline";
    }
    if (Status v = coupling::validateAccountingStudy(study); !v)
        return invalid(v.message(), "study");
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    std::shared_ptr<const Project> snapshot(std::move(copy.value().project));
    const std::string name = study.value("name", study["id"].get<std::string>());
    auto job = std::make_shared<AccountingJob>("Accounting: " + name, snapshot, study);
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = kRunKind;
    record.sessionId = a.value().sessionId;
    record.onFinished = [job, revision, name](Session& session, const JobInfo& info) {
        const auto doc = job->result();
        if (!doc)
            return;
        json d = *doc;
        d["jobId"] = info.id.str();
        d["projectRevision"] = revision;
        ResultEntry entry;
        entry.kind = kRunKind;
        entry.name = name;
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        entry.document = std::make_shared<const json>(std::move(d));
        const std::string resultId = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(resultId);
    };
    s_.trackJob(std::move(record));
    audit(s_, a.value(), "AccountingService.Run", {study["id"].get<std::string>(), jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<const ResultEntry*> AccountingService::entry(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* e = s_.result(resultId);
    if (!e || e->kind != kRunKind || !e->document)
        return notFound("there is no accounting result '" + resultId + "'", "result", resultId);
    return e;
}

Outcome<json> AccountingService::result(const CallContext& c, const std::string& resultId)
{
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    json d = *e.value()->document;
    d["resultId"] = resultId;
    return d;
}

Outcome<json> AccountingService::report(const CallContext& c, const std::string& resultId, const json& system)
{
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    auto r = coupling::accountingReport(*e.value()->document, system);
    if (!r)
        return invalid(r.message(), "system");
    return r.take();
}

Outcome<json> AccountingService::compare(const CallContext& c, const std::string& resultId, const json& systems)
{
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    auto r = coupling::compareAccountingSystems(*e.value()->document, systems);
    if (!r)
        return invalid(r.message(), "systems");
    return r.take();
}

Outcome<std::string> AccountingService::exportResult(const CallContext& c, const std::string& resultId, const std::string& format)
{
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    auto r = coupling::exportAccounting(*e.value()->document, format.empty() ? std::string("json") : format);
    if (!r)
        return invalid(r.message(), "format");
    return r.take();
}

Outcome<json> AccountingService::query(const CallContext& c, const std::string& resultId, const json& system, const std::string& facet)
{
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    auto r = coupling::queryAccounting(*e.value()->document, system, facet);
    if (!r)
        return invalid(r.message(), "facet");
    return r.take();
}

Outcome<Mutation> AccountingService::saveRecord(const CallContext& c, const std::string& resultId, const std::string& recordId, const std::string& name)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto e = entry(c, resultId);
    if (!e)
        return e.error();
    const std::string id = recordId.empty() ? "acc-" + Id::generate().str().substr(0, 8) : recordId;
    json rec = coupling::accountingRecord(*e.value()->document, id, name.empty() ? e.value()->name : name);
    rec["result"] = resultId;
    return change(c, "AccountingService.SaveRecord", "Save accounting record " + rec.value("name", id), [rec, id](Project& p) -> Outcome<bool> {
        json w = workspaceOf(p);
        for (const json& r : w["records"])
            if (r.value("id", std::string()) == id)
                return ServiceError::make(ErrorCode::AlreadyExists, "an accounting record '" + id + "' exists");
        w["records"].push_back(rec);
        p.extensions[kExtension] = w;
        return true;
    });
}

Outcome<std::vector<json>> AccountingService::records(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    std::vector<json> out;
    const json w = workspaceOf(s_.project());
    for (const json& r : w["records"])
        out.push_back({{"id", r.value("id", std::string())}, {"name", r.value("name", std::string())}, {"title", r.value("title", std::string())},
                       {"status", r.value("status", std::string())}, {"created", r.value("created", std::string())},
                       {"samples", r.value("retainedSamples", 0)}});
    return out;
}

Outcome<json> AccountingService::record(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const json w = workspaceOf(s_.project());
    for (const json& r : w["records"])
        if (r.value("id", std::string()) == id) {
            json out = r;
            auto reports = coupling::accountingRecordReports(r);
            if (!reports)
                return ServiceError::make(ErrorCode::FailedPrecondition, "the record cannot be evaluated: " + reports.message());
            out["reports"] = reports.take();
            return out;
        }
    return notFound("there is no accounting record '" + id + "'", "record", id);
}

Outcome<Mutation> AccountingService::deleteRecord(const CallContext& c, const std::string& id)
{
    return change(c, "AccountingService.DeleteRecord", "Delete accounting record " + id, [id](Project& p) -> Outcome<bool> {
        json w = workspaceOf(p);
        json kept = json::array();
        for (const json& r : w["records"])
            if (r.value("id", std::string()) != id)
                kept.push_back(r);
        if (kept.size() == w["records"].size())
            return notFound("there is no accounting record '" + id + "'", "record", id);
        w["records"] = kept;
        p.extensions[kExtension] = w;
        return true;
    });
}

}  // namespace resonant::services
