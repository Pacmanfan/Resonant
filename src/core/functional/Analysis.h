#pragma once

// Analyses of functional structures (RES-FUNC-005): what a kind of analysis
// computes, with which models, and the outcome of one evaluation. The
// functional module defines the interface only; the analyses — adapters to
// the domains' models — are registered where the domains are known
// (src/core/coupling/FunctionalAnalyses.cpp), which also defines instance().

#include "foundation/Result.h"
#include "functional/FunctionalStructure.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::functional {

using Values = std::map<std::string, double, std::less<>>;

// The energy-transformation category of the request (section 4).
enum class Category { Propagation, Reflection, Focusing, Confinement, Dissipation, Force, Coupled };
std::string_view toString(Category category);

struct MetricInfo {
    std::string key;
    std::string label;
    std::string unit;
    std::string definition;
};

struct ModelInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string fidelity;  // analytical, reduced, full-wave, finite-element, coupled
    std::vector<std::string> assumptions;
    std::string validity;
    double typicalSeconds = 0.0;
};

struct RoleInfo {
    std::string role;
    std::string kind;  // body, region, boundary
    std::string description;
    bool required = true;
};

// One resonance, as every analysis reports it (RES-RSN-001).
struct Mode {
    Mode(std::string label_ = {}, double frequency = 0.0, double q = 0.0, nlohmann::json details_ = nlohmann::json::object())
        : label(std::move(label_)), frequencyHz(frequency), quality(q), details(std::move(details_))
    {
    }
    std::string label;          // empty when the model cannot label its modes
    double frequencyHz = 0.0;
    double quality = 0.0;       // 0: not computed; +inf: lossless
    nlohmann::json details = nlohmann::json::object();
    // "natural" (an eigenmode), "driven-peak" (a maximum of a response) or "identified" (from a signal).
    std::string nature = "natural";
    double decayRate = 0.0;     // 1/s (amplitude), 0 when not computed
    double bandwidthHz = 0.0;   // half-power, 0 when not computed
    std::map<std::string, double> lossQ;  // Q of each loss mechanism the model includes
};

struct Curve {
    std::string id, title, xLabel, xUnit, yLabel, yUnit;
    std::vector<double> x, y;
};

struct AnalysisOutcome {
    Values metrics;
    std::vector<Mode> modes;
    std::vector<Curve> curves;
    std::vector<std::string> assumptions;
    std::vector<std::string> warnings;
    std::string modelId, modelName, modelVersion, solver;
    // resonant, broadband, non-resonant or mixed: what the model established, not a label of the shape.
    std::string behaviour;
    nlohmann::json details = nlohmann::json::object();
    // The simulation geometry the model built (grid, mesh, steps) and whether it was reused.
    nlohmann::json discretization = nlohmann::json::object();
};

using Evaluate = std::function<Result<AnalysisOutcome>(const FunctionalStructure& structure, const Project& project, const Values& parameters,
                                                       const ModelInfo& model, JobContext* context)>;

struct AnalysisKind {
    std::string id;
    std::string title;
    std::string description;
    std::vector<std::string> domains;
    Category category = Category::Confinement;
    std::string mechanism;  // the physical mechanism, for explanations
    std::vector<ModelInfo> models;
    std::vector<MetricInfo> metrics;
    std::vector<RoleInfo> roles;
    Evaluate evaluate;

    const MetricInfo* metric(std::string_view key) const;
    const ModelInfo* model(std::string_view id) const;  // empty: the first
};
nlohmann::json toJson(const AnalysisKind& kind);  // the catalogue entry (no function)
nlohmann::json toJson(const AnalysisOutcome& outcome);

class AnalysisRegistry {
public:
    // Every analysis (defined in src/core/coupling/FunctionalAnalyses.cpp).
    static AnalysisRegistry& instance();
    void add(AnalysisKind kind);
    const AnalysisKind* find(std::string_view id) const;
    const std::vector<AnalysisKind>& kinds() const { return kinds_; }

private:
    std::vector<AnalysisKind> kinds_;
};

// Analysis-specific checks: the kind exists, the model exists, the roles it
// needs are present, objectives name its metrics.
std::vector<Issue> validateForAnalysis(const FunctionalStructure& structure, const AnalysisRegistry& registry);

}  // namespace resonant::functional
