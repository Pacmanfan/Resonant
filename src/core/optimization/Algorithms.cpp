#include "optimization/Algorithms.h"

#include "optimization/Random.h"
#include "optimization/Surrogate.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <numeric>
#include <sstream>

namespace resonant::optimization {

namespace {

using Point = std::vector<double>;
constexpr double kInf = std::numeric_limits<double>::infinity();

Point clampUnit(Point p)
{
    for (double& x : p)
        x = std::clamp(x, 0.0, 1.0);
    return p;
}

double infNorm(const Point& a, const Point& b)
{
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

std::string text(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

// Evaluates one point; returns its merit (infinity when stopped).
struct Probe {
    EvaluationService& s;
    std::string origin;
    int generation = 0;
    double operator()(const Point& u, const std::vector<std::string>& parents = {}) const
    {
        auto c = s.evaluate({u}, origin, generation, {parents});
        return c.empty() ? kInf : s.merit(c.front());
    }
    std::vector<double> batch(const std::vector<Point>& us) const
    {
        auto cs = s.evaluate(us, origin, generation);
        std::vector<double> out(us.size(), kInf);
        for (size_t i = 0; i < cs.size(); ++i)
            out[i] = s.merit(cs[i]);
        return out;
    }
};

// --- sampling --------------------------------------------------------------------------------------

std::vector<double> axisValues(const DesignVariable& v, int pointsPerVariable)
{
    std::vector<double> out;
    if (v.kind == VariableKind::Categorical) {
        const size_t n = v.choices.size();
        for (size_t k = 0; k < n; ++k)
            out.push_back((k + 0.5) / n);
        return out;
    }
    if (v.kind == VariableKind::Integer) {
        const double lo = std::ceil(v.lower), hi = std::floor(v.upper);
        const int count = static_cast<int>(hi - lo + 1.0);
        if (count <= std::max(2, pointsPerVariable)) {
            for (int k = 0; k < count; ++k)
                out.push_back((k + 0.5) / count);
            return out;
        }
    }
    const int n = std::max(2, pointsPerVariable);
    for (int k = 0; k < n; ++k)
        out.push_back(static_cast<double>(k) / (n - 1));
    return out;
}

class Sweep final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"sweep", "Parameter sweep (grid)", "sampling", true, true, false, true,
                "Every combination of a fixed number of values per variable (all choices of a categorical variable); "
                "one free variable gives a one-variable sweep.",
                "ends when the grid is complete or the budget is used"};
    }
    Termination run(EvaluationService& s) const override
    {
        const auto free = s.space().freeVariables();
        std::vector<std::vector<double>> axes;
        size_t total = 1;
        for (const DesignVariable* v : free) {
            axes.push_back(axisValues(*v, s.problem().algorithm.pointsPerVariable));
            total *= axes.back().size();
        }
        std::vector<Point> grid;
        std::vector<size_t> index(axes.size(), 0);
        for (size_t n = 0; n < total; ++n) {
            Point p(axes.size());
            for (size_t k = 0; k < axes.size(); ++k)
                p[k] = axes[k][index[k]];
            grid.push_back(std::move(p));
            for (size_t k = 0; k < axes.size(); ++k) {
                if (++index[k] < axes[k].size())
                    break;
                index[k] = 0;
            }
        }
        Termination t;
        const size_t affordable = static_cast<size_t>(s.remaining());
        if (grid.size() > affordable)
            t.notes.push_back("the grid has " + std::to_string(grid.size()) + " points; the budget allowed " + std::to_string(affordable));
        s.evaluate(grid, "sweep", 0);
        t.reason = s.cancelled() ? "cancelled" : grid.size() > affordable ? "budget used before the grid was complete" : "grid complete";
        t.converged = grid.size() <= affordable && !s.cancelled();
        return t;
    }
};

class RandomSampling final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"random", "Random sampling", "sampling", true, true, false, true, "Independent uniform samples of the design space.",
                "ends after the requested number of samples or the budget"};
    }
    Termination run(EvaluationService& s) const override
    {
        Random random(s.problem().algorithm.seed);
        const int n = std::min(s.problem().algorithm.samples, s.remaining());
        s.evaluate(uniformSamples(n, static_cast<int>(s.dimension()), random), "random", 0);
        return {s.cancelled() ? "cancelled" : "samples complete", !s.cancelled(), {}};
    }
};

class LatinHypercubeSampling final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"latin-hypercube", "Latin hypercube (design of experiments)", "sampling", true, true, false, true,
                "Stratified samples: each variable's range is cut into as many equal strata as there are samples and each stratum is "
                "sampled once (best of ten hypercubes by the smallest pairwise distance).",
                "ends after the requested number of samples or the budget"};
    }
    Termination run(EvaluationService& s) const override
    {
        Random random(s.problem().algorithm.seed);
        const int n = std::min(s.problem().algorithm.samples, s.remaining());
        s.evaluate(latinHypercube(n, static_cast<int>(s.dimension()), random), "latin-hypercube", 0);
        return {s.cancelled() ? "cancelled" : "samples complete", !s.cancelled(), {}};
    }
};

// --- Nelder-Mead ----------------------------------------------------------------------------------------

class NelderMead final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"nelder-mead", "Nelder-Mead simplex", "derivative-free", false, false, false, false,
                "Local derivative-free simplex search in the unit cube (points outside are projected onto the bounds), with the "
                "dimension-adapted coefficients of Gao and Han (2012). Robust for noisy or non-smooth objectives of a few variables; "
                "finds a LOCAL optimum.",
                "stops when the simplex is smaller than 1e-4 of every range, or the merit values of its vertices differ by less than "
                "tolerance x (1 + |best|), or the budget is used"};
    }
    Termination run(EvaluationService& s) const override
    {
        const size_t n = s.dimension();
        const double step = std::clamp(s.problem().algorithm.initialStep, 1e-4, 0.5);
        const double tol = std::max(s.problem().algorithm.tolerance, 1e-14);
        const double alpha = 1.0, beta = 1.0 + 2.0 / n, gamma = 0.75 - 1.0 / (2.0 * n), delta = 1.0 - 1.0 / n;
        Probe probe{s, "nelder-mead", 0};
        std::vector<Point> x{s.initialUnit()};
        for (size_t i = 0; i < n; ++i) {
            Point p = x.front();
            p[i] = p[i] + step <= 1.0 ? p[i] + step : p[i] - step;
            x.push_back(clampUnit(p));
        }
        std::vector<double> f = probe.batch(x);
        Termination t;
        int iteration = 0;
        while (!s.shouldStop()) {
            std::vector<size_t> order(n + 1);
            std::iota(order.begin(), order.end(), size_t(0));
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return f[a] < f[b]; });
            std::vector<Point> xs;
            std::vector<double> fs;
            for (size_t k : order) {
                xs.push_back(x[k]);
                fs.push_back(f[k]);
            }
            x = xs;
            f = fs;
            double diameter = 0.0;
            for (size_t i = 1; i <= n; ++i)
                diameter = std::max(diameter, infNorm(x[i], x[0]));
            if (std::isfinite(f[n]) && (f[n] - f[0] <= tol * (1.0 + std::abs(f[0])) || diameter < 1e-4)) {
                t.converged = true;
                t.reason = diameter < 1e-4 ? "simplex smaller than 1e-4 of every range" : "simplex merit values agree within the tolerance";
                return t;
            }
            ++iteration;
            probe.generation = iteration;
            Point centroid(n, 0.0);
            for (size_t i = 0; i < n; ++i)
                for (size_t k = 0; k < n; ++k)
                    centroid[k] += x[i][k] / n;
            const auto along = [&](double coefficient) {
                Point p(n);
                for (size_t k = 0; k < n; ++k)
                    p[k] = centroid[k] + coefficient * (x[n][k] - centroid[k]);
                return clampUnit(p);
            };
            const Point xr = along(-alpha);
            const double fr = probe(xr);
            if (!std::isfinite(fr) && s.shouldStop())
                break;
            if (fr < f[0]) {
                const Point xe = along(-alpha * beta);
                const double fe = probe(xe);
                if (fe < fr) {
                    x[n] = xe;
                    f[n] = fe;
                } else {
                    x[n] = xr;
                    f[n] = fr;
                }
                continue;
            }
            if (fr < f[n - 1]) {
                x[n] = xr;
                f[n] = fr;
                continue;
            }
            const bool outside = fr < f[n];
            const Point xc = along(outside ? -alpha * gamma : gamma);
            const double fc = probe(xc);
            if (fc < (outside ? fr : f[n])) {
                x[n] = xc;
                f[n] = fc;
                continue;
            }
            // Shrink towards the best vertex.
            std::vector<Point> shrunk;
            for (size_t i = 1; i <= n; ++i) {
                Point p(n);
                for (size_t k = 0; k < n; ++k)
                    p[k] = x[0][k] + delta * (x[i][k] - x[0][k]);
                shrunk.push_back(clampUnit(p));
            }
            const std::vector<double> fsh = probe.batch(shrunk);
            for (size_t i = 1; i <= n; ++i) {
                x[i] = shrunk[i - 1];
                f[i] = fsh[i - 1];
            }
        }
        t.reason = s.stopReason();
        return t;
    }
};

// --- L-BFGS-B with finite differences ------------------------------------------------------------------------

class Lbfgs final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"lbfgs", "L-BFGS-B (finite-difference gradients)", "gradient", false, false, true, false,
                "Bound-constrained limited-memory BFGS in the unit cube (projected search direction, backtracking Armijo line "
                "search, memory 6). Gradients are CENTRAL FINITE DIFFERENCES (2 evaluations per variable), not exact gradients; "
                "each is checked against the one-sided difference and a warning is recorded where they disagree (discontinuity, "
                "solver noise, mesh changes). Local.",
                "stops when the projected gradient step is below tolerance x max(1, |merit|), when the line search finds no "
                "decrease, or when the budget is used"};
    }
    Termination run(EvaluationService& s) const override
    {
        const size_t n = s.dimension();
        const double h = std::clamp(s.problem().algorithm.gradientStep, 1e-8, 0.05);
        const double tol = std::max(s.problem().algorithm.tolerance, 1e-14);
        const auto free = s.space().freeVariables();
        Probe probe{s, "lbfgs", 0};
        Point x = s.initialUnit();
        double fx = probe(x);
        Termination t;
        if (!std::isfinite(fx)) {
            t.reason = s.shouldStop() ? s.stopReason() : "the initial design could not be evaluated";
            return t;
        }
        // Central differences (one-sided at a bound), with a reliability check.
        const auto gradient = [&](const Point& at, double fat, int iteration, bool& ok) {
            probe.origin = "lbfgs gradient";
            probe.generation = iteration;
            std::vector<Point> stencil;
            std::vector<std::pair<double, double>> steps;
            for (size_t i = 0; i < n; ++i) {
                const double up = std::min(h, 1.0 - at[i]), down = std::min(h, at[i]);
                Point a = at, b = at;
                a[i] += up;
                b[i] -= down;
                stencil.push_back(a);
                stencil.push_back(b);
                steps.emplace_back(up, down);
            }
            const std::vector<double> fv = probe.batch(stencil);
            ok = !(s.shouldStop() && std::any_of(fv.begin(), fv.end(), [](double v) { return !std::isfinite(v); }));
            Point g(n, 0.0);
            for (size_t i = 0; i < n; ++i) {
                const double fp = fv[2 * i], fm = fv[2 * i + 1];
                const auto [up, down] = steps[i];
                if (std::isfinite(fp) && std::isfinite(fm) && up + down > 0.0)
                    g[i] = (fp - fm) / (up + down);
                else if (std::isfinite(fp) && up > 0.0)
                    g[i] = (fp - fat) / up;
                else if (std::isfinite(fm) && down > 0.0)
                    g[i] = (fat - fm) / down;
                if (std::isfinite(fp) && std::isfinite(fm) && up > 0.0 && down > 0.0) {
                    const double forward = (fp - fat) / up, backward = (fat - fm) / down;
                    const double scale = std::max(std::abs(g[i]), 1e-9 * (1.0 + std::abs(fat)) / h);
                    if (std::abs(forward - backward) > 0.5 * scale + 1e-12)
                        s.note("finite-difference gradient of '" + free[i]->id + "' is unreliable at iteration " + std::to_string(iteration) +
                               ": the one-sided differences " + text(forward) + " and " + text(backward) +
                               " disagree (discontinuity, solver noise or a mesh change); these are approximate, not exact, gradients");
                }
            }
            return g;
        };
        bool ok = true;
        Point g = gradient(x, fx, 0, ok);
        std::deque<std::pair<Point, Point>> memory;  // (s, y)
        for (int iteration = 1; ok && !s.shouldStop(); ++iteration) {
            double pg = 0.0;
            for (size_t i = 0; i < n; ++i)
                pg = std::max(pg, std::abs(std::clamp(x[i] - g[i], 0.0, 1.0) - x[i]));
            if (pg <= tol * std::max(1.0, std::abs(fx))) {
                t.converged = true;
                t.reason = "projected gradient below the tolerance";
                return t;
            }
            std::vector<bool> active(n, true);
            for (size_t i = 0; i < n; ++i)
                if ((x[i] <= 0.0 && g[i] > 0.0) || (x[i] >= 1.0 && g[i] < 0.0))
                    active[i] = false;
            const auto dot = [&](const Point& a, const Point& b) {
                double v = 0.0;
                for (size_t i = 0; i < n; ++i)
                    if (active[i])
                        v += a[i] * b[i];
                return v;
            };
            Point q = g;
            for (size_t i = 0; i < n; ++i)
                if (!active[i])
                    q[i] = 0.0;
            std::vector<double> alphas;
            for (auto it = memory.rbegin(); it != memory.rend(); ++it) {
                const double sy = dot(it->first, it->second);
                const double a = sy > 0.0 ? dot(it->first, q) / sy : 0.0;
                alphas.push_back(a);
                for (size_t i = 0; i < n; ++i)
                    if (active[i])
                        q[i] -= a * it->second[i];
            }
            double scale = 1.0;
            if (!memory.empty()) {
                const double sy = dot(memory.back().first, memory.back().second), yy = dot(memory.back().second, memory.back().second);
                if (sy > 0.0 && yy > 0.0)
                    scale = sy / yy;
            } else {
                double gn = 0.0;
                for (size_t i = 0; i < n; ++i)
                    gn = std::max(gn, std::abs(q[i]));
                scale = gn > 0.0 ? 0.1 / gn : 1.0;  // without curvature information: a step of a tenth of a range
            }
            for (double& v : q)
                v *= scale;
            size_t k = alphas.size();
            for (const auto& [sv, yv] : memory) {
                --k;
                const double sy = dot(sv, yv);
                const double b = sy > 0.0 ? dot(yv, q) / sy : 0.0;
                for (size_t i = 0; i < n; ++i)
                    if (active[i])
                        q[i] += sv[i] * (alphas[k] - b);
            }
            Point d(n);
            double slope = 0.0;
            for (size_t i = 0; i < n; ++i) {
                d[i] = active[i] ? -q[i] : 0.0;
                slope += g[i] * d[i];
            }
            if (!(slope < 0.0)) {  // not a descent direction: restart with steepest descent
                memory.clear();
                double gn = 0.0;
                for (size_t i = 0; i < n; ++i)
                    gn = std::max(gn, std::abs(g[i]));
                for (size_t i = 0; i < n; ++i)
                    d[i] = active[i] && gn > 0.0 ? -g[i] * 0.1 / gn : 0.0;
            }
            probe.origin = "lbfgs line search";
            probe.generation = iteration;
            double step = 1.0, fn = kInf;
            Point xn;
            bool accepted = false;
            for (int trial = 0; trial < 12 && !s.shouldStop(); ++trial) {
                xn = x;
                for (size_t i = 0; i < n; ++i)
                    xn[i] = std::clamp(x[i] + step * d[i], 0.0, 1.0);
                if (infNorm(xn, x) < 1e-12)
                    break;
                fn = probe(xn);
                double predicted = 0.0;
                for (size_t i = 0; i < n; ++i)
                    predicted += g[i] * (xn[i] - x[i]);
                if (std::isfinite(fn) && fn <= fx + 1e-4 * predicted) {
                    accepted = true;
                    break;
                }
                step *= 0.5;
            }
            if (!accepted) {
                if (s.shouldStop())
                    break;
                t.reason = "the line search found no decrease (a local optimum, or an objective too noisy for finite differences)";
                return t;
            }
            Point gn = gradient(xn, fn, iteration, ok);
            Point sv(n), yv(n);
            double sy = 0.0;
            for (size_t i = 0; i < n; ++i) {
                sv[i] = xn[i] - x[i];
                yv[i] = gn[i] - g[i];
                sy += sv[i] * yv[i];
            }
            if (sy > 1e-16) {  // curvature condition; otherwise the pair is skipped
                memory.emplace_back(sv, yv);
                if (memory.size() > 6)
                    memory.pop_front();
            }
            x = xn;
            fx = fn;
            g = gn;
        }
        t.reason = s.stopReason();
        return t;
    }
};

// --- differential evolution -----------------------------------------------------------------------------------

class DifferentialEvolution final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"differential-evolution", "Differential evolution", "population", false, true, false, true,
                "DE/rand/1/bin (F = 0.6, CR = 0.9) on a Latin-hypercube initial population; selection by constraint domination "
                "(feasible before infeasible, then the smaller violation, then the merit). Discrete and categorical variables are "
                "quantized. Global but not guaranteed to find the global optimum.",
                "stops when the population's merit spread is below tolerance x (1 + |best|) and its extent below 1e-4 of every "
                "range, or the budget is used"};
    }
    Termination run(EvaluationService& s) const override
    {
        const size_t n = s.dimension();
        const AlgorithmSettings& a = s.problem().algorithm;
        int np = a.populationSize > 0 ? a.populationSize : static_cast<int>(std::max<size_t>(10, std::min<size_t>(8 * n, 60)));
        np = std::max(4, std::min(np, std::max(4, a.maxEvaluations / 3)));
        Random random(a.seed);
        std::vector<Point> pop = latinHypercube(np, static_cast<int>(n), random);
        pop.front() = s.initialUnit();  // the user's design takes part
        std::vector<Candidate> cand = s.evaluate(pop, "differential-evolution initial", 0);
        Termination t;
        if (cand.size() < pop.size()) {
            t.reason = s.stopReason();
            return t;
        }
        for (int generation = 1; !s.shouldStop(); ++generation) {
            std::vector<Point> trials;
            std::vector<std::vector<std::string>> parents;
            for (int i = 0; i < np; ++i) {
                int r1, r2, r3;
                do r1 = random.below(np); while (r1 == i);
                do r2 = random.below(np); while (r2 == i || r2 == r1);
                do r3 = random.below(np); while (r3 == i || r3 == r1 || r3 == r2);
                const size_t jr = static_cast<size_t>(random.below(static_cast<int>(n)));
                Point trial = pop[static_cast<size_t>(i)];
                for (size_t k = 0; k < n; ++k)
                    if (random.uniform() < 0.9 || k == jr) {
                        double v = pop[static_cast<size_t>(r1)][k] + 0.6 * (pop[static_cast<size_t>(r2)][k] - pop[static_cast<size_t>(r3)][k]);
                        if (v < 0.0)
                            v = random.uniform() * pop[static_cast<size_t>(i)][k];  // bounce back inside
                        if (v > 1.0)
                            v = pop[static_cast<size_t>(i)][k] + random.uniform() * (1.0 - pop[static_cast<size_t>(i)][k]);
                        trial[k] = v;
                    }
                trials.push_back(trial);
                parents.push_back({cand[static_cast<size_t>(i)].id, cand[static_cast<size_t>(r1)].id});
            }
            std::vector<Candidate> next = s.evaluate(trials, "differential-evolution", generation, parents);
            for (size_t i = 0; i < next.size(); ++i)
                if (!betterScalar(cand[i], next[i])) {  // the trial is at least as good
                    pop[i] = trials[i];
                    cand[i] = next[i];
                }
            if (next.size() < trials.size())
                break;
            double best = kInf, worst = -kInf, extent = 0.0;
            bool allFeasible = true;
            for (int i = 0; i < np; ++i) {
                const double m = s.merit(cand[static_cast<size_t>(i)]);
                best = std::min(best, m);
                worst = std::max(worst, m);
                allFeasible = allFeasible && cand[static_cast<size_t>(i)].feasible;
                extent = std::max(extent, infNorm(pop[static_cast<size_t>(i)], pop[0]));
            }
            if (allFeasible && std::isfinite(worst) && worst - best <= a.tolerance * (1.0 + std::abs(best)) && extent < 1e-4) {
                t.converged = true;
                t.reason = "the population has collapsed onto one design";
                return t;
            }
        }
        t.reason = s.stopReason();
        return t;
    }
};

// --- NSGA-II -------------------------------------------------------------------------------------------------------

class Nsga2 final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"nsga2", "NSGA-II (Pareto)", "population", true, true, false, true,
                "Elitist non-dominated sorting genetic algorithm with crowding distance, simulated binary crossover (eta 15, "
                "probability 0.9) and polynomial mutation (eta 20, probability 1/n), constraint domination. Approximates the "
                "Pareto front of competing objectives; it does not choose one design.",
                "runs until the budget is used (Pareto fronts have no single convergence value)"};
    }
    Termination run(EvaluationService& s) const override
    {
        const size_t n = s.dimension();
        const AlgorithmSettings& a = s.problem().algorithm;
        int np = a.populationSize > 0 ? a.populationSize : static_cast<int>(std::max<size_t>(20, std::min<size_t>(12 * n, 80)));
        np = std::max(4, np + (np % 2));
        Random random(a.seed);
        std::vector<Point> pop = latinHypercube(np, static_cast<int>(n), random);
        pop.front() = s.initialUnit();
        std::vector<Candidate> cand = s.evaluate(pop, "nsga2 initial", 0);
        Termination t;
        if (cand.size() < pop.size()) {
            t.reason = s.stopReason();
            return t;
        }
        const auto rankAndCrowd = [&](const std::vector<Candidate>& cs, std::vector<int>& rank, std::vector<double>& crowd) {
            rank = paretoRanks(cs, false);
            crowd.assign(cs.size(), 0.0);
            const int maxRank = cs.empty() ? -1 : *std::max_element(rank.begin(), rank.end());
            for (int r = 0; r <= maxRank; ++r) {
                std::vector<size_t> front;
                for (size_t i = 0; i < cs.size(); ++i)
                    if (rank[i] == r)
                        front.push_back(i);
                const size_t m = cs.empty() ? 0 : cs.front().scores.size();
                for (size_t k = 0; k < m; ++k) {
                    std::stable_sort(front.begin(), front.end(), [&](size_t x, size_t y) { return cs[x].scores[k] < cs[y].scores[k]; });
                    if (front.empty())
                        continue;
                    crowd[front.front()] = crowd[front.back()] = kInf;
                    const double span = cs[front.back()].scores[k] - cs[front.front()].scores[k];
                    if (!(span > 0.0) || !std::isfinite(span))
                        continue;
                    for (size_t i = 1; i + 1 < front.size(); ++i)
                        crowd[front[i]] += (cs[front[i + 1]].scores[k] - cs[front[i - 1]].scores[k]) / span;
                }
            }
            for (size_t i = 0; i < cs.size(); ++i)
                if (rank[i] < 0)
                    rank[i] = std::numeric_limits<int>::max();
        };
        std::vector<int> rank;
        std::vector<double> crowd;
        rankAndCrowd(cand, rank, crowd);
        const auto better = [&](size_t x, size_t y) { return rank[x] < rank[y] || (rank[x] == rank[y] && crowd[x] > crowd[y]); };
        for (int generation = 1; !s.shouldStop(); ++generation) {
            std::vector<Point> children;
            std::vector<std::vector<std::string>> parents;
            while (static_cast<int>(children.size()) < np) {
                const auto tournament = [&] {
                    const size_t x = static_cast<size_t>(random.below(np)), y = static_cast<size_t>(random.below(np));
                    return better(x, y) ? x : y;
                };
                const size_t p1 = tournament(), p2 = tournament();
                Point c1 = pop[p1], c2 = pop[p2];
                if (random.uniform() < 0.9)
                    for (size_t k = 0; k < n; ++k) {
                        if (random.uniform() > 0.5)
                            continue;
                        const double u = random.uniform();
                        const double eta = 15.0;
                        const double beta = u <= 0.5 ? std::pow(2.0 * u, 1.0 / (eta + 1.0)) : std::pow(1.0 / (2.0 * (1.0 - u)), 1.0 / (eta + 1.0));
                        const double a1 = c1[k], a2 = c2[k];
                        c1[k] = 0.5 * ((1.0 + beta) * a1 + (1.0 - beta) * a2);
                        c2[k] = 0.5 * ((1.0 - beta) * a1 + (1.0 + beta) * a2);
                    }
                for (Point* c : {&c1, &c2})
                    for (size_t k = 0; k < n; ++k)
                        if (random.uniform() < 1.0 / n) {
                            const double u = random.uniform(), eta = 20.0;
                            const double dq = u < 0.5 ? std::pow(2.0 * u, 1.0 / (eta + 1.0)) - 1.0 : 1.0 - std::pow(2.0 * (1.0 - u), 1.0 / (eta + 1.0));
                            (*c)[k] += dq;
                        }
                children.push_back(clampUnit(c1));
                children.push_back(clampUnit(c2));
                parents.push_back({cand[p1].id, cand[p2].id});
                parents.push_back({cand[p1].id, cand[p2].id});
            }
            children.resize(static_cast<size_t>(np));
            parents.resize(static_cast<size_t>(np));
            std::vector<Candidate> next = s.evaluate(children, "nsga2", generation, parents);
            // Environmental selection from parents + children.
            std::vector<Candidate> all = cand;
            std::vector<Point> allPts = pop;
            for (size_t i = 0; i < next.size(); ++i) {
                all.push_back(next[i]);
                allPts.push_back(children[i]);
            }
            std::vector<int> r;
            std::vector<double> cr;
            rankAndCrowd(all, r, cr);
            std::vector<size_t> order(all.size());
            std::iota(order.begin(), order.end(), size_t(0));
            std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) { return r[x] < r[y] || (r[x] == r[y] && cr[x] > cr[y]); });
            std::vector<Candidate> keep;
            std::vector<Point> keepPts;
            for (size_t i = 0; i < static_cast<size_t>(np); ++i) {
                keep.push_back(all[order[i]]);
                keepPts.push_back(allPts[order[i]]);
            }
            cand = keep;
            pop = keepPts;
            rankAndCrowd(cand, rank, crowd);
            if (next.size() < children.size())
                break;
        }
        t.reason = s.stopReason();
        return t;
    }
};

// --- Bayesian (Gaussian process + expected improvement) -----------------------------------------------------------------

class Bayesian final : public Algorithm {
public:
    AlgorithmInfo info() const override
    {
        return {"bayesian", "Bayesian optimization (Gaussian process)", "surrogate", false, false, false, true,
                "A Latin-hypercube initial design, then one design at a time chosen by the largest expected improvement of a "
                "Gaussian-process surrogate of the merit (2000 random and 200 local pool points). Every chosen design is evaluated "
                "with the physical model; the surrogate's prediction is recorded in the candidate's origin and is never used as a "
                "result. For expensive evaluations of up to about ten continuous variables.",
                "stops when the largest expected improvement is below tolerance x (1 + |best|) or the budget is used"};
    }
    Termination run(EvaluationService& s) const override
    {
        const size_t n = s.dimension();
        const AlgorithmSettings& a = s.problem().algorithm;
        Random random(a.seed);
        const int initial = std::clamp(a.samples > 0 ? a.samples : static_cast<int>(2 * n + 1), static_cast<int>(n) + 1, std::max(2, a.maxEvaluations / 2));
        std::vector<Point> start = latinHypercube(initial, static_cast<int>(n), random);
        start.front() = s.initialUnit();
        std::vector<Candidate> evaluated = s.evaluate(start, "bayesian initial design", 0);
        std::vector<Point> xs(start.begin(), start.begin() + static_cast<long>(evaluated.size()));
        std::vector<double> ys;
        for (const Candidate& c : evaluated)
            ys.push_back(s.merit(c));
        Termination t;
        for (int iteration = 1; !s.shouldStop(); ++iteration) {
            // Merits that could not be computed are replaced by the worst finite one plus a margin.
            double worst = -kInf, best = kInf;
            size_t bestIndex = 0;
            for (size_t i = 0; i < ys.size(); ++i)
                if (std::isfinite(ys[i])) {
                    worst = std::max(worst, ys[i]);
                    if (ys[i] < best) {
                        best = ys[i];
                        bestIndex = i;
                    }
                }
            if (!std::isfinite(best)) {
                t.reason = "no initial design could be evaluated";
                return t;
            }
            std::vector<double> y = ys;
            for (double& v : y)
                if (!std::isfinite(v))
                    v = worst + 0.1 * (std::abs(worst) + 1.0);
            auto gp = makeSurrogate("gaussian-process");
            if (!gp->fit(xs, y)) {
                t.reason = "the surrogate could not be fitted";
                return t;
            }
            std::vector<Point> pool = uniformSamples(2000, static_cast<int>(n), random);
            for (int k = 0; k < 200; ++k) {
                Point p = xs[bestIndex];
                for (double& v : p)
                    v += 0.05 * random.normal();
                pool.push_back(clampUnit(p));
            }
            double bestEi = -1.0;
            Prediction bestPrediction;
            Point next;
            for (const Point& p : pool) {
                const Prediction pr = gp->predict(p);
                const double ei = expectedImprovement(pr, best);
                if (ei > bestEi) {
                    bestEi = ei;
                    bestPrediction = pr;
                    next = p;
                }
            }
            if (bestEi <= a.tolerance * (1.0 + std::abs(best))) {
                t.converged = true;
                t.reason = "the expected improvement is below the tolerance";
                return t;
            }
            const std::string origin = "bayesian: surrogate predicted merit " + text(bestPrediction.mean) + " +/- " + text(bestPrediction.stddev) +
                                       " (a prediction, not a simulation)";
            auto c = s.evaluate({next}, origin, iteration);
            if (c.empty())
                break;
            xs.push_back(next);
            ys.push_back(s.merit(c.front()));
        }
        t.reason = s.stopReason();
        return t;
    }
};

}  // namespace

const std::vector<const Algorithm*>& algorithms()
{
    static const Sweep sweep;
    static const RandomSampling random;
    static const LatinHypercubeSampling lhs;
    static const NelderMead nm;
    static const Lbfgs lbfgs;
    static const DifferentialEvolution de;
    static const Nsga2 nsga2;
    static const Bayesian bayesian;
    static const std::vector<const Algorithm*> all{&nm, &lbfgs, &de, &nsga2, &bayesian, &sweep, &lhs, &random};
    return all;
}

const Algorithm* findAlgorithm(std::string_view id)
{
    for (const Algorithm* a : algorithms())
        if (a->info().id == id)
            return a;
    return nullptr;
}

ProblemTraits traitsOf(const OptimizationProblem& problem, const Capabilities& capabilities)
{
    ProblemTraits t;
    t.variables = problem.space.dimension();
    t.discrete = problem.space.hasDiscrete();
    t.objectives = problem.objectives.size();
    t.constraints = problem.constraints.size();
    if (const ModelInfo* m = capabilities.model(problem.modelId))
        t.secondsPerEvaluation = m->typicalSeconds;
    if (problem.robust.enabled)
        t.secondsPerEvaluation *= 1 + problem.robust.samples;
    return t;
}

std::vector<Recommendation> recommendAlgorithms(const ProblemTraits& t)
{
    std::vector<Recommendation> out;
    const bool expensive = t.secondsPerEvaluation >= 1.0;
    if (t.objectives > 1) {
        out.push_back({"nsga2", "several objectives: NSGA-II approximates the Pareto front of the trade-off"});
        out.push_back({"latin-hypercube", "a design-of-experiments sample shows the trade-off space without choosing weights"});
        out.push_back({"differential-evolution", "a weighted sum of the objectives, if the priorities are already known"});
        return out;
    }
    if (t.discrete) {
        out.push_back({"differential-evolution", "discrete or categorical variables: a population method that quantizes them"});
        if (t.variables <= 3)
            out.push_back({"sweep", "few variables: a grid shows every combination"});
        out.push_back({"latin-hypercube", "a first exploration of the design space"});
        return out;
    }
    if (expensive && t.variables <= 10)
        out.push_back({"bayesian", "expensive evaluations (about " + text(t.secondsPerEvaluation) +
                                       " s each) of few continuous variables: a surrogate chooses each simulation"});
    if (t.variables <= 2 && !expensive)
        out.push_back({"sweep", "one or two cheap variables: a sweep shows the whole response"});
    if (t.constraints == 0 || !expensive)
        out.push_back({"lbfgs", "smooth continuous problem: gradient-based with finite differences (2 evaluations per variable per step)"});
    out.push_back({"nelder-mead", "derivative-free local search, robust to noise and kinks, for up to about ten variables"});
    out.push_back({"differential-evolution", "global search when several local optima are possible"});
    return out;
}

}  // namespace resonant::optimization
