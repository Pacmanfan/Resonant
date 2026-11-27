#include "optimization/Persistence.h"

#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <cmath>
#include <sstream>

namespace resonant::optimization {

const OptimizationProblem* OptimizationWorkspace::findProblem(std::string_view id) const
{
    for (const OptimizationProblem& p : problems)
        if (p.id == id)
            return &p;
    return nullptr;
}

OptimizationProblem* OptimizationWorkspace::findProblem(std::string_view id)
{
    for (OptimizationProblem& p : problems)
        if (p.id == id)
            return &p;
    return nullptr;
}

const RunRecord* OptimizationWorkspace::findRun(std::string_view id) const
{
    for (const RunRecord& r : runs)
        if (r.runId == id)
            return &r;
    return nullptr;
}

nlohmann::json toJson(const DesignRevision& r)
{
    return {{"id", r.id},           {"candidate", r.candidateId}, {"run", r.runId},         {"problem", r.problemId},
            {"promotedUtc", r.promotedUtc}, {"model", r.model},   {"fidelity", r.fidelity}, {"validated", r.validated},
            {"values", r.values},   {"previous", r.previous},     {"parents", r.parents},   {"note", r.note}};
}

DesignRevision revisionFromJson(const nlohmann::json& j)
{
    DesignRevision r;
    r.id = j.value("id", std::string());
    r.candidateId = j.value("candidate", std::string());
    r.runId = j.value("run", std::string());
    r.problemId = j.value("problem", std::string());
    r.promotedUtc = j.value("promotedUtc", std::string());
    r.model = j.value("model", std::string());
    r.fidelity = j.value("fidelity", std::string());
    r.validated = j.value("validated", false);
    const nlohmann::json values = j.value("values", nlohmann::json::object());
    for (const auto& [k, v] : values.items())
        if (v.is_number())
            r.values[k] = v.get<double>();
    const nlohmann::json previous = j.value("previous", nlohmann::json::object());
    for (const auto& [k, v] : previous.items())
        if (v.is_number())
            r.previous[k] = v.get<double>();
    r.parents = j.value("parents", std::vector<std::string>());
    r.note = j.value("note", std::string());
    return r;
}

OptimizationWorkspace workspaceOf(const Project& project)
{
    OptimizationWorkspace w;
    if (!project.extensions.contains(kExtensionKey))
        return w;
    const nlohmann::json& j = project.extensions[kExtensionKey];
    if (!j.is_object())
        return w;
    const nlohmann::json problems = j.value("problems", nlohmann::json::array());
    for (const auto& p : problems)
        if (auto v = problemFromJson(p))
            w.problems.push_back(v.take());
    const nlohmann::json runs = j.value("runs", nlohmann::json::array());
    for (const auto& r : runs)
        if (auto v = runRecordFromJson(r))
            w.runs.push_back(v.take());
    const nlohmann::json revisions = j.value("revisions", nlohmann::json::array());
    for (const auto& r : revisions)
        w.revisions.push_back(revisionFromJson(r));
    return w;
}

void storeWorkspace(Project& project, const OptimizationWorkspace& w)
{
    nlohmann::json problems = nlohmann::json::array();
    for (const OptimizationProblem& p : w.problems)
        problems.push_back(toJson(p));
    nlohmann::json runs = nlohmann::json::array();
    for (const RunRecord& r : w.runs)
        runs.push_back(toJson(r));
    nlohmann::json revisions = nlohmann::json::array();
    for (const DesignRevision& r : w.revisions)
        revisions.push_back(toJson(r));
    if (w.problems.empty() && w.runs.empty() && w.revisions.empty()) {
        project.extensions.erase(kExtensionKey);
        return;
    }
    project.extensions[kExtensionKey] = {{"version", kWorkspaceVersion}, {"problems", problems}, {"runs", runs}, {"revisions", revisions}};
}

nlohmann::json designSnapshot(const Project& project)
{
    nlohmann::json j = serializeProject(project);
    if (j.contains("extensions") && j["extensions"].is_object())
        j["extensions"].erase(kExtensionKey);
    return j;
}

nlohmann::json dependencyDocument(const nlohmann::json& snapshot)
{
    nlohmann::json j = snapshot;
    if (j.contains("project") && j["project"].is_object())
        j["project"].erase("modifiedUtc");
    if (j.contains("extensions") && j["extensions"].is_object())
        j["extensions"].erase(kExtensionKey);
    j.erase("application");
    return j;
}

Result<std::unique_ptr<Project>> projectFromSnapshot(const nlohmann::json& snapshot)
{
    auto loaded = deserializeProject(snapshot);
    if (!loaded)
        return Result<std::unique_ptr<Project>>::failure(loaded.message());
    return std::move(loaded.value().project);
}

std::string candidatesCsv(const OptimizationProblem& problem, const std::vector<Candidate>& candidates)
{
    const auto quote = [](std::string s) {
        if (s.find_first_of(",\"\n") == std::string::npos)
            return s;
        std::string out = "\"";
        for (char c : s)
            out += c == '"' ? std::string("\"\"") : std::string(1, c);
        return out + "\"";
    };
    const auto value = [](double v) {
        if (!std::isfinite(v))
            return std::string();
        std::ostringstream s;
        s.precision(12);
        s << v;
        return s.str();
    };
    std::ostringstream out;
    out << "id,origin,model,fidelity,status,feasible";
    for (const DesignVariable& v : problem.space.variables)
        if (v.active)
            out << "," << quote(v.id + (v.unit.empty() ? "" : " [" + v.unit + "]"));
    for (const Objective& o : problem.objectives)
        out << "," << quote(o.id + (o.unit.empty() ? "" : " [" + o.unit + "]"));
    for (const DesignConstraint& c : problem.constraints)
        out << "," << quote(c.id + " violation" + (c.unit.empty() ? "" : " [" + c.unit + "]"));
    out << "\n";
    for (const Candidate& c : candidates) {
        out << quote(c.id) << "," << quote(c.origin) << "," << quote(c.evaluation.model.modelId) << "," << toString(c.evaluation.model.fidelity)
            << "," << toString(c.evaluation.status) << "," << (c.feasible ? "yes" : "no");
        for (const DesignVariable& v : problem.space.variables)
            if (v.active) {
                const auto it = c.point.values.find(v.id);
                out << "," << (it == c.point.values.end() ? std::string() : value(it->second));
            }
        for (size_t i = 0; i < problem.objectives.size(); ++i)
            out << "," << (i < c.objectiveValues.size() ? value(c.objectiveValues[i]) : std::string());
        for (size_t i = 0; i < problem.constraints.size(); ++i)
            out << "," << (i < c.constraints.size() ? value(c.constraints[i].violation) : std::string());
        out << "\n";
    }
    return out.str();
}

}  // namespace resonant::optimization
