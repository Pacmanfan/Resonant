// The multiscale application services (REQ-CHAT-20261126-020, RES-MSC-014).
#include "services/Services.h"

#include "atomistic/Crystal.h"
#include "atomistic/ForceField.h"
#include "atomistic/StructureImport.h"
#include "coupling/CoupledRun.h"
#include "coupling/MultiscaleCoupling.h"
#include "coupling/MultiscaleDerivations.h"
#include "multiscale/Couplings.h"
#include "multiscale/ExternalSolvers.h"
#include "multiscale/Microstructure.h"
#include "multiscale/ModelCards.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <mutex>

namespace resonant::services {

using nlohmann::json;
namespace ms = multiscale;

namespace {

void audit(Session& s, const CallContext& c, const std::string& operation, const std::vector<std::string>& resources,
           const std::optional<ServiceError>& error = {})
{
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = resources;
    r.revisionBefore = r.revisionAfter = s.projectRevision();
    r.succeeded = !error;
    r.error = error;
    s.record(std::move(r));
}

Outcome<ms::MultiscaleModel> modelOf(const Project& p)
{
    auto m = ms::modelOf(p);
    if (!m)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's multiscale model cannot be read: " + m.message());
    return m.take();
}

json staleJson(const std::vector<ms::StaleProperty>& stale)
{
    json out = json::array();
    for (const auto& s : stale)
        out.push_back({{"material", s.material}, {"domain", s.domain}, {"key", s.key}, {"derivation", s.derivation}, {"reasons", s.reasons}});
    return out;
}

class MultiscaleJob final : public Job {
public:
    using Work = std::function<Result<json>(const Project&, JobContext&)>;
    MultiscaleJob(std::string name, std::shared_ptr<const Project> project, Work work)
        : name_(std::move(name)), project_(std::move(project)), work_(std::move(work))
    {
    }
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

// The derivation document of a result (a coupling result carries its final derivation).
const json* derivationOf(const json& doc)
{
    if (doc.value("format", "") == coupling::kDerivationFormat)
        return &doc;
    if (doc.contains("derivation") && doc["derivation"].is_object())
        return &doc["derivation"];
    return nullptr;
}

// Why a derivation document no longer describes the project.
std::vector<std::string> documentStaleness(const json& d, const Project& p, const ms::MultiscaleModel& m)
{
    std::vector<std::string> reasons;
    const json rep = d.value("representation", json::object());
    const ms::Representation* r = m.find(rep.value("id", ""));
    if (!r)
        reasons.push_back("the representation '" + rep.value("id", std::string()) + "' was removed");
    else if (r->revision() != rep.value("revision", ""))
        reasons.push_back("the representation '" + r->name + "' changed");
    const json keys = d.value("constituentKeys", json::object());  // items() must not outlive its object
    for (const auto& [id, key] : keys.items()) {
        const Material* c = p.materials.find(Id(id));
        if (!c)
            reasons.push_back("the constituent '" + id + "' was removed");
        else if (ms::materialContentKey(*c) != key.get<std::string>())
            reasons.push_back("the constituent '" + c->name() + "' changed");
    }
    return reasons;
}

}  // namespace

Outcome<Mutation> MultiscaleService::changeModel(const CallContext& c, const std::string& operation, const std::string& description,
                                                 const std::function<Outcome<bool>(Project&, ms::MultiscaleModel&)>& change)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    auto model = modelOf(changed);
    if (!model)
        return model.error();
    ms::MultiscaleModel m = model.take();
    auto ok = change(changed, m);
    if (!ok)
        return ok.error();
    if (const auto issues = ms::validateModel(m, changed); !issues.empty())
        return invalid("the multiscale model would not be valid: " + issues.front(), "definition");
    ms::storeModel(changed, m);
    return s_.applyCopy(c, operation, changed, description);
}

Outcome<json> MultiscaleService::model(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    json d = m.value().toJson();
    d["issues"] = ms::validateModel(m.value(), s_.project());
    d["staleProperties"] = staleJson(ms::staleProperties(m.value(), s_.project()));
    return d;
}

Outcome<std::vector<json>> MultiscaleService::representations(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    std::vector<json> out;
    for (const auto& r : m.value().representations)
        out.push_back(r.toJson());
    return out;
}

Outcome<json> MultiscaleService::representation(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    const ms::Representation* r = m.value().find(id);
    if (!r)
        return notFound("there is no representation '" + id + "'", "representation", id);
    json d = r->toJson();
    if (const ms::ModelCard* card = ms::findModelCard(r->modelCard))
        d["modelCardDocument"] = card->toJson();
    json rels = json::array();
    const std::string ref = "representation:" + id;
    for (const auto& rel : m.value().relationships)
        if (rel.from == ref || rel.to == ref)
            rels.push_back({{"from", rel.from}, {"to", rel.to}, {"kind", rel.kind}});
    d["relationships"] = rels;
    json methods = json::array();
    for (const auto& method : coupling::derivationMethods()) {
        const auto issues = coupling::applicability(method, *r, s_.project(), json::object());
        methods.push_back({{"id", method.id}, {"applicable", issues.empty()}, {"issues", issues}});
    }
    d["methods"] = methods;
    return d;
}

Outcome<Mutation> MultiscaleService::setRepresentation(const CallContext& c, const json& document)
{
    auto parsed = ms::Representation::fromJson(document);
    if (!parsed)
        return invalid(parsed.message(), "definition");
    ms::Representation r = parsed.take();
    if (r.modelCard.empty())
        r.modelCard = "representation." + r.kind;
    return changeModel(c, "MultiscaleService.SetRepresentation", "Set representation " + r.name, [r](Project&, ms::MultiscaleModel& m) -> Outcome<bool> {
        bool replaced = false;
        for (auto& x : m.representations)
            if (x.id == r.id) {
                x = r;
                replaced = true;
            }
        if (!replaced)
            m.representations.push_back(r);
        // The representation describes its material.
        if (!r.materialId.empty()) {
            const ms::Relationship rel{"representation:" + r.id, "material:" + r.materialId, "describes"};
            if (std::find(m.relationships.begin(), m.relationships.end(), rel) == m.relationships.end())
                m.relationships.push_back(rel);
        }
        for (const std::string& cst : r.constituents) {
            const ms::Relationship rel{"material:" + cst, "representation:" + r.id, "constituent-of"};
            if (std::find(m.relationships.begin(), m.relationships.end(), rel) == m.relationships.end())
                m.relationships.push_back(rel);
        }
        return true;
    });
}

Outcome<Mutation> MultiscaleService::deleteRepresentation(const CallContext& c, const std::string& id)
{
    return changeModel(c, "MultiscaleService.DeleteRepresentation", "Delete representation " + id, [id](Project&, ms::MultiscaleModel& m) -> Outcome<bool> {
        const auto before = m.representations.size();
        m.representations.erase(std::remove_if(m.representations.begin(), m.representations.end(), [&](const auto& r) { return r.id == id; }),
                                m.representations.end());
        if (m.representations.size() == before)
            return notFound("there is no representation '" + id + "'", "representation", id);
        const std::string ref = "representation:" + id;
        m.relationships.erase(std::remove_if(m.relationships.begin(), m.relationships.end(), [&](const auto& r) { return r.from == ref || r.to == ref; }),
                              m.relationships.end());
        // Derivation records stay: the properties they gave are now stale (their source was removed).
        return true;
    });
}

Outcome<Mutation> MultiscaleService::generateMicrostructure(const CallContext& c, const json& request)
{
    if (!request.is_object())
        return invalid("the request must be an object", "request");
    const json spec = request.value("rve", json::object());
    auto rve = ms::generateRve(spec);
    if (!rve)
        return invalid(rve.message(), "rve");
    json phaseMaterials = request.value("phaseMaterials", json::array());
    if (!phaseMaterials.is_array() || phaseMaterials.size() != rve.value().phases.size())
        return invalid("one phase material per phase", "phaseMaterials");
    std::vector<std::string> constituents;
    const auto add = [&](const std::string& id) {
        if (!id.empty() && std::find(constituents.begin(), constituents.end(), id) == constituents.end())
            constituents.push_back(id);
    };
    for (const json& p : phaseMaterials) {
        if (p.is_string())
            add(p.get<std::string>());
        else if (p.is_object()) {
            add(p.value("axial", ""));
            add(p.value("transverse", ""));
        }
    }
    ms::Representation r;
    r.id = request.value("id", "rve-" + Id::generate().str().substr(0, 8));
    r.name = request.value("name", r.id);
    r.description = request.value("description", "");
    r.scale = ms::Scale::Microstructure;
    r.kind = "rve.voxel";
    r.materialId = request.value("materialId", "");
    r.data = {{"rve", spec}, {"phaseMaterials", phaseMaterials}, {"temperatureLaws", request.value("temperatureLaws", json::array())}};
    r.constituents = constituents;
    r.provenance = rve.value().generator == "voxels" ? "imported" : "synthetic";
    r.source = rve.value().generator == "voxels" ? request.value("source", std::string("imported voxel data"))
                                                  : "procedural (" + rve.value().generator + ", seed " + std::to_string(spec.value("seed", 1)) + "): not a measured microstructure";
    r.assumptions = {"each voxel holds one phase", "phases are continua with their library properties", "perfect contact between phases"};
    r.boundaryConditions = {{"periodic", rve.value().periodic}};
    r.modelCard = "representation.rve.voxel";
    r.uncertainty = "the statistics of one generator; spread over realizations from the derivations' size studies";
    return setRepresentation(c, r.toJson());
}

Outcome<Mutation> MultiscaleService::createAtomisticStructure(const CallContext& c, const json& request)
{
    if (!request.is_object())
        return invalid("the request must be an object", "request");
    Result<atomistic::Crystal> crystal = Result<atomistic::Crystal>::failure("no structure");
    std::string provenance = "user", source;
    if (request.contains("text")) {
        crystal = atomistic::parseStructure(request.value("text", ""), request.value("format", ""));
        provenance = "imported";
        source = request.value("source", "imported " + request.value("format", std::string()));
    } else {
        crystal = atomistic::cubicCrystal(request.value("structure", ""), request.value("latticeParameter", 0.0), request.value("species", ""));
        source = "built: " + request.value("structure", std::string()) + " lattice";
    }
    if (!crystal)
        return invalid(crystal.message(), request.contains("text") ? "text" : "structure");
    const std::string field = request.value("forceField", "");
    std::vector<std::string> assumptions = {"classical point atoms", "ideal structure as given"};
    if (!field.empty()) {
        const atomistic::ForceField* f = atomistic::findForceField(field);
        if (!f)
            return invalid("unknown force field '" + field + "'", "forceField");
        if (const auto bad = atomistic::incompatibleSpecies(*f, crystal.value()); !bad.empty() && !request.value("allowIncompatible", false))
            return invalid("the force field '" + field + "' does not cover the species '" + bad.front() +
                               "': choose another force field (or pass allowIncompatible to store the structure anyway)",
                           "forceField");
    }
    ms::Representation r;
    r.id = request.value("id", "atoms-" + Id::generate().str().substr(0, 8));
    r.name = request.value("name", crystal.value().name);
    r.description = request.value("description", "");
    r.scale = ms::Scale::Atomistic;
    r.kind = crystal.value().periodic ? "crystal" : "atomic-structure";
    r.materialId = request.value("materialId", "");
    r.data = {{"structure", crystal.value().toJson()}, {"forceField", field}};
    r.provenance = provenance;
    r.source = source;
    r.assumptions = assumptions;
    r.boundaryConditions = {{"periodic", crystal.value().periodic}};
    r.modelCard = "representation." + r.kind;
    r.state.temperatureK = 0.0;
    return setRepresentation(c, r.toJson());
}

Outcome<json> MultiscaleService::methods(const CallContext& c, const std::string& representationId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    const ms::Representation* r = nullptr;
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    if (!representationId.empty()) {
        r = m.value().find(representationId);
        if (!r)
            return notFound("there is no representation '" + representationId + "'", "representation", representationId);
    }
    for (const auto& method : coupling::derivationMethods()) {
        json j = method.toJson();
        if (r) {
            const auto issues = coupling::applicability(method, *r, s_.project(), json::object());
            j["applicable"] = issues.empty();
            j["issues"] = issues;
        }
        out.push_back(std::move(j));
    }
    return out;
}

Outcome<json> MultiscaleService::modelCards(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& card : ms::modelCards())
        out.push_back(card.toJson());
    return out;
}

Outcome<json> MultiscaleService::couplings(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& k : ms::couplingContracts())
        out.push_back(k.toJson());
    return out;
}

Outcome<json> MultiscaleService::externalSolvers(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const auto& x : ms::externalSolvers())
        out.push_back(x.toJson());
    return out;
}

Outcome<JobView> MultiscaleService::startJob(const CallContext& c, const std::string& operation, const std::string& name, const std::string& kind,
                                             std::function<Result<json>(const Project&, JobContext&)> work, const std::vector<std::string>& resources)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    std::shared_ptr<const Project> snapshot(std::move(copy.value().project));
    auto job = std::make_shared<MultiscaleJob>(name, snapshot, std::move(work));
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

Outcome<JobView> MultiscaleService::derive(const CallContext& c, const std::string& representationId, const std::string& method, const json& settings)
{
    if (auto a = s_.authorize(c, Permission::Execute); !a)
        return a.error();
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    const ms::Representation* r = m.value().find(representationId);
    if (!r)
        return notFound("there is no representation '" + representationId + "'", "representation", representationId);
    const coupling::DerivationMethod* dm = coupling::findDerivationMethod(method);
    if (!dm)
        return invalid(r->scale == ms::Scale::Electronic || r->scale == ms::Scale::Molecular
                           ? "no " + std::string(ms::toString(r->scale)) + "-scale calculation is integrated in this build (see the external solvers)"
                           : "unknown derivation method '" + method + "'",
                       "method");
    if (const auto issues = coupling::applicability(*dm, *r, s_.project(), settings); !issues.empty())
        return invalid("the method does not apply: " + issues.front(), "method");
    const ms::Representation rep = *r;
    return startJob(c, "MultiscaleService.Derive", dm->title + ": " + rep.name, kDerivationKind,
                    [rep, method, settings](const Project& p, JobContext& ctx) { return coupling::derive(rep, p, method, settings, &ctx); },
                    {representationId});
}

Outcome<json> MultiscaleService::result(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || (entry->kind != kDerivationKind && entry->kind != kCouplingKind) || !entry->document)
        return notFound("there is no multiscale result '" + resultId + "'", "result", resultId);
    json d = *entry->document;
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    std::vector<std::string> reasons;
    if (const json* drv = derivationOf(d))
        reasons = documentStaleness(*drv, s_.project(), m.value());
    d["stale"] = !reasons.empty();
    d["staleReasons"] = reasons;
    d["resultId"] = resultId;
    return d;
}

Outcome<Mutation> MultiscaleService::assign(const CallContext& c, const std::string& resultId, const json& options)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || (entry->kind != kDerivationKind && entry->kind != kCouplingKind) || !entry->document)
        return notFound("there is no multiscale result '" + resultId + "'", "result", resultId);
    const json* drv = derivationOf(*entry->document);
    if (!drv)
        return invalid("the result holds no derivation", "resultId");
    const json derivation = *drv;
    const std::string calculation = "result " + resultId + " (job " + entry->jobId + ")";
    return changeModel(c, "MultiscaleService.AssignDerivedProperties", "Assign derived properties",
                       [derivation, options, calculation](Project& p, ms::MultiscaleModel& m) -> Outcome<bool> {
                           if (const auto reasons = documentStaleness(derivation, p, m); !reasons.empty())
                               return ServiceError::make(ErrorCode::FailedPrecondition,
                                                         "the result is stale (" + reasons.front() + "): derive again before assigning it");
                           auto plan = coupling::planAssignment(derivation, p, m, options, calculation);
                           if (!plan)
                               return invalid(plan.message(), "options");
                           const coupling::Assignment& a = plan.value();
                           MaterialLibrary library = p.materials;
                           Status st = a.created ? library.add(a.material) : library.update(a.material);
                           if (!st)
                               return invalid(st.message(), "options");
                           p.materials = std::move(library);
                           m.derivations.push_back(a.derivation);
                           for (const auto& rel : a.relationships)
                               if (std::find(m.relationships.begin(), m.relationships.end(), rel) == m.relationships.end())
                                   m.relationships.push_back(rel);
                           return true;
                       });
}

Outcome<json> MultiscaleService::lineage(const CallContext& c, const std::string& materialId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto m = modelOf(s_.project());
    if (!m)
        return m.error();
    json l = ms::lineageOf(m.value(), s_.project(), materialId);
    if (l.is_null())
        return notFound("the project has no material '" + materialId + "'", "material", materialId);
    return l;
}

Outcome<json> MultiscaleService::localState(const CallContext& c, const std::string& systemId, const std::string& region, const std::string& resultId)
{
    if (auto a = s_.authorize(c, resultId.empty() ? Permission::Execute : Permission::Read); !a)
        return a.error();
    nlohmann::json systemDoc;
    for (const json& j : s_.project().extensions.value("multiphysics", json::object()).value("systems", json::array()))
        if (j.value("id", "") == systemId)
            systemDoc = j;
    if (systemDoc.is_null())
        return notFound("there is no physical system '" + systemId + "'", "physical-system", systemId);
    auto system = multiphysics::PhysicalSystem::fromJson(systemDoc);
    if (!system)
        return invalid(system.message(), "system");
    Result<coupling::LocalState> state = Result<coupling::LocalState>::failure("");
    if (!resultId.empty()) {
        const ResultEntry* entry = s_.result(resultId);
        if (!entry || entry->kind != "multiphysics.coupled" || !entry->document)
            return notFound("there is no coupled result '" + resultId + "'", "result", resultId);
        state = coupling::regionState(system.value(), *entry->document, region);
    } else {
        auto run = coupling::runPhysicalSystem(system.value(), s_.project().materials);
        if (!run)
            return invalid(run.message(), "system");
        state = coupling::regionState(system.value(), run.value(), region);
    }
    if (!state)
        return invalid(state.message(), "region");
    json d = state.value().toJson();
    d["system"] = systemId;
    d["source"] = resultId.empty() ? "a run of the system now" : "the coupled result " + resultId;
    d["transfer"] = "the region's mean temperature configures a lower-scale calculation (rve.* with temperatureK); stress, fields and "
                    "composition are not transferred";
    return d;
}

Outcome<JobView> MultiscaleService::iterate(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Execute); !a)
        return a.error();
    const std::string systemId = spec.value("system", "");
    nlohmann::json systemDoc;
    for (const json& j : s_.project().extensions.value("multiphysics", json::object()).value("systems", json::array()))
        if (j.value("id", "") == systemId)
            systemDoc = j;
    if (systemDoc.is_null())
        return notFound("there is no physical system '" + systemId + "'", "physical-system", systemId);
    auto system = multiphysics::PhysicalSystem::fromJson(systemDoc);
    if (!system)
        return invalid(system.message(), "system");
    coupling::IterationSettings st;
    st.region = spec.value("region", "");
    st.representation = spec.value("representation", "");
    st.derive = spec.value("derive", json::object());
    st.tolerance = spec.value("tolerance", 1e-4);
    st.maxIterations = spec.value("maxIterations", 20);
    const multiphysics::PhysicalSystem sys = system.value();
    return startJob(c, "MultiscaleService.IterateRegion", "Iterative coupling of " + st.region, kCouplingKind,
                    [sys, st](const Project& p, JobContext& ctx) { return coupling::iterateRegion(sys, p, st, &ctx); }, {systemId, st.representation});
}

}  // namespace resonant::services
