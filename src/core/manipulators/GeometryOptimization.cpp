#include "manipulators/GeometryOptimization.h"

#include <cmath>
#include <limits>

namespace resonant::manipulators {

OptimizationOutcome optimizeGeometry(const Geometry& start, const std::vector<DesignVariable>& variables,
                                     const GeometryObjective& objective, const OptimizationSettings& settings)
{
    OptimizationOutcome outcome;
    outcome.method = "cyclic coordinate search with golden-section line searches (derivative-free, local)";
    outcome.variables = variables;
    for (const DesignVariable& variable : variables) {
        if (!start.kind().findParameter(variable.parameter)) {
            outcome.notes.push_back("The geometry has no parameter '" + variable.parameter + "'; nothing was optimized.");
            return outcome;
        }
        if (!(variable.lower < variable.upper)) {
            outcome.notes.push_back("The bounds of '" + variable.parameter + "' are empty; nothing was optimized.");
            return outcome;
        }
    }
    if (variables.empty()) {
        outcome.notes.push_back("No design variable was given; nothing was optimized.");
        return outcome;
    }

    int evaluations = 0;
    const auto evaluate = [&](const std::vector<double>& values) {
        Candidate candidate;
        candidate.values = values;
        Geometry geometry = start;
        for (size_t i = 0; i < variables.size(); ++i) {
            if (Status status = geometry.setParameter(variables[i].parameter, values[i]); !status) {
                candidate.feasible = false;
                candidate.reason = status.message();
            }
        }
        if (candidate.feasible) {
            if (const auto violations = checkConstraints(geometry, settings.constraints); !violations.empty()) {
                candidate.feasible = false;
                candidate.reason = "violates " + violations.front().constraint;
            }
        }
        candidate.objective = candidate.feasible ? objective(geometry) : -std::numeric_limits<double>::infinity();
        ++evaluations;
        outcome.history.push_back(candidate);
        return candidate;
    };

    std::vector<double> current;
    for (const DesignVariable& variable : variables)
        current.push_back(std::clamp(start.parameter(variable.parameter), variable.lower, variable.upper));
    outcome.initial = evaluate(current);
    Candidate best = outcome.initial;

    const double ratio = 0.5 * (std::sqrt(5.0) - 1.0);  // 0.618...
    bool progressed = true;
    while (progressed && evaluations < settings.maxEvaluations) {
        progressed = false;
        for (size_t i = 0; i < variables.size() && evaluations < settings.maxEvaluations; ++i) {
            double a = variables[i].lower;
            double b = variables[i].upper;
            const double span = b - a;
            std::vector<double> probe = best.values;
            probe[i] = b - ratio * (b - a);
            Candidate c = evaluate(probe);
            probe[i] = a + ratio * (b - a);
            Candidate d = evaluate(probe);
            while (b - a > settings.tolerance * span && evaluations < settings.maxEvaluations) {
                if (!c.feasible && !d.feasible) {
                    // Both probes are infeasible: move towards the side that
                    // holds the best feasible value known, and probe again.
                    if (best.values[i] <= c.values[i])
                        b = c.values[i];
                    else
                        a = d.values[i];
                    probe[i] = b - ratio * (b - a);
                    c = evaluate(probe);
                    probe[i] = a + ratio * (b - a);
                    d = evaluate(probe);
                } else if (c.objective > d.objective) {
                    b = d.values[i];
                    d = c;
                    probe[i] = b - ratio * (b - a);
                    c = evaluate(probe);
                } else {
                    a = c.values[i];
                    c = d;
                    probe[i] = a + ratio * (b - a);
                    d = evaluate(probe);
                }
            }
            const Candidate& found = c.objective > d.objective ? c : d;
            if (found.feasible && found.objective > best.objective * (1.0 + 1e-9) + 1e-300) {
                best = found;
                progressed = variables.size() > 1;  // with one variable a second pass adds nothing
            }
        }
    }

    outcome.best = best;
    outcome.improved = best.feasible && best.objective > outcome.initial.objective;
    if (best.feasible) {
        Geometry geometry = start;
        for (size_t i = 0; i < variables.size(); ++i)
            [[maybe_unused]] const Status status = geometry.setParameter(variables[i].parameter, best.values[i]);
        outcome.bestGeometry = geometry;
    }
    if (!outcome.initial.feasible)
        outcome.notes.push_back("The starting design is infeasible: " + outcome.initial.reason + ".");
    if (evaluations >= settings.maxEvaluations)
        outcome.notes.push_back("Stopped at the limit of " + std::to_string(settings.maxEvaluations) + " evaluations.");
    outcome.notes.push_back("A local search: the result is the best candidate found, not a proven optimum. Check it "
                            "with a field simulation.");
    return outcome;
}

}  // namespace resonant::manipulators
