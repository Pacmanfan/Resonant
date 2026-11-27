#include "optimization/Evaluator.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace resonant::optimization {

namespace {
constexpr std::pair<Fidelity, std::string_view> kFidelities[] = {
    {Fidelity::Analytical, "analytical"}, {Fidelity::Reduced, "reduced"}, {Fidelity::HighFidelity, "high-fidelity"}, {Fidelity::Experimental, "experimental"}};
constexpr std::pair<Evaluation::Status, std::string_view> kStatuses[] = {{Evaluation::Status::Ok, "ok"},
                                                                         {Evaluation::Status::InvalidDesign, "invalid-design"},
                                                                         {Evaluation::Status::Refused, "refused"},
                                                                         {Evaluation::Status::Failed, "failed"},
                                                                         {Evaluation::Status::Cancelled, "cancelled"}};

nlohmann::json number(double v) { return std::isfinite(v) ? nlohmann::json(v) : nlohmann::json(nullptr); }
}  // namespace

std::string_view toString(Fidelity fidelity)
{
    for (const auto& [f, s] : kFidelities)
        if (f == fidelity)
            return s;
    return "analytical";
}

std::optional<Fidelity> fidelityFromString(std::string_view text)
{
    for (const auto& [f, s] : kFidelities)
        if (s == text)
            return f;
    return std::nullopt;
}

std::string_view toString(Evaluation::Status status)
{
    for (const auto& [f, s] : kStatuses)
        if (f == status)
            return s;
    return "failed";
}

const QuantityInfo* Capabilities::quantity(std::string_view key) const
{
    for (const QuantityInfo& q : quantities)
        if (q.key == key)
            return &q;
    return nullptr;
}

const ModelInfo* Capabilities::model(std::string_view id) const
{
    for (const ModelInfo& m : models)
        if (m.id == id)
            return &m;
    return nullptr;
}

const DesignVariable* Capabilities::variable(std::string_view binding) const
{
    for (const DesignVariable& v : variables)
        if (v.key() == binding)
            return &v;
    return nullptr;
}

nlohmann::json toJson(const ModelRecord& r)
{
    return {{"modelId", r.modelId},         {"modelName", r.modelName}, {"version", r.version},
            {"fidelity", toString(r.fidelity)}, {"assumptions", r.assumptions}, {"settings", r.settings},
            {"adapterId", r.adapterId},     {"adapterVersion", r.adapterVersion}, {"snapshotKey", r.snapshotKey}};
}

ModelRecord modelRecordFromJson(const nlohmann::json& j)
{
    ModelRecord r;
    r.modelId = j.value("modelId", std::string());
    r.modelName = j.value("modelName", std::string());
    r.version = j.value("version", std::string());
    r.fidelity = fidelityFromString(j.value("fidelity", std::string("analytical"))).value_or(Fidelity::Analytical);
    r.assumptions = j.value("assumptions", std::vector<std::string>());
    r.settings = j.value("settings", nlohmann::json::object());
    r.adapterId = j.value("adapterId", std::string());
    r.adapterVersion = j.value("adapterVersion", std::string());
    r.snapshotKey = j.value("snapshotKey", std::string());
    return r;
}

nlohmann::json toJson(const Evaluation& e)
{
    nlohmann::json q = nlohmann::json::object();
    for (const auto& [k, v] : e.quantities)
        q[k] = number(v);
    nlohmann::json j{{"status", toString(e.status)}, {"quantities", q}, {"model", toJson(e.model)}, {"seconds", e.seconds}};
    if (!e.message.empty())
        j["message"] = e.message;
    if (!e.notes.empty())
        j["notes"] = e.notes;
    if (!e.warnings.empty())
        j["warnings"] = e.warnings;
    if (!e.details.empty())
        j["details"] = e.details;
    return j;
}

Evaluation evaluationFromJson(const nlohmann::json& j)
{
    Evaluation e;
    const std::string status = j.value("status", std::string("failed"));
    e.status = Evaluation::Status::Failed;
    for (const auto& [f, s] : kStatuses)
        if (s == status)
            e.status = f;
    e.message = j.value("message", std::string());
    if (j.contains("quantities"))
        for (const auto& [k, v] : j.at("quantities").items())
            e.quantities[k] = v.is_number() ? v.get<double>() : std::numeric_limits<double>::quiet_NaN();
    if (j.contains("model"))
        e.model = modelRecordFromJson(j.at("model"));
    e.notes = j.value("notes", std::vector<std::string>());
    e.warnings = j.value("warnings", std::vector<std::string>());
    e.details = j.value("details", nlohmann::json::object());
    e.seconds = j.value("seconds", 0.0);
    return e;
}

void AdapterRegistry::add(std::string adapterId, std::string title, Factory factory)
{
    for (Entry& e : entries_)
        if (e.id == adapterId) {
            e.title = std::move(title);
            e.factory = std::move(factory);
            return;
        }
    entries_.push_back({std::move(adapterId), std::move(title), std::move(factory)});
}

void AdapterRegistry::addTemplate(ProblemTemplate problemTemplate) { templates_.push_back(std::move(problemTemplate)); }

bool AdapterRegistry::knows(std::string_view adapterId) const
{
    for (const Entry& e : entries_)
        if (e.id == adapterId)
            return true;
    return false;
}

std::vector<std::pair<std::string, std::string>> AdapterRegistry::adapters() const
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const Entry& e : entries_)
        out.emplace_back(e.id, e.title);
    return out;
}

const ProblemTemplate* AdapterRegistry::findTemplate(std::string_view id) const
{
    for (const ProblemTemplate& t : templates_)
        if (t.id == id)
            return &t;
    return nullptr;
}

Result<std::shared_ptr<const DesignEvaluator>> AdapterRegistry::bind(std::string_view adapterId, const nlohmann::json& projectSnapshot,
                                                                     const nlohmann::json& configuration) const
{
    for (const Entry& e : entries_)
        if (e.id == adapterId)
            return e.factory(projectSnapshot, configuration);
    return Result<std::shared_ptr<const DesignEvaluator>>::failure("no optimization adapter '" + std::string(adapterId) + "'");
}

std::string contentKey(const nlohmann::json& document)
{
    const std::string text = document.dump();
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    char buffer[17];
    std::snprintf(buffer, sizeof buffer, "%016llx", static_cast<unsigned long long>(hash));
    return buffer;
}

}  // namespace resonant::optimization
