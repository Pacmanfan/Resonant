#include "optimization/Sensitivity.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace resonant::optimization {

namespace {

double objectiveValue(const Candidate& c, size_t i) { return i < c.objectiveValues.size() ? c.objectiveValues[i] : NAN; }

std::vector<double> ranks(const std::vector<double>& v)
{
    std::vector<size_t> order(v.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return v[a] < v[b]; });
    std::vector<double> r(v.size());
    for (size_t i = 0; i < order.size();) {
        size_t j = i;
        while (j + 1 < order.size() && v[order[j + 1]] == v[order[i]])
            ++j;
        const double average = 0.5 * (static_cast<double>(i) + static_cast<double>(j));
        for (size_t k = i; k <= j; ++k)
            r[order[k]] = average;
        i = j + 1;
    }
    return r;
}

double pearson(const std::vector<double>& a, const std::vector<double>& b)
{
    const size_t n = a.size();
    if (n < 3)
        return NAN;
    const double ma = std::accumulate(a.begin(), a.end(), 0.0) / n, mb = std::accumulate(b.begin(), b.end(), 0.0) / n;
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return saa > 0.0 && sbb > 0.0 ? sab / std::sqrt(saa * sbb) : 0.0;
}

}  // namespace

SensitivityReport localSensitivity(EvaluationService& service, const DesignPoint& at, double step)
{
    SensitivityReport report;
    const OptimizationProblem& problem = service.problem();
    const auto free = service.space().freeVariables();
    for (const DesignVariable* v : free)
        report.variables.push_back(v->id);
    for (const Objective& o : problem.objectives)
        report.objectives.push_back(o.id);
    std::vector<DesignPoint> points{at};
    std::vector<std::pair<double, double>> steps;
    for (const DesignVariable* v : free) {
        const double range = v->upper - v->lower;
        const double x = at.values.count(v->id) ? at.values.at(v->id) : v->initial;
        double h = std::max(step * range, v->step);
        if (v->kind == VariableKind::Integer)
            h = 1.0;
        if (v->kind == VariableKind::Categorical) {
            steps.emplace_back(0.0, 0.0);
            report.warnings.push_back("'" + v->id + "' is categorical: it has no derivative (see the response curve instead)");
            continue;
        }
        const double up = std::min(h, v->upper - x), down = std::min(h, x - v->lower);
        DesignPoint a = at, b = at;
        a.values[v->id] = v->quantize(x + up);
        b.values[v->id] = v->quantize(x - down);
        [[maybe_unused]] Status sa = service.space().completeDerived(a, service.constants());
        [[maybe_unused]] Status sb = service.space().completeDerived(b, service.constants());
        points.push_back(a);
        points.push_back(b);
        steps.emplace_back(a.values[v->id] - x, x - b.values[v->id]);
    }
    const std::vector<Candidate> c = service.evaluatePoints(points, "sensitivity", 0, {}, {}, true);
    report.evaluations = static_cast<int>(c.size());
    if (c.size() < points.size() || !c.front().evaluated()) {
        report.warnings.push_back("the design could not be evaluated at every finite-difference point");
        if (c.empty() || !c.front().evaluated())
            return report;
    }
    report.heatmap.assign(free.size(), std::vector<double>(problem.objectives.size(), 0.0));
    std::vector<std::pair<double, std::string>> influence;
    size_t index = 1;
    for (size_t vi = 0; vi < free.size(); ++vi) {
        const DesignVariable& v = *free[vi];
        const auto [up, down] = steps[vi];
        if (v.kind == VariableKind::Categorical) {
            influence.emplace_back(NAN, v.id);
            continue;
        }
        const Candidate* plus = index < c.size() ? &c[index] : nullptr;
        const Candidate* minus = index + 1 < c.size() ? &c[index + 1] : nullptr;
        index += 2;
        double largest = 0.0;
        for (size_t oi = 0; oi < problem.objectives.size(); ++oi) {
            const Objective& o = problem.objectives[oi];
            LocalSensitivity e;
            e.variable = v.id;
            e.objective = o.id;
            e.unit = (o.unit.empty() ? std::string("1") : o.unit) + " per " + (v.unit.empty() ? std::string("unit") : v.unit);
            const double q0 = objectiveValue(c.front(), oi);
            const double qp = plus && plus->evaluated() ? objectiveValue(*plus, oi) : NAN;
            const double qm = minus && minus->evaluated() ? objectiveValue(*minus, oi) : NAN;
            if (up > 0.0 && std::isfinite(qp))
                e.forward = (qp - q0) / up;
            if (down > 0.0 && std::isfinite(qm))
                e.backward = (q0 - qm) / down;
            if (up > 0.0 && down > 0.0 && std::isfinite(qp) && std::isfinite(qm)) {
                e.derivative = (qp - qm) / (up + down);
                const double scale = std::max(std::abs(e.derivative), 1e-12 * (1.0 + std::abs(q0)) / std::max(up, down));
                if (std::abs(e.forward - e.backward) > 0.5 * scale + 1e-300) {
                    e.reliable = false;
                    e.note = "one-sided differences disagree: discontinuity, solver noise or a mesh change";
                }
            } else if (up > 0.0 && std::isfinite(qp)) {
                e.derivative = e.forward;
                e.note = "one-sided (at the lower bound)";
            } else if (down > 0.0 && std::isfinite(qm)) {
                e.derivative = e.backward;
                e.note = "one-sided (at the upper bound)";
            } else {
                e.reliable = false;
                e.note = "the quantity was not available next to the design";
            }
            if (!std::isfinite(q0)) {
                e.reliable = false;
                e.note = "the quantity is not available at the design";
            }
            e.normalized = e.derivative * (v.upper - v.lower) / (o.scale > 0.0 ? o.scale : 1.0);
            report.heatmap[vi][oi] = std::abs(e.normalized);
            largest = std::max(largest, std::abs(e.normalized));
            if (!e.reliable)
                report.warnings.push_back("d(" + o.id + ")/d(" + v.id + "): " + e.note);
            report.entries.push_back(e);
        }
        influence.emplace_back(largest, v.id);
        if (largest < 0.01)
            report.negligible.push_back(v.id);
    }
    std::stable_sort(influence.begin(), influence.end(), [](const auto& a, const auto& b) {
        if (std::isnan(a.first) != std::isnan(b.first))
            return !std::isnan(a.first);
        return a.first > b.first;
    });
    for (const auto& [value, id] : influence)
        report.ranking.push_back(id);
    report.warnings.push_back("finite-difference estimates (step " + std::to_string(step) + " of each range); not exact derivatives");
    return report;
}

std::vector<Correlation> correlations(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, const std::string& modelId)
{
    std::vector<Correlation> out;
    const std::string model = modelId.empty() ? problem.modelId : modelId;
    for (const DesignVariable* v : problem.space.freeVariables())
        for (size_t oi = 0; oi < problem.objectives.size(); ++oi) {
            std::vector<double> xs, ys;
            for (const Candidate& c : candidates) {
                if (!c.evaluated() || c.evaluation.model.modelId != model)
                    continue;
                const auto it = c.point.values.find(v->id);
                const double y = objectiveValue(c, oi);
                if (it == c.point.values.end() || !std::isfinite(y))
                    continue;
                xs.push_back(it->second);
                ys.push_back(y);
            }
            Correlation r;
            r.variable = v->id;
            r.objective = problem.objectives[oi].id;
            r.samples = xs.size();
            r.pearson = pearson(xs, ys);
            r.spearman = pearson(ranks(xs), ranks(ys));
            out.push_back(r);
        }
    return out;
}

std::vector<ResponseCurve> responseCurves(EvaluationService& service, const DesignPoint& at, int points)
{
    std::vector<ResponseCurve> out;
    const OptimizationProblem& problem = service.problem();
    for (const DesignVariable* v : service.space().freeVariables()) {
        std::vector<DesignPoint> designs;
        std::vector<double> xs;
        const int n = v->kind == VariableKind::Categorical ? static_cast<int>(v->choices.size()) : std::max(2, points);
        for (int k = 0; k < n; ++k) {
            DesignPoint p = at;
            const double x = v->kind == VariableKind::Categorical ? k : v->lower + (v->upper - v->lower) * k / (n - 1);
            p.values[v->id] = v->quantize(x);
            [[maybe_unused]] Status s = service.space().completeDerived(p, service.constants());
            designs.push_back(p);
            xs.push_back(p.values[v->id]);
        }
        const auto c = service.evaluatePoints(designs, "response curve", 0, {}, {}, true);
        for (size_t oi = 0; oi < problem.objectives.size(); ++oi) {
            ResponseCurve curve;
            curve.variable = v->id;
            curve.objective = problem.objectives[oi].id;
            for (size_t k = 0; k < c.size(); ++k) {
                curve.x.push_back(xs[k]);
                curve.y.push_back(c[k].evaluated() ? objectiveValue(c[k], oi) : NAN);
            }
            out.push_back(std::move(curve));
        }
    }
    return out;
}

}  // namespace resonant::optimization
