#include "laboratory/Laboratory.h"

#include "laboratory/Sha256.h"
#include "project/Project.h"

#include <algorithm>
#include <set>

namespace resonant::laboratory {

using nlohmann::json;

const std::vector<std::string>& modelLinkKinds()
{
    static const std::vector<std::string> k = {"physical-system", "functional-structure", "material", "material-state-study", "design-revision",
                                               "multiscale-representation", "scene-object", "accounting-study"};
    return k;
}

const std::vector<std::string>& studyStatuses()
{
    static const std::vector<std::string> s = {"planned", "measured", "compared", "calibrated", "validated", "inconclusive"};
    return s;
}

json ModelLink::toJson() const
{
    return {{"kind", kind}, {"id", id}, {"revision", revision}, {"label", label}, {"note", note}};
}

Result<ModelLink> ModelLink::fromJson(const json& j)
{
    using R = Result<ModelLink>;
    if (!j.is_object())
        return R::failure("a model link is an object");
    ModelLink m;
    m.kind = j.value("kind", std::string());
    m.id = j.value("id", std::string());
    if (std::find(modelLinkKinds().begin(), modelLinkKinds().end(), m.kind) == modelLinkKinds().end())
        return R::failure("unknown model link kind '" + m.kind + "'");
    if (m.id.empty())
        return R::failure("a model link needs the id of what it links");
    m.revision = j.value("revision", std::string());
    m.label = j.value("label", std::string());
    m.note = j.value("note", std::string());
    return m;
}

json Deviation::toJson() const
{
    return {{"id", id}, {"parameter", parameter}, {"unit", unit}, {"nominal", nominal}, {"measured", measured}, {"uncertainty", uncertainty},
            {"method", method}, {"source", source}, {"note", note}};
}

Result<Deviation> Deviation::fromJson(const json& j)
{
    if (!j.is_object() || j.value("parameter", std::string()).empty())
        return Result<Deviation>::failure("a deviation names its parameter");
    Deviation d;
    d.id = j.value("id", std::string());
    d.parameter = j.value("parameter", std::string());
    d.unit = j.value("unit", std::string());
    d.nominal = j.value("nominal", 0.0);
    d.measured = j.value("measured", 0.0);
    d.uncertainty = j.value("uncertainty", 0.0);
    d.method = j.value("method", std::string());
    d.source = j.value("source", std::string());
    d.note = j.value("note", std::string());
    return d;
}

std::string_view toString(DatasetRole r)
{
    switch (r) {
    case DatasetRole::Raw:
        return "raw";
    case DatasetRole::Calibration:
        return "calibration";
    case DatasetRole::Validation:
        return "validation";
    case DatasetRole::Holdout:
        return "holdout";
    case DatasetRole::Reference:
        return "reference";
    case DatasetRole::Environment:
        return "environment";
    }
    return "raw";
}

std::optional<DatasetRole> datasetRoleFromString(std::string_view s)
{
    for (DatasetRole r : {DatasetRole::Raw, DatasetRole::Calibration, DatasetRole::Validation, DatasetRole::Holdout, DatasetRole::Reference,
                          DatasetRole::Environment})
        if (toString(r) == s)
            return r;
    return std::nullopt;
}

std::vector<std::string> Study::datasetsWith(DatasetRole role) const
{
    std::vector<std::string> out;
    for (const DatasetUse& d : datasets)
        if (d.role == role)
            out.push_back(d.measurementId);
    return out;
}

Status Study::validate() const
{
    if (id.empty())
        return Status::failure("a study needs an id");
    if (std::find(studyStatuses().begin(), studyStatuses().end(), status) == studyStatuses().end())
        return Status::failure("unknown study status '" + status + "'");
    // A dataset may not be both calibration and validation data (RES-LAB-010).
    const auto cal = datasetsWith(DatasetRole::Calibration);
    for (const std::string& v : datasetsWith(DatasetRole::Validation))
        if (std::find(cal.begin(), cal.end(), v) != cal.end())
            return Status::failure("the dataset '" + v + "' is both calibration and validation data: validation needs independent data");
    std::set<std::string> ids;
    for (const Deviation& d : deviations)
        if (!d.id.empty() && !ids.insert(d.id).second)
            return Status::failure("the deviation '" + d.id + "' is listed twice");
    return Status::success();
}

json Study::toJson() const
{
    json j{{"id", id}, {"name", name}, {"objective", objective}, {"hypothesis", hypothesis}, {"domain", domain}, {"experimentId", experimentId},
           {"specimen", specimen}, {"operator", operatorName}, {"created", created}, {"status", status}, {"conditions", conditions}, {"notes", notes}};
    j["models"] = json::array();
    for (const ModelLink& m : models)
        j["models"].push_back(m.toJson());
    j["deviations"] = json::array();
    for (const Deviation& d : deviations)
        j["deviations"].push_back(d.toJson());
    j["datasets"] = json::array();
    for (const DatasetUse& d : datasets)
        j["datasets"].push_back({{"measurement", d.measurementId}, {"role", std::string(toString(d.role))}, {"note", d.note}});
    return j;
}

Result<Study> Study::fromJson(const json& j)
{
    using R = Result<Study>;
    if (!j.is_object())
        return R::failure("a study is an object");
    Study s;
    s.id = j.value("id", std::string());
    s.name = j.value("name", s.id);
    s.objective = j.value("objective", std::string());
    s.hypothesis = j.value("hypothesis", std::string());
    s.domain = j.value("domain", std::string());
    s.experimentId = j.value("experimentId", std::string());
    s.specimen = j.value("specimen", std::string());
    s.operatorName = j.value("operator", std::string());
    s.created = j.value("created", std::string());
    s.status = j.value("status", std::string("planned"));
    s.conditions = j.value("conditions", json::object());
    s.notes = j.value("notes", std::vector<std::string>{});
    for (const json& m : j.value("models", json::array())) {
        auto l = ModelLink::fromJson(m);
        if (!l)
            return R::failure(l.message());
        s.models.push_back(l.take());
    }
    for (const json& d : j.value("deviations", json::array())) {
        auto v = Deviation::fromJson(d);
        if (!v)
            return R::failure(v.message());
        s.deviations.push_back(v.take());
    }
    for (const json& d : j.value("datasets", json::array())) {
        auto role = datasetRoleFromString(d.value("role", std::string("raw")));
        if (!role)
            return R::failure("unknown dataset role '" + d.value("role", std::string()) + "'");
        s.datasets.push_back({d.value("measurement", std::string()), *role, d.value("note", std::string())});
    }
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    return s;
}

Study* Workspace::study(const std::string& studyId)
{
    for (Study& s : studies)
        if (s.id == studyId)
            return &s;
    return nullptr;
}

const Study* Workspace::study(const std::string& studyId) const
{
    for (const Study& s : studies)
        if (s.id == studyId)
            return &s;
    return nullptr;
}

const json* Workspace::record(const std::string& section, const std::string& recordId) const
{
    const json* list = section == "imports" ? &imports : section == "comparisons" ? &comparisons : section == "calibrations" ? &calibrations
                       : section == "validations"                                    ? &validations
                                                                                     : nullptr;
    if (!list)
        return nullptr;
    for (const json& r : *list)
        if (r.value("id", std::string()) == recordId)
            return &r;
    return nullptr;
}

json Workspace::toJson() const
{
    json j{{"format", "resonant.laboratory"}, {"version", 1}, {"imports", imports}, {"comparisons", comparisons}, {"calibrations", calibrations},
           {"validations", validations}, {"sources", sources}};
    j["studies"] = json::array();
    for (const Study& s : studies)
        j["studies"].push_back(s.toJson());
    return j;
}

Result<Workspace> Workspace::fromJson(const json& j)
{
    using R = Result<Workspace>;
    Workspace w;
    if (j.is_null() || (j.is_object() && j.empty()))
        return w;
    if (!j.is_object() || j.value("format", std::string()) != "resonant.laboratory")
        return R::failure("the laboratory extension is not a resonant.laboratory document");
    std::set<std::string> ids;
    for (const json& s : j.value("studies", json::array())) {
        auto st = Study::fromJson(s);
        if (!st)
            return R::failure(st.message());
        if (!ids.insert(st.value().id).second)
            return R::failure("the study '" + st.value().id + "' is defined twice");
        w.studies.push_back(st.take());
    }
    for (const char* key : {"imports", "comparisons", "calibrations", "validations", "sources"}) {
        const json v = j.value(key, json::array());
        if (!v.is_array())
            return R::failure(std::string("the laboratory's ") + key + " is not a list");
    }
    w.imports = j.value("imports", json::array());
    w.comparisons = j.value("comparisons", json::array());
    w.calibrations = j.value("calibrations", json::array());
    w.validations = j.value("validations", json::array());
    w.sources = j.value("sources", json::array());
    return w;
}

Result<Workspace> workspaceOf(const Project& project)
{
    if (!project.extensions.contains(kExtension))
        return Workspace{};
    return Workspace::fromJson(project.extensions[kExtension]);
}

void storeWorkspace(Project& project, const Workspace& workspace)
{
    if (workspace.studies.empty() && workspace.imports.empty() && workspace.comparisons.empty() && workspace.calibrations.empty() &&
        workspace.validations.empty() && workspace.sources.empty()) {
        project.extensions.erase(kExtension);
        return;
    }
    project.extensions[kExtension] = workspace.toJson();
}

std::string fingerprint(const json& content)
{
    return sha256Hex(content.dump()).substr(0, 16);
}

}  // namespace resonant::laboratory
