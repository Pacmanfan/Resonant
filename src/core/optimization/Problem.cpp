#include "optimization/Problem.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace resonant::optimization {

std::string_view toString(Distribution distribution) { return distribution == Distribution::Normal ? "normal" : "uniform"; }

std::string_view toString(RobustMeasure measure)
{
    switch (measure) {
    case RobustMeasure::Mean: return "mean";
    case RobustMeasure::MeanPlusSigma: return "mean-plus-k-sigma";
    case RobustMeasure::WorstCase: return "worst-case";
    }
    return "mean";
}

Status OptimizationProblem::validate() const
{
    if (adapterId.empty())
        return Status::failure("the problem names no physics adapter");
    if (modelId.empty())
        return Status::failure("the problem names no physical model");
    if (Status s = space.validate(); !s && s.message().find("neither a variable nor a constant") == std::string::npos)
        return s;
    if (space.dimension() == 0)
        return Status::failure("the problem has no free design variable");
    if (objectives.empty())
        return Status::failure("the problem has no objective");
    std::set<std::string> ids;
    for (const Objective& o : objectives) {
        if (Status s = o.validate(); !s)
            return s;
        if (!ids.insert(o.id).second)
            return Status::failure("two objectives have the id '" + o.id + "'");
    }
    for (const DesignConstraint& c : constraints) {
        if (Status s = c.validate(); !s)
            return s;
        if (!ids.insert(c.id).second)
            return Status::failure("the id '" + c.id + "' is used twice among objectives and constraints");
    }
    if (algorithm.maxEvaluations < 1)
        return Status::failure("the evaluation budget must be at least one");
    if (robust.enabled) {
        if (robust.parameters.empty())
            return Status::failure("robust optimization needs at least one uncertain parameter");
        if (robust.samples < 2)
            return Status::failure("robust optimization needs at least two samples per design");
        for (const UncertainParameter& p : robust.parameters) {
            if (p.binding.empty())
                return Status::failure("an uncertain parameter names no binding");
            if (!(p.width > 0.0) || !std::isfinite(p.width))
                return Status::failure("uncertain parameter '" + p.binding + "': the width must be positive");
            if (p.source.empty())
                return Status::failure("uncertain parameter '" + p.binding +
                                       "': state where the distribution comes from (tolerance, data sheet, measurement); "
                                       "Resonant does not assume one");
        }
        if (robust.requiredProbability <= 0.0 || robust.requiredProbability > 1.0)
            return Status::failure("the required probability must be in (0, 1]");
    }
    return Status::success();
}

Status OptimizationProblem::validateAgainst(const Capabilities& caps, std::vector<std::string>* warnings) const
{
    if (Status s = validate(); !s)
        return s;
    if (Status s = space.validate(caps.constants); !s)
        return s;
    const ModelInfo* model = caps.model(modelId);
    if (!model)
        return Status::failure("the adapter '" + caps.adapterId + "' has no model '" + modelId + "'");
    if (!model->available)
        return Status::failure("the model '" + model->name + "' is not available: " + model->unavailableReason);
    if (!validationModelId.empty()) {
        const ModelInfo* v = caps.model(validationModelId);
        if (!v)
            return Status::failure("the adapter has no validation model '" + validationModelId + "'");
        if (warnings && static_cast<int>(v->fidelity) <= static_cast<int>(model->fidelity))
            warnings->push_back("the validation model is not of a higher fidelity than the optimization model");
    }
    for (const DesignVariable& v : space.variables) {
        if (!v.active)
            continue;
        if (!caps.variable(v.key()))
            return Status::failure("the adapter cannot change '" + v.key() + "' (variable '" + v.id + "') in this project");
    }
    for (const DerivedVariable& d : space.derived)
        if (!d.binding.empty() && !caps.variable(d.binding))
            return Status::failure("the adapter cannot change '" + d.binding + "' (derived variable '" + d.id + "')");
    const auto checkQuantity = [&](const std::string& key, const std::string& owner) -> Status {
        const QuantityInfo* q = caps.quantity(key);
        if (!q)
            return Status::failure(owner + ": the adapter does not calculate '" + key + "'");
        if (!q->models.empty() && std::find(q->models.begin(), q->models.end(), modelId) == q->models.end())
            return Status::failure(owner + ": '" + q->label + "' is not calculated by the model '" + model->name + "'");
        return Status::success();
    };
    for (const Objective& o : objectives)
        if (Status s = checkQuantity(o.quantity, "objective '" + o.id + "'"); !s)
            return s;
    for (const DesignConstraint& c : constraints) {
        if (c.isExpression()) {
            auto e = Expression::parse(c.expression());
            for (const std::string& n : e.value().names())
                if (!space.find(n) && !caps.constants.contains(n) &&
                    std::none_of(space.derived.begin(), space.derived.end(), [&](const DerivedVariable& d) { return d.id == n; }))
                    return Status::failure("constraint '" + c.id + "' refers to '" + n + "', which is neither a design variable nor a constant");
            continue;
        }
        if (Status s = checkQuantity(c.quantity, "constraint '" + c.id + "'"); !s)
            return s;
    }
    if (robust.enabled)
        for (const UncertainParameter& p : robust.parameters)
            if (!caps.variable(p.binding))
                return Status::failure("the adapter cannot perturb '" + p.binding + "'");
    if (warnings && objectives.size() > 1 && algorithm.algorithm != "nsga2")
        warnings->push_back("several objectives with a single-objective method: they are combined as a weighted sum of their scores; "
                            "the Pareto front of the evaluated candidates is still reported");
    return Status::success();
}

void OptimizationProblem::fillUnits(const Capabilities& caps)
{
    for (Objective& o : objectives)
        if (const QuantityInfo* q = caps.quantity(o.quantity))
            o.unit = q->unit;
    for (DesignConstraint& c : constraints)
        if (const QuantityInfo* q = caps.quantity(c.quantity))
            c.unit = q->unit;
}

nlohmann::json toJson(const AlgorithmSettings& s)
{
    return {{"algorithm", s.algorithm},
            {"maxEvaluations", s.maxEvaluations},
            {"seed", s.seed},
            {"populationSize", s.populationSize},
            {"pointsPerVariable", s.pointsPerVariable},
            {"samples", s.samples},
            {"tolerance", s.tolerance},
            {"initialStep", s.initialStep},
            {"gradientStep", s.gradientStep},
            {"parallel", s.parallel},
            {"maxSeconds", s.maxSeconds},
            {"memoryBudgetBytes", s.memoryBudgetBytes},
            {"checkpointEvery", s.checkpointEvery}};
}

AlgorithmSettings algorithmSettingsFromJson(const nlohmann::json& j)
{
    AlgorithmSettings s;
    if (!j.is_object())
        return s;
    s.algorithm = j.value("algorithm", s.algorithm);
    s.maxEvaluations = j.value("maxEvaluations", s.maxEvaluations);
    s.seed = j.value("seed", s.seed);
    s.populationSize = j.value("populationSize", s.populationSize);
    s.pointsPerVariable = j.value("pointsPerVariable", s.pointsPerVariable);
    s.samples = j.value("samples", s.samples);
    s.tolerance = j.value("tolerance", s.tolerance);
    s.initialStep = j.value("initialStep", s.initialStep);
    s.gradientStep = j.value("gradientStep", s.gradientStep);
    s.parallel = j.value("parallel", s.parallel);
    s.maxSeconds = j.value("maxSeconds", s.maxSeconds);
    s.memoryBudgetBytes = j.value("memoryBudgetBytes", s.memoryBudgetBytes);
    s.checkpointEvery = j.value("checkpointEvery", s.checkpointEvery);
    return s;
}

nlohmann::json toJson(const RobustSettings& s)
{
    nlohmann::json params = nlohmann::json::array();
    for (const UncertainParameter& p : s.parameters)
        params.push_back({{"binding", p.binding},
                          {"label", p.label},
                          {"distribution", toString(p.distribution)},
                          {"width", p.width},
                          {"relative", p.relative},
                          {"truncate", p.truncate},
                          {"source", p.source}});
    return {{"enabled", s.enabled},   {"parameters", params}, {"samples", s.samples}, {"measure", toString(s.measure)},
            {"k", s.k},               {"requiredProbability", s.requiredProbability}, {"seed", s.seed}};
}

Result<RobustSettings> robustSettingsFromJson(const nlohmann::json& j)
{
    RobustSettings s;
    if (!j.is_object())
        return s;
    try {
        s.enabled = j.value("enabled", false);
        s.samples = j.value("samples", s.samples);
        const std::string m = j.value("measure", std::string("mean"));
        s.measure = m == "worst-case" ? RobustMeasure::WorstCase : m == "mean-plus-k-sigma" ? RobustMeasure::MeanPlusSigma : RobustMeasure::Mean;
        s.k = j.value("k", 1.0);
        s.requiredProbability = j.value("requiredProbability", 1.0);
        s.seed = j.value("seed", s.seed);
        for (const auto& p : j.value("parameters", nlohmann::json::array())) {
            UncertainParameter u;
            u.binding = p.at("binding").get<std::string>();
            u.label = p.value("label", u.binding);
            u.distribution = p.value("distribution", std::string("uniform")) == "normal" ? Distribution::Normal : Distribution::Uniform;
            u.width = p.value("width", 0.0);
            u.relative = p.value("relative", false);
            u.truncate = p.value("truncate", 3.0);
            u.source = p.value("source", std::string());
            s.parameters.push_back(std::move(u));
        }
        return s;
    } catch (const std::exception& e) {
        return Result<RobustSettings>::failure(std::string("robust settings: ") + e.what());
    }
}

nlohmann::json toJson(const OptimizationProblem& p)
{
    nlohmann::json objectives = nlohmann::json::array();
    for (const Objective& o : p.objectives)
        objectives.push_back(toJson(o));
    nlohmann::json constraints = nlohmann::json::array();
    for (const DesignConstraint& c : p.constraints)
        constraints.push_back(toJson(c));
    return {{"format", kProblemFormat},
            {"version", kProblemVersion},
            {"id", p.id},
            {"name", p.name},
            {"description", p.description},
            {"template", p.templateId},
            {"adapter", p.adapterId},
            {"adapterConfig", p.adapterConfig},
            {"model", p.modelId},
            {"validationModel", p.validationModelId},
            {"designSpace", toJson(p.space)},
            {"objectives", objectives},
            {"constraints", constraints},
            {"algorithm", toJson(p.algorithm)},
            {"robust", toJson(p.robust)}};
}

Result<OptimizationProblem> problemFromJson(const nlohmann::json& j)
{
    using R = Result<OptimizationProblem>;
    if (!j.is_object())
        return R::failure("an optimization problem must be an object");
    if (j.value("format", std::string(kProblemFormat)) != kProblemFormat)
        return R::failure("not an optimization problem (format '" + j.value("format", std::string()) + "')");
    const int version = j.value("version", kProblemVersion);
    if (version > kProblemVersion)
        return R::failure("the problem has version " + std::to_string(version) + "; this build reads version " +
                          std::to_string(kProblemVersion) + " and older");
    try {
        OptimizationProblem p;
        p.id = j.value("id", std::string());
        p.name = j.value("name", p.id);
        p.description = j.value("description", std::string());
        p.templateId = j.value("template", std::string());
        p.adapterId = j.value("adapter", std::string());
        p.adapterConfig = j.value("adapterConfig", nlohmann::json::object());
        p.modelId = j.value("model", std::string());
        p.validationModelId = j.value("validationModel", std::string());
        auto space = designSpaceFromJson(j.value("designSpace", nlohmann::json::object()));
        if (!space)
            return R::failure(space.message());
        p.space = space.take();
        for (const auto& o : j.value("objectives", nlohmann::json::array())) {
            auto v = objectiveFromJson(o);
            if (!v)
                return R::failure(v.message());
            p.objectives.push_back(v.take());
        }
        for (const auto& c : j.value("constraints", nlohmann::json::array())) {
            auto v = constraintFromJson(c);
            if (!v)
                return R::failure(v.message());
            p.constraints.push_back(v.take());
        }
        p.algorithm = algorithmSettingsFromJson(j.value("algorithm", nlohmann::json::object()));
        auto robust = robustSettingsFromJson(j.value("robust", nlohmann::json::object()));
        if (!robust)
            return R::failure(robust.message());
        p.robust = robust.take();
        return p;
    } catch (const std::exception& e) {
        return R::failure(std::string("optimization problem: ") + e.what());
    }
}

}  // namespace resonant::optimization
