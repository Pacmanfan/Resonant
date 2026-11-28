// The multiphysics application services (REQ-CHAT-20261126-016, RES-CEME-015).
#include "services/Services.h"

#include "coupling/CoupledRun.h"
#include "coupling/CouplingModels.h"
#include "project/ProjectSerializer.h"

namespace resonant::services {
namespace {

void auditRun(Session& s, const CallContext& c, const std::string& operation, const std::vector<std::string>& resources,
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

nlohmann::json storedSystems(const Project& project)
{
    const auto it = project.extensions.find(MultiphysicsService::kExtension);
    if (it == project.extensions.end() || !it->is_object())
        return nlohmann::json::array();
    return it->value("systems", nlohmann::json::array());
}

}  // namespace

Outcome<std::vector<multiphysics::CouplingModelDescriptor>> MultiphysicsService::couplingModels(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    return coupling::couplingModels();
}

Outcome<std::vector<nlohmann::json>> MultiphysicsService::systems(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    std::vector<nlohmann::json> out;
    for (const auto& j : storedSystems(s_.project()))
        out.push_back(j);
    return out;
}

Outcome<nlohmann::json> MultiphysicsService::system(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    for (const auto& j : storedSystems(s_.project()))
        if (j.value("id", "") == id)
            return j;
    return notFound("there is no physical system '" + id + "'", "physical-system", id);
}

Outcome<Mutation> MultiphysicsService::setSystem(const CallContext& c, const nlohmann::json& document)
{
    auto parsed = multiphysics::PhysicalSystem::fromJson(document);
    if (!parsed)
        return invalid(parsed.message(), "definition");
    if (parsed.value().id.empty())
        return invalid("the physical system needs an id", "definition.id");
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    nlohmann::json systemsJson = storedSystems(changed);
    const nlohmann::json canonical = parsed.value().toJson();
    bool replaced = false;
    for (auto& j : systemsJson)
        if (j.value("id", "") == parsed.value().id) {
            j = canonical;
            replaced = true;
        }
    if (!replaced)
        systemsJson.push_back(canonical);
    changed.extensions[kExtension] = {{"format", "resonant.multiphysics"}, {"version", 1}, {"systems", systemsJson}};
    const std::string name = parsed.value().name.empty() ? parsed.value().id : parsed.value().name;
    return s_.applyCopy(c, "MultiphysicsService.SetPhysicalSystem", changed, (replaced ? "Change physical system " : "Add physical system ") + name);
}

Outcome<Mutation> MultiphysicsService::deleteSystem(const CallContext& c, const std::string& id)
{
    nlohmann::json systemsJson = storedSystems(s_.project());
    nlohmann::json kept = nlohmann::json::array();
    for (const auto& j : systemsJson)
        if (j.value("id", "") != id)
            kept.push_back(j);
    if (kept.size() == systemsJson.size())
        return notFound("there is no physical system '" + id + "'", "physical-system", id);
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    changed.extensions[kExtension] = {{"format", "resonant.multiphysics"}, {"version", 1}, {"systems", kept}};
    return s_.applyCopy(c, "MultiphysicsService.DeletePhysicalSystem", changed, "Delete physical system " + id);
}

Outcome<multiphysics::PhysicalSystem> MultiphysicsService::resolve(const std::string& id, const nlohmann::json& document)
{
    nlohmann::json source = document;
    if (source.is_null() || source.empty()) {
        bool found = false;
        for (const auto& j : storedSystems(s_.project()))
            if (j.value("id", "") == id) {
                source = j;
                found = true;
            }
        if (!found)
            return notFound("there is no physical system '" + id + "'", "physical-system", id);
    }
    auto parsed = multiphysics::PhysicalSystem::fromJson(source);
    if (!parsed)
        return invalid(parsed.message(), "definition");
    return parsed.take();
}

Outcome<ValidationView> MultiphysicsService::validate(const CallContext& c, const std::string& id, const nlohmann::json& document)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto system = resolve(id, document);
    if (!system)
        return system.error();
    ValidationView v;
    for (const auto& issue : coupling::validatePhysicalSystem(system.value(), s_.project().materials)) {
        v.issues.push_back({issue.severity, issue.field, issue.message, system.value().id});
        if (issue.severity >= 3)
            v.valid = false;
    }
    return v;
}

Outcome<JobView> MultiphysicsService::start(const CallContext& c, const std::string& id, const nlohmann::json& document)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto system = resolve(id, document);
    if (!system) {
        auditRun(s_, a.value(), "MultiphysicsService.StartCoupledSimulation", {id}, system.error());
        return system.error();
    }
    std::string errors;
    for (const auto& issue : coupling::validatePhysicalSystem(system.value(), s_.project().materials))
        if (issue.severity >= 3)
            errors += (errors.empty() ? "" : "; ") + issue.field + ": " + issue.message;
    if (!errors.empty()) {
        ServiceError e = invalid("the physical system is not valid: " + errors, "definition");
        auditRun(s_, a.value(), "MultiphysicsService.StartCoupledSimulation", {system.value().id}, e);
        return e;
    }
    auto job = std::make_shared<coupling::CoupledSimulationJob>(system.value(), s_.project().materials);
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "multiphysics.coupled";
    record.sessionId = a.value().sessionId;
    record.onFinished = [job, revision](Session& session, const JobInfo& info) {
        const auto result = job->result();
        if (!result)
            return;
        ResultEntry entry;
        entry.kind = "multiphysics.coupled";
        entry.name = job->system().name.empty() ? job->system().id : job->system().name;
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        nlohmann::json doc = result->toJson();
        doc["projectRevision"] = revision;
        doc["jobId"] = info.id.str();
        entry.document = std::make_shared<const nlohmann::json>(std::move(doc));
        const std::string resultId = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(resultId);
    };
    s_.trackJob(std::move(record));
    auditRun(s_, a.value(), "MultiphysicsService.StartCoupledSimulation", {system.value().id, jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<std::shared_ptr<const nlohmann::json>> MultiphysicsService::result(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || entry->kind != "multiphysics.coupled" || !entry->document)
        return notFound("there is no coupled result '" + resultId + "'", "result", resultId);
    return entry->document;
}

}  // namespace resonant::services
