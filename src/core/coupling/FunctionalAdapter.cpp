#include "coupling/FunctionalAdapter.h"

#include "functional/FunctionalStudy.h"
#include "optimization/Persistence.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"

#include <chrono>
#include <cmath>

namespace resonant::coupling {

namespace opt = optimization;
using nlohmann::json;

namespace {

constexpr const char* kParameterPrefix = "parameter/";

opt::Fidelity fidelityOf(const std::string& text)
{
    if (text == "full-wave" || text == "finite-element" || text == "coupled")
        return opt::Fidelity::HighFidelity;
    if (text == "reduced")
        return opt::Fidelity::Reduced;
    return opt::Fidelity::Analytical;
}

class FunctionalAdapter final : public opt::SnapshotAdapter {
public:
    FunctionalAdapter(json snapshot, json configuration, const Project& project, functional::FunctionalStructure structure,
                      const functional::AnalysisKind& kind)
        : SnapshotAdapter(std::move(snapshot), std::move(configuration)), structure_(std::move(structure)), kind_(kind)
    {
        describe(project);
        finishBinding();
    }

    opt::Evaluation evaluate(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId, JobContext* context) const override
    {
        using clock = std::chrono::steady_clock;
        const auto start = clock::now();
        opt::Evaluation e;
        auto project = build(bindings);
        if (!project) {
            e.status = opt::Evaluation::Status::InvalidDesign;
            e.message = project.message();
            return e;
        }
        auto list = functional::structuresOf(*project.value());
        const functional::FunctionalStructure* s = nullptr;
        if (list)
            for (const auto& x : list.value())
                if (x.id == structure_.id)
                    s = &x;
        if (!s) {
            e.status = opt::Evaluation::Status::Failed;
            e.message = "the structure is not in the design";
            return e;
        }
        const functional::ModelInfo* model = kind_.model(modelId.empty() ? s->analysis.model : modelId);
        auto outcome = functional::evaluateStructure(*s, *project.value(), functional::AnalysisRegistry::instance(), model ? model->id : modelId, context);
        if (!outcome) {
            e.status = outcome.message() == "cancelled" ? opt::Evaluation::Status::Cancelled : opt::Evaluation::Status::Failed;
            e.message = outcome.message();
            return e;
        }
        for (const auto& [key, value] : outcome.value().metrics)
            if (std::isfinite(value))
                e.quantities[key] = value;
        auto parameters = functional::resolveParameters(*s, *project.value());
        if (parameters)
            for (const auto& c : functional::evaluateConstraints(*s, functional::valuesOf(parameters.value()), outcome.value().metrics))
                if (c.evaluated)
                    e.quantities["constraint." + c.id] = c.value;
        e.model.modelId = outcome.value().modelId;
        e.model.modelName = outcome.value().modelName;
        e.model.version = outcome.value().modelVersion;
        e.model.fidelity = model ? fidelityOf(model->fidelity) : opt::Fidelity::Analytical;
        e.model.assumptions = outcome.value().assumptions;
        e.model.adapterId = kFunctionalAdapterId;
        e.model.adapterVersion = "1";
        e.model.snapshotKey = snapshotKey();
        e.warnings = outcome.value().warnings;
        e.details = {{"behaviour", outcome.value().behaviour}, {"discretization", outcome.value().discretization}};
        e.seconds = std::chrono::duration<double>(clock::now() - start).count();
        return e;
    }

protected:
    Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const override
    {
        if (key.substr(0, std::string_view(kParameterPrefix).size()) != kParameterPrefix)
            return false;
        const std::string id(key.substr(std::string_view(kParameterPrefix).size()));
        auto list = functional::structuresOf(project);
        if (!list)
            return Result<bool>::failure(list.message());
        for (auto& s : list.value())
            if (s.id == structure_.id) {
                functional::Parameter* p = s.parameter(id);
                if (!p || p->kind != functional::ParameterKind::Free)
                    return Result<bool>::failure("'" + id + "' is not a free parameter of the structure");
                if (!(value >= p->minimum && value <= p->maximum))
                    return Result<bool>::failure("'" + id + "' lies outside its range");
                p->value = value;
                functional::storeStructures(project, list.value());
                return true;
            }
        return Result<bool>::failure("the structure is not in the design");
    }

private:
    void describe(const Project& project)
    {
        opt::Capabilities& c = capabilities_;
        c.adapterId = kFunctionalAdapterId;
        c.domain = kind_.domains.empty() ? "functional" : kind_.domains.front();
        c.version = "1";
        c.threadSafe = true;
        auto parameters = functional::resolveParameters(structure_, project);
        if (parameters)
            for (const auto& p : parameters.value()) {
                if (!p.editable)
                    continue;
                opt::DesignVariable v;
                v.id = p.id;
                v.label = p.name;
                v.binding = p.kind == functional::ParameterKind::Bound ? p.binding : std::string(kParameterPrefix) + p.id;
                v.unit = p.unit;
                v.initial = p.value;
                const double span = std::abs(p.value) > 0.0 ? 0.5 * std::abs(p.value) : 1.0;
                v.lower = std::isfinite(p.minimum) ? p.minimum : p.value - span;
                v.upper = std::isfinite(p.maximum) ? p.maximum : p.value + span;
                v.group = p.group;
                c.variables.push_back(v);
            }
        for (const auto& m : kind_.metrics)
            c.quantities.push_back({m.key, m.label, m.unit, m.definition, {}, {}, false});
        for (const auto& k : structure_.constraints)
            c.quantities.push_back({"constraint." + k.id, "Constraint " + k.id, k.unit, k.expression, {}, {}, false});
        for (const auto& m : kind_.models)
            c.models.push_back({m.id, m.name, m.version, fidelityOf(m.fidelity), m.assumptions, true, {}, m.typicalSeconds});
        c.limitations = {"every candidate is evaluated by the structure's analysis on its own copy of the design"};
    }

    functional::FunctionalStructure structure_;
    const functional::AnalysisKind& kind_;
};

}  // namespace

Result<std::shared_ptr<const opt::DesignEvaluator>> bindFunctionalAdapter(const json& snapshot, const json& configuration)
{
    using R = Result<std::shared_ptr<const opt::DesignEvaluator>>;
    auto project = opt::projectFromSnapshot(snapshot);
    if (!project)
        return R::failure(project.message());
    const std::string id = configuration.value("structure", "");
    auto list = functional::structuresOf(*project.value());
    if (!list)
        return R::failure(list.message());
    for (const auto& s : list.value())
        if (s.id == id) {
            const functional::AnalysisKind* kind = functional::AnalysisRegistry::instance().find(s.analysis.kind);
            if (!kind)
                return R::failure("no analysis '" + s.analysis.kind + "'");
            return R(std::make_shared<FunctionalAdapter>(snapshot, configuration, *project.value(), s, *kind));
        }
    return R::failure("the project has no functional structure '" + id + "'");
}

void registerFunctionalOptimization(opt::AdapterRegistry& registry)
{
    registry.add(kFunctionalAdapterId, "Functional structures (geometry-driven analyses)", bindFunctionalAdapter);
}

}  // namespace resonant::coupling
