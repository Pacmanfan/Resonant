// The material-state application services (REQ-CHAT-20261126-021, RES-DMS-014).
#include "services/Services.h"

#include "coupling/MaterialStateModels.h"
#include "coupling/StateEvolution.h"
#include "materialstate/StateModel.h"
#include "multiscale/Representation.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <mutex>

namespace resonant::services {

using nlohmann::json;
namespace mst = materialstate;

namespace {

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

class StateJob final : public Job {
public:
    using Work = std::function<Result<json>(const Project&, JobContext&)>;
    StateJob(std::string name, std::shared_ptr<const Project> project, Work work) : name_(std::move(name)), project_(std::move(project)), work_(std::move(work)) {}
    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override
    {
        auto r = work_(*project_, context);
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
    Work work_;
    mutable std::mutex mutex_;
    std::shared_ptr<const json> result_;
};

Outcome<mst::Workspace> workspace(const Project& p)
{
    auto w = mst::workspaceOf(p);
    if (!w)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's material-state workspace cannot be read: " + w.message());
    return w.take();
}

}  // namespace

Outcome<Mutation> MaterialStateService::change(const CallContext& c, const std::string& operation, const std::string& description,
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

Outcome<json> MaterialStateService::models(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& card : coupling::materialStateModels())
        out.push_back(card.toJson());
    return out;
}

Outcome<json> MaterialStateService::studies(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = workspace(s_.project());
    if (!w)
        return w.error();
    return w.value().studies;
}

Outcome<Mutation> MaterialStateService::setStudy(const CallContext& c, const json& study)
{
    if (!study.is_object() || study.value("id", "").empty())
        return invalid("a study needs an id", "study");
    const mst::ModelCard* card = coupling::findMaterialStateModel(study.value("model", ""));
    if (!card)
        return invalid("unknown material-state model '" + study.value("model", std::string()) + "'", "study.model");
    if (card->status != "implemented")
        return invalid("the model '" + card->title + "' is not implemented", "study.model");
    return change(c, "MaterialStateService.SetStudy", "Set study " + study.value("name", study["id"].get<std::string>()), [study](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        bool replaced = false;
        for (json& s : w.value().studies)
            if (s.value("id", "") == study["id"].get<std::string>()) {
                s = study;
                replaced = true;
            }
        if (!replaced)
            w.value().studies.push_back(study);
        mst::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<Mutation> MaterialStateService::deleteStudy(const CallContext& c, const std::string& id)
{
    return change(c, "MaterialStateService.DeleteStudy", "Delete study " + id, [id](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        auto& list = w.value().studies;
        const auto before = list.size();
        list.erase(std::remove_if(list.begin(), list.end(), [&](const json& s) { return s.value("id", "") == id; }), list.end());
        if (list.size() == before)
            return notFound("there is no study '" + id + "'", "study", id);
        mst::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<JobView> MaterialStateService::startJob(const CallContext& c, const std::string& operation, const std::string& name, const std::string& kind,
                                                std::function<Result<json>(const Project&, JobContext&)> work, const std::vector<std::string>& resources)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    std::shared_ptr<const Project> snapshot(std::move(copy.value().project));
    auto job = std::make_shared<StateJob>(name, snapshot, std::move(work));
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = kind;
    record.sessionId = a.value().sessionId;
    record.onFinished = [job, revision, name, kind](Session& session, const JobInfo& info) {
        const auto doc = job->result();
        if (!doc)
            return;
        json d = *doc;
        d["jobId"] = info.id.str();
        d["projectRevision"] = revision;
        ResultEntry entry;
        entry.kind = kind;
        entry.name = name;
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        entry.document = std::make_shared<const json>(std::move(d));
        const std::string resultId = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(resultId);
    };
    s_.trackJob(std::move(record));
    std::vector<std::string> ids = resources;
    ids.push_back(jobId.str());
    audit(s_, a.value(), operation, ids);
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<JobView> MaterialStateService::run(const CallContext& c, const std::string& studyId, const json& inline_)
{
    if (auto a = s_.authorize(c, Permission::Execute); !a)
        return a.error();
    json study = inline_;
    if (!studyId.empty()) {
        auto w = workspace(s_.project());
        if (!w)
            return w.error();
        study = json();
        for (const json& s : w.value().studies)
            if (s.value("id", "") == studyId)
                study = s;
        if (study.is_null())
            return notFound("there is no study '" + studyId + "'", "study", studyId);
    }
    const mst::ModelCard* card = coupling::findMaterialStateModel(study.is_object() ? study.value("model", "") : "");
    if (!card)
        return invalid("the study names no known material-state model", "study.model");
    if (card->status != "implemented")
        return invalid("the model '" + card->title + "' is not implemented: " + (card->excluded.empty() ? std::string() : card->excluded.front()),
                       "study.model");
    if (card->id == "coupled.evolution") {
        const json p = study.value("parameters", json::object());
        return evolve(c, study.value("system", std::string()),
                      {{"duration", p.value("duration", 60.0)}, {"steps", p.value("steps", 20)}, {"substeps", p.value("substeps", 4)}});
    }
    return startJob(c, "MaterialStateService.Run", card->title, kRunKind,
                    [study](const Project&, JobContext& ctx) { return coupling::runMaterialStateStudy(study, &ctx); }, {studyId});
}

Outcome<JobView> MaterialStateService::evolve(const CallContext& c, const std::string& systemId, const json& settings)
{
    if (auto a = s_.authorize(c, Permission::Execute); !a)
        return a.error();
    json systemDoc;
    for (const json& j : s_.project().extensions.value("multiphysics", json::object()).value("systems", json::array()))
        if (j.value("id", "") == systemId)
            systemDoc = j;
    if (systemDoc.is_null())
        return notFound("there is no physical system '" + systemId + "'", "physical-system", systemId);
    auto system = multiphysics::PhysicalSystem::fromJson(systemDoc);
    if (!system)
        return invalid(system.message(), "system");
    coupling::EvolutionSettings st;
    st.duration = settings.value("duration", 0.0);
    st.steps = settings.value("steps", 20);
    st.substeps = settings.value("substeps", 4);
    if (Status v = st.validate(); !v)
        return invalid(v.message(), "settings");
    const multiphysics::PhysicalSystem sys = system.value();
    return startJob(c, "MaterialStateService.Evolve", "Evolution of " + (sys.name.empty() ? sys.id : sys.name), kEvolutionKind,
                    [sys, st](const Project& p, JobContext& ctx) { return coupling::evolveSystem(sys, p.materials, st, &ctx); }, {systemId});
}

Outcome<json> MaterialStateService::result(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || (entry->kind != kRunKind && entry->kind != kEvolutionKind) || !entry->document)
        return notFound("there is no material-state result '" + resultId + "'", "result", resultId);
    json d = *entry->document;
    d["resultId"] = resultId;
    return d;
}

Outcome<Mutation> MaterialStateService::saveRecord(const CallContext& c, const std::string& resultId, const std::string& recordId, const std::string& name)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || (entry->kind != kRunKind && entry->kind != kEvolutionKind) || !entry->document)
        return notFound("there is no material-state result '" + resultId + "'", "result", resultId);
    const json& d = *entry->document;
    mst::StateRecord r;
    r.id = recordId.empty() ? "rec-" + Id::generate().str().substr(0, 8) : recordId;
    r.name = name.empty() ? entry->name : name;
    r.model = entry->kind == kEvolutionKind ? std::string("coupled.evolution") : d.value("model", std::string());
    r.kind = d.value("kind", std::string());
    r.region = d.value("system", std::string());
    r.created = d.value("created", std::string());
    r.settings = entry->kind == kEvolutionKind ? d.value("settings", json::object())
                                               : json{{"parameters", d.value("parameters", json::object())}, {"loading", d.value("loading", json::object())}};
    r.history = d.value("history", json::array());
    r.final = d.value("final", json::object());
    r.summary = {{"summary", d.value("summary", json::object())}, {"energy", d.value("energy", json::object())},
                 {"reliable", d.value("reliable", false)}, {"reliability", d.value("reliability", json::array())}, {"result", resultId}};
    if (d.contains("profiles"))
        r.summary["profiles"] = d["profiles"];
    if (d.contains("positions"))
        r.summary["positions"] = d["positions"];
    r.committable = d.value("committable", json::array());
    return change(c, "MaterialStateService.SaveRecord", "Save state record " + r.name, [r](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        if (w.value().record(r.id))
            return ServiceError::make(ErrorCode::AlreadyExists, "a record '" + r.id + "' exists");
        w.value().records.push_back(r);
        mst::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<std::vector<json>> MaterialStateService::records(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = workspace(s_.project());
    if (!w)
        return w.error();
    std::vector<json> out;
    for (const auto& r : w.value().records)
        out.push_back({{"id", r.id}, {"name", r.name}, {"model", r.model}, {"kind", r.kind}, {"created", r.created}, {"steps", r.history.size()},
                       {"committable", r.committable}});
    return out;
}

Outcome<json> MaterialStateService::record(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = workspace(s_.project());
    if (!w)
        return w.error();
    const mst::StateRecord* r = w.value().record(id);
    if (!r)
        return notFound("there is no state record '" + id + "'", "record", id);
    return r->toJson();
}

Outcome<Mutation> MaterialStateService::deleteRecord(const CallContext& c, const std::string& id)
{
    return change(c, "MaterialStateService.DeleteRecord", "Delete state record " + id, [id](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        auto& list = w.value().records;
        const auto before = list.size();
        list.erase(std::remove_if(list.begin(), list.end(), [&](const mst::StateRecord& r) { return r.id == id; }), list.end());
        if (list.size() == before)
            return notFound("there is no state record '" + id + "'", "record", id);
        mst::storeWorkspace(p, w.value());
        return true;
    });
}

Outcome<Mutation> MaterialStateService::commit(const CallContext& c, const std::string& recordId, const json& options)
{
    const json o = options.is_object() ? options : json::object();
    return change(c, "MaterialStateService.Commit", "Commit state " + recordId, [recordId, o](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        const mst::StateRecord* r = w.value().record(recordId);
        if (!r)
            return notFound("there is no state record '" + recordId + "'", "record", recordId);
        if (r->committable.empty())
            return ServiceError::make(ErrorCode::FailedPrecondition, "the record has nothing to commit (the model '" + r->model + "' changes no library property)");
        const auto wanted = o.value("properties", std::vector<std::string>{});
        auto model = multiscale::modelOf(p);
        if (!model)
            return ServiceError::make(ErrorCode::FailedPrecondition, model.message());
        multiscale::MultiscaleModel m = model.take();
        // The record as a continuum representation: the source of the committed values (lineage).
        const std::string repId = "state-" + r->id;
        std::map<std::string, multiscale::Derivation> derivations;  // by material
        int committed = 0;
        for (const json& e : r->committable) {
            const std::string domain = e.value("domain", ""), key = e.value("key", ""), full = domain + "." + key;
            if (!wanted.empty() && std::find(wanted.begin(), wanted.end(), full) == wanted.end())
                continue;
            const std::string target = o.value("materialId", e.value("material", std::string()));
            if (target.empty())
                return invalid("the committable value " + full + " names no material: pass materialId", "materialId");
            Material* mat = p.materials.findMutable(Id(target));
            if (!mat)
                return notFound("there is no material '" + target + "'", "material", target);
            const auto d = physicsDomainFromString(domain);
            if (!d)
                return invalid("unknown domain '" + domain + "'", "properties");
            auto& drv = derivations[target];
            if (drv.id.empty()) {
                drv.id = "drv-" + Id::generate().str();
                drv.method = "state." + r->model;
                drv.modelCard = r->model;
                drv.solver = r->model;
                drv.solverVersion = "1";
                drv.representation = repId;
                drv.sourceScale = multiscale::Scale::Continuum;
                drv.targetScale = multiscale::Scale::Continuum;
                drv.material = target;
                drv.inputs = r->settings;
                drv.convergence = r->summary.value("reliability", json::array());
                drv.created = r->created;
                drv.calculation = "state record " + r->id;
            }
            PropertyValue v;
            if (const PropertyValue* old = mat->property(*d, key)) {
                v = *old;
                if (!old->derivation.empty())
                    drv.parent = old->derivation;
            }
            v.value = e.value("value", 0.0);
            v.provenance = Provenance::Computed;
            v.source = "state of '" + r->name + "' (" + r->model + "): " + e.value("note", std::string()) + "; derivation " + drv.id;
            v.derivation = drv.id;
            v.uncertainty.reset();
            if (Status st = mat->setProperty(*d, key, v); !st)
                return invalid(full + ": " + st.message(), "properties");
            if (Status st = mat->validate(); !st)
                return invalid(st.message(), "properties");
            drv.properties.push_back({domain, key, "isotropic", "", v.value, {}});
            ++committed;
        }
        if (committed == 0)
            return invalid("no committable value matched", "properties");
        if (!m.find(repId)) {
            multiscale::Representation rep;
            rep.id = repId;
            rep.name = "State: " + r->name;
            rep.scale = multiscale::Scale::Continuum;
            rep.kind = "continuum-material";
            rep.data = {{"stateRecord", r->id}, {"model", r->model}, {"final", r->final}};
            rep.provenance = "computed";
            rep.source = "the state record '" + r->name + "'";
            rep.modelCard = "representation.continuum-material";
            m.representations.push_back(rep);
        }
        const multiscale::Representation* rep = m.find(repId);
        for (auto& [material, drv] : derivations) {
            drv.representationRevision = rep->revision();
            m.derivations.push_back(drv);
            m.relationships.push_back({"material:" + material, "representation:" + repId, "derived-from"});
            m.relationships.push_back({"derivation:" + drv.id, "representation:" + repId, "computed-from"});
        }
        multiscale::storeModel(p, m);
        return true;
    });
}

}  // namespace resonant::services
