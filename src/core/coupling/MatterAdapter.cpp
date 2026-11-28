#include "coupling/MatterAdapter.h"

#include "coupling/MatterAnalyses.h"
#include "matter/Manufacturing.h"
#include "matter/MatterArchitecture.h"
#include "optimization/Persistence.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"

#include <chrono>
#include <cmath>
#include <numbers>
#include <sstream>

namespace resonant::coupling {

namespace opt = optimization;
using nlohmann::json;

namespace {

constexpr const char* kPrefix = "matter/";

std::vector<std::string> split(std::string_view key)
{
    std::vector<std::string> out;
    std::string part;
    std::stringstream s{std::string(key)};
    while (std::getline(s, part, '/'))
        out.push_back(part);
    return out;
}

std::vector<matter::ResidualStress> residualOf(const matter::Architecture& a, const MaterialLibrary& lib)
{
    std::vector<matter::ResidualStress> out;
    for (const json& r : processResidualStresses(a, lib))
        if (r.contains("stress"))
            out.push_back({r["interface"].get<std::string>(), r["layer"].get<std::string>(), r["stress"].get<double>(), r["temperature"].get<double>(),
                           r["basis"].get<std::string>()});
    return out;
}

class MatterAdapter final : public opt::SnapshotAdapter {
public:
    MatterAdapter(json snapshot, json configuration, const Project& project, matter::Architecture architecture)
        : SnapshotAdapter(std::move(snapshot), std::move(configuration)), architecture_(std::move(architecture))
    {
        describe(project);
        finishBinding();
    }

    opt::Evaluation evaluate(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId, JobContext*) const override
    {
        (void)modelId;
        using clock = std::chrono::steady_clock;
        const auto start = clock::now();
        opt::Evaluation e;
        auto project = build(bindings);
        if (!project) {
            e.status = opt::Evaluation::Status::InvalidDesign;
            e.message = project.message();
            return e;
        }
        auto w = matter::workspaceOf(*project.value());
        const matter::Architecture* a = w ? w.value().find(architecture_.id) : nullptr;
        if (!a) {
            e.status = opt::Evaluation::Status::Failed;
            e.message = "the architecture is not in the design";
            return e;
        }
        const std::string stack = configuration_.value("stack", std::string());
        json details = json::object();
        for (const json& item : configuration_.value("analyses", json::array())) {
            const std::string analysis = item.value("analysis", std::string());
            auto r = analyzeStack(*a, stack, analysis, item.value("options", json::object()), project.value()->materials);
            if (!r) {
                e.status = opt::Evaluation::Status::InvalidDesign;
                e.message = analysis + ": " + r.message();
                return e;
            }
            for (const auto& [k, v] : r.value()["metrics"].items())
                if (v.is_number() && std::isfinite(v.get<double>()))
                    e.quantities[analysis + "." + k] = v.get<double>();
            details[analysis] = r.value()["metrics"];
            for (const auto& wv : r.value()["warnings"])
                if (std::find(e.warnings.begin(), e.warnings.end(), wv.get<std::string>()) == e.warnings.end())
                    e.warnings.push_back(wv.get<std::string>());
        }
        const matter::ManufacturingCheck mc = matter::checkManufacturing(*a, residualOf(*a, project.value()->materials));
        e.quantities["manufacturing.violations"] = static_cast<double>(mc.violations.size());
        e.quantities["manufacturing.feasible"] = mc.status == "feasible" ? 1.0 : 0.0;
        details["manufacturing"] = mc.toJson();
        e.model.modelId = "matter.analyses";
        e.model.modelName = "Architecture analyses";
        e.model.version = "1";
        e.model.fidelity = opt::Fidelity::Analytical;
        e.model.assumptions = {"the stack analyses' own assumptions (lamination theory, the lumped cantilever, series conduction, the layered solver)"};
        e.model.adapterId = kMatterAdapterId;
        e.model.adapterVersion = "1";
        e.model.snapshotKey = snapshotKey();
        e.warnings.push_back("an optimized architecture is a prediction, not a validated design");
        e.details = details;
        e.seconds = std::chrono::duration<double>(clock::now() - start).count();
        return e;
    }

protected:
    Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const override
    {
        if (key.substr(0, std::string_view(kPrefix).size()) != kPrefix)
            return false;
        auto w = matter::workspaceOf(project);
        if (!w)
            return Result<bool>::failure(w.message());
        matter::Workspace ws = w.value();
        matter::Architecture* a = ws.find(architecture_.id);
        if (!a)
            return Result<bool>::failure("the architecture is not in the design");
        const auto parts = split(key);
        auto fail = [&](const std::string& why) { return Result<bool>::failure("'" + std::string(key) + "': " + why); };
        if (parts.size() == 5 && parts[1] == "layer") {
            matter::Stack* s = a->stack(parts[2]);
            const size_t index = static_cast<size_t>(std::stoul(parts[3]));
            if (!s || index >= s->layers.size())
                return fail("no such layer");
            matter::Layer& l = s->layers[index];
            if (parts[4] == "thickness") {
                if (!(value > 0.0))
                    return fail("a thickness must be positive");
                l.thickness = value;
            } else if (parts[4] == "angle") {
                l.angle = value;
            } else if (parts[4] == "coverage") {
                if (!(value > l.pattern.start && value <= 1.0))
                    return fail("the coverage must end after its start and within the length");
                l.pattern.kind = "segmented";
                l.pattern.end = value;
            } else {
                return fail("unknown layer quantity");
            }
        } else if (parts.size() == 4 && parts[1] == "region" && parts[3] == "material") {
            matter::FunctionalRegion* r = a->region(parts[2]);
            const json choices = configuration_.value("materialChoices", json::object()).value(parts[2], json::array());
            const long index = std::lround(value);
            if (!r || index < 0 || index >= static_cast<long>(choices.size()))
                return fail("not a material choice of the region");
            r->materialId = choices[static_cast<size_t>(index)].get<std::string>();
        } else if (parts.size() == 5 && parts[1] == "interface") {
            matter::Interface* in = a->interfaceById(parts[2]);
            if (!in)
                return fail("no such interface");
            bool set = false;
            for (auto& m : in->models)
                if (m.kind == parts[3])
                    if (auto it = m.parameters.find(parts[4]); it != m.parameters.end()) {
                        if (!(value >= 0.0))
                            return fail("an interface parameter must not be negative");
                        it->second.value = value;
                        set = true;
                    }
            if (!set)
                return fail("the interface has no such model parameter");
        } else {
            return fail("not an architecture binding");
        }
        matter::storeWorkspace(project, ws);
        return true;
    }

    Status checkDesign(const Project& project) const override
    {
        auto w = matter::workspaceOf(project);
        if (!w)
            return Status::failure(w.message());
        const matter::Architecture* a = w.value().find(architecture_.id);
        if (!a)
            return Status::failure("the architecture is not in the design");
        const auto issues = matter::validate(*a, &project);
        for (const auto& i : issues)
            if (i.severity == "error")
                return Status::failure(i.message);
        if (configuration_.value("manufacturing", std::string("constrain")) == "reject") {
            const matter::ManufacturingCheck mc = matter::checkManufacturing(*a, residualOf(*a, project.materials));
            if (!mc.violations.empty())
                return Status::failure("cannot be manufactured: " + mc.violations.front().message);
        }
        return Status::success();
    }

private:
    void describe(const Project& project)
    {
        (void)project;
        opt::Capabilities& c = capabilities_;
        c.adapterId = kMatterAdapterId;
        c.domain = "matter";
        c.version = "1";
        c.threadSafe = true;
        const std::string stackId = configuration_.value("stack", std::string());
        const matter::Stack* s = architecture_.stack(stackId);
        std::vector<std::string> wanted;
        if (configuration_.contains("variables") && configuration_["variables"].is_array())
            for (const json& v : configuration_["variables"])
                wanted.push_back(v.get<std::string>());
        auto offer = [&](opt::DesignVariable v) {
            if (wanted.empty() || std::find(wanted.begin(), wanted.end(), v.binding) != wanted.end())
                c.variables.push_back(std::move(v));
        };
        if (s)
            for (size_t i = 0; i < s->layers.size(); ++i) {
                const matter::Layer& l = s->layers[i];
                const std::string base = std::string(kPrefix) + "layer/" + s->id + "/" + std::to_string(i) + "/";
                opt::DesignVariable t;
                t.id = l.id + ".thickness";
                t.label = "Thickness of " + l.id;
                t.binding = base + "thickness";
                t.unit = "m";
                t.role = opt::VariableRole::Thickness;
                t.initial = l.thickness;
                t.lower = l.thickness / 4.0;
                t.upper = l.thickness * 4.0;
                t.group = s->id;
                offer(t);
                const matter::FunctionalRegion* r = architecture_.region(l.region);
                if (r && !r->anisotropic.empty()) {
                    opt::DesignVariable ang;
                    ang.id = l.id + ".angle";
                    ang.label = "In-plane angle of " + l.id;
                    ang.binding = base + "angle";
                    ang.unit = "rad";
                    ang.role = opt::VariableRole::Orientation;
                    ang.initial = l.angle;
                    ang.lower = 0.0;
                    ang.upper = std::numbers::pi;
                    ang.group = s->id;
                    offer(ang);
                }
                if (r && (r->anisotropic.count("piezoelectric") || l.pattern.kind == "segmented")) {
                    opt::DesignVariable cov;
                    cov.id = l.id + ".coverage";
                    cov.label = "Electrode coverage of " + l.id;
                    cov.binding = base + "coverage";
                    cov.unit = "";
                    cov.role = opt::VariableRole::Layout;
                    cov.initial = l.pattern.end;
                    cov.lower = std::max(0.1, l.pattern.start + 0.05);
                    cov.upper = 1.0;
                    cov.group = s->id;
                    offer(cov);
                }
            }
        for (const auto& in : architecture_.interfaces)
            for (const auto& m : in.models)
                for (const auto& [key, v] : m.parameters) {
                    opt::DesignVariable p;
                    p.id = in.id + "." + m.kind + "." + key;
                    p.label = key + " of " + in.id + " (" + m.kind + ")";
                    p.binding = std::string(kPrefix) + "interface/" + in.id + "/" + m.kind + "/" + key;
                    p.unit = v.unit;
                    p.role = opt::VariableRole::MaterialParameter;
                    p.initial = v.value;
                    p.lower = v.value / 10.0;
                    p.upper = v.value * 10.0;
                    p.group = "interfaces";
                    if (v.value > 0.0)
                        offer(p);
                }
        const json choices = configuration_.value("materialChoices", json::object());
        for (const auto& [region, list] : choices.items()) {
            const matter::FunctionalRegion* r = architecture_.region(region);
            if (!r || !list.is_array() || list.empty())
                continue;
            opt::DesignVariable m;
            m.id = region + ".material";
            m.label = "Material of " + region;
            m.binding = std::string(kPrefix) + "region/" + region + "/material";
            m.kind = opt::VariableKind::Categorical;
            m.role = opt::VariableRole::MaterialSelection;
            for (const json& id : list)
                m.choices.push_back(id.get<std::string>());
            m.lower = 0.0;
            m.upper = static_cast<double>(m.choices.size() - 1);
            const auto it = std::find(m.choices.begin(), m.choices.end(), r->materialId);
            m.initial = it == m.choices.end() ? 0.0 : static_cast<double>(it - m.choices.begin());
            offer(m);
        }
        for (const json& item : configuration_.value("analyses", json::array())) {
            const std::string analysis = item.value("analysis", std::string());
            const auto& metrics = matterMetrics(analysis);
            for (const auto& q : metrics)
                c.quantities.push_back({analysis + "." + q.key, q.label, q.unit, q.definition, {}, {}, false});
        }
        c.quantities.push_back({"manufacturing.violations", "Manufacturing violations", "", "the number of violated process constraints", {}, {}, false});
        c.quantities.push_back({"manufacturing.feasible", "Manufacturable (1) or not", "", "1 when the stated constraints hold and nothing is unassessed", {}, {}, false});
        c.models.push_back({"matter.analyses", "Architecture analyses", "1", opt::Fidelity::Analytical,
                            {"lamination theory, the lumped piezoelectric cantilever, series conduction and the layered solver, as configured"}, true, {}, 0.001});
        c.limitations = {"every candidate is evaluated on its own copy of the design", "an optimized architecture is a prediction, not a validated design",
                         "manufacturing constraints are those the architecture states"};
    }

    matter::Architecture architecture_;
};

}  // namespace

Result<std::shared_ptr<const opt::DesignEvaluator>> bindMatterAdapter(const json& snapshot, const json& configuration)
{
    using R = Result<std::shared_ptr<const opt::DesignEvaluator>>;
    auto project = opt::projectFromSnapshot(snapshot);
    if (!project)
        return R::failure(project.message());
    auto w = matter::workspaceOf(*project.value());
    if (!w)
        return R::failure(w.message());
    const matter::Architecture* a = w.value().find(configuration.value("architecture", std::string()));
    if (!a)
        return R::failure("the project has no architecture '" + configuration.value("architecture", std::string()) + "'");
    if (!a->stack(configuration.value("stack", std::string())))
        return R::failure("the architecture has no stack '" + configuration.value("stack", std::string()) + "'");
    if (configuration.value("analyses", json::array()).empty())
        return R::failure("the configuration names no analysis");
    return R(std::make_shared<MatterAdapter>(snapshot, configuration, *project.value(), *a));
}

void registerMatterOptimization(opt::AdapterRegistry& registry)
{
    registry.add(kMatterAdapterId, "Functionally architected matter (stack analyses)", bindMatterAdapter);
}

}  // namespace resonant::coupling
