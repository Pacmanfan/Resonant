#include "optimization/Engine.h"

#include "foundation/Log.h"
#include "optimization/Algorithms.h"
#include "optimization/Random.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <fstream>
#include <numeric>
#include <condition_variable>
#include <thread>

namespace resonant::optimization {

namespace {

// Inverse of the standard normal distribution function (P. J. Acklam's
// rational approximation, relative error below 1.2e-9).
double inverseNormal(double p)
{
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02, 6.680131188771972e+01,
                               -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00};
    p = std::clamp(p, 1e-12, 1.0 - 1e-12);
    if (p < 0.02425) {
        const double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    if (p > 1.0 - 0.02425) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    const double q = p - 0.5, r = q * q;
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
}

std::string pad(size_t n)
{
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%05zu", n);
    return buffer;
}

}  // namespace

std::string utcNow()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof buffer, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

std::string newRunId()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof buffer, "%Y%m%dT%H%M%S", &tm);
    static std::atomic<unsigned> counter{0};
    Random r(static_cast<std::uint64_t>(t) ^ (static_cast<std::uint64_t>(++counter) << 32) ^
             static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    char suffix[8];
    std::snprintf(suffix, sizeof suffix, "%04x", static_cast<unsigned>(r.next() & 0xffffu));
    return std::string("OPT-") + buffer + "-" + suffix;
}

// --- cache -------------------------------------------------------------------------------------

std::optional<Evaluation> EvaluationCache::find(const std::string& key, bool* fromCheckpoint) const
{
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(key);
    if (it == entries_.end())
        return std::nullopt;
    if (fromCheckpoint)
        *fromCheckpoint = it->second.second;
    return it->second.first;
}

void EvaluationCache::store(const std::string& key, const Evaluation& evaluation, bool fromCheckpoint)
{
    std::lock_guard lock(mutex_);
    entries_[key] = {evaluation, fromCheckpoint};
}

size_t EvaluationCache::size() const
{
    std::lock_guard lock(mutex_);
    return entries_.size();
}

nlohmann::json EvaluationCache::toJson() const
{
    std::lock_guard lock(mutex_);
    nlohmann::json out = nlohmann::json::array();
    for (const auto& [k, v] : entries_)
        out.push_back({{"key", k}, {"evaluation", optimization::toJson(v.first)}});
    return out;
}

void EvaluationCache::load(const nlohmann::json& entries)
{
    if (!entries.is_array())
        return;
    std::lock_guard lock(mutex_);
    for (const auto& e : entries)
        if (e.contains("key") && e.contains("evaluation"))
            entries_[e["key"].get<std::string>()] = {evaluationFromJson(e["evaluation"]), true};
}

// --- service ----------------------------------------------------------------------------------

EvaluationService::EvaluationService(OptimizationProblem problem, std::shared_ptr<const DesignEvaluator> evaluator, ServiceOptions options,
                                     JobContext* context)
    : problem_(std::move(problem)), evaluator_(std::move(evaluator)), options_(std::move(options)), context_(context),
      start_(std::chrono::steady_clock::now())
{
    if (!options_.cache)
        options_.cache = std::make_shared<EvaluationCache>();
    free_ = problem_.space.freeVariables();
    constants_ = evaluator_->capabilities().constants;
    if (!evaluator_->capabilities().threadSafe)
        options_.parallel = 1;
    options_.parallel = std::max(1, options_.parallel);
    if (problem_.robust.enabled) {
        // Common random numbers: the same perturbations for every design, so
        // designs are compared under identical conditions. Stratified (Latin
        // hypercube) in probability, mapped through each distribution.
        const RobustSettings& r = problem_.robust;
        for (const UncertainParameter& u : r.parameters)
            if (const DesignVariable* v = evaluator_->capabilities().variable(u.binding))
                perturbationBase_[u.binding] = v->initial;
        Random random(r.seed);
        const auto u = latinHypercube(r.samples, static_cast<int>(r.parameters.size()), random, 5);
        for (int s = 0; s < r.samples; ++s) {
            std::map<std::string, double, std::less<>> delta;
            for (size_t k = 0; k < r.parameters.size(); ++k) {
                const UncertainParameter& p = r.parameters[k];
                const double q = u[static_cast<size_t>(s)][k];
                double z = p.distribution == Distribution::Uniform ? 2.0 * q - 1.0
                                                                    : std::clamp(inverseNormal(q), -p.truncate, p.truncate);
                delta[p.binding] += z * p.width;  // relative widths are scaled by the design value when applied
            }
            robustSamples_.push_back(std::move(delta));
        }
    }
}

std::vector<double> EvaluationService::initialUnit() const { return problem_.space.toUnit(problem_.space.initialPoint(constants_)); }

bool EvaluationService::cancelled() const { return context_ && context_->cancelRequested(); }

int EvaluationService::remaining() const { return std::max(0, problem_.algorithm.maxEvaluations - used_); }

bool EvaluationService::shouldStop() const
{
    if (cancelled() || remaining() <= 0)
        return true;
    if (problem_.algorithm.maxSeconds > 0.0 &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count() >= problem_.algorithm.maxSeconds)
        return true;
    return false;
}

std::string EvaluationService::stopReason() const
{
    if (cancelled())
        return "cancelled";
    if (remaining() <= 0)
        return "evaluation budget used (" + std::to_string(problem_.algorithm.maxEvaluations) + " candidates)";
    if (problem_.algorithm.maxSeconds > 0.0 &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count() >= problem_.algorithm.maxSeconds)
        return "time limit reached";
    return {};
}

std::string EvaluationService::cacheKey(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId) const
{
    DesignPoint p;
    p.values = bindings;
    std::string key = evaluator_->snapshotKey() + "|" + modelId + "|";
    if (problem_.algorithm.memoryBudgetBytes > 0.0)
        key += "mem=" + std::to_string(problem_.algorithm.memoryBudgetBytes) + "|";
    return key + DesignSpace::canonical(p);
}

Evaluation EvaluationService::evaluateOne(const DesignPoint& point, const std::string& modelId, bool& cached)
{
    std::map<std::string, double, std::less<>> bindings = problem_.space.bindings(point, &perturbationBase_);
    if (problem_.algorithm.memoryBudgetBytes > 0.0)
        bindings["memoryBudgetBytes"] = problem_.algorithm.memoryBudgetBytes;
    const std::string key = cacheKey(bindings, modelId);
    bool restored = false;
    if (auto hit = options_.cache->find(key, &restored)) {
        cached = true;
        ++cacheHits_;
        return *hit;
    }
    cached = false;
    const auto t0 = std::chrono::steady_clock::now();
    Evaluation e;
    try {
        e = evaluator_->evaluate(bindings, modelId, context_);
    } catch (const std::exception& ex) {
        e.status = Evaluation::Status::Failed;
        e.message = std::string("the evaluation threw: ") + ex.what();
    }
    e.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    e.model.snapshotKey = evaluator_->snapshotKey();
    e.model.adapterId = evaluator_->capabilities().adapterId;
    e.model.adapterVersion = evaluator_->capabilities().version;
    ++physics_;
    if (e.status != Evaluation::Status::Cancelled)
        options_.cache->store(key, e);
    return e;
}

Candidate EvaluationService::score(Candidate c, const std::vector<Evaluation>& samples) const
{
    const OptimizationProblem& p = problem_;
    const auto quantityOf = [](const Evaluation& e, const std::string& key) {
        const auto it = e.quantities.find(key);
        return it == e.quantities.end() ? std::numeric_limits<double>::quiet_NaN() : it->second;
    };
    std::map<std::string, double, std::less<>> exprValues = constants_;
    for (const auto& [k, v] : c.point.values)
        exprValues[k] = v;
    const auto constraintValue = [&](const DesignConstraint& k, const Evaluation& e, std::string& message) {
        if (k.isExpression()) {
            auto expr = Expression::parse(k.expression());
            auto v = expr ? expr.value().evaluate(exprValues) : Result<double>::failure(expr.message());
            if (!v) {
                message = v.message();
                return std::numeric_limits<double>::quiet_NaN();
            }
            return v.value();
        }
        if (!e.ok()) {
            message = "not evaluated: " + e.message;
            return std::numeric_limits<double>::quiet_NaN();
        }
        const double v = quantityOf(e, k.quantity);
        if (!std::isfinite(v))
            message = "the model did not provide '" + k.quantity + "'";
        return v;
    };

    const Evaluation& nominal = c.evaluation;
    c.objectiveValues.assign(p.objectives.size(), std::numeric_limits<double>::quiet_NaN());
    c.scores.assign(p.objectives.size(), std::numeric_limits<double>::infinity());
    if (nominal.ok())
        for (size_t i = 0; i < p.objectives.size(); ++i) {
            c.objectiveValues[i] = quantityOf(nominal, p.objectives[i].quantity);
            if (std::isfinite(c.objectiveValues[i]))
                c.scores[i] = p.objectives[i].score(c.objectiveValues[i]);
        }

    if (!samples.empty()) {
        RobustStatistics s;
        s.samples = static_cast<int>(samples.size());
        s.measure = std::string(toString(p.robust.measure));
        for (size_t i = 0; i < p.objectives.size(); ++i) {
            std::vector<double> values, scores;
            for (const Evaluation& e : samples) {
                const double v = e.ok() ? quantityOf(e, p.objectives[i].quantity) : std::numeric_limits<double>::quiet_NaN();
                values.push_back(v);
                scores.push_back(std::isfinite(v) ? p.objectives[i].score(v) : std::numeric_limits<double>::infinity());
            }
            const auto finite = [](const std::vector<double>& x) {
                std::vector<double> f;
                for (double v : x)
                    if (std::isfinite(v))
                        f.push_back(v);
                return f;
            };
            const std::vector<double> fv = finite(values), fs = finite(scores);
            const auto mean = [](const std::vector<double>& x) {
                return x.empty() ? std::numeric_limits<double>::quiet_NaN() : std::accumulate(x.begin(), x.end(), 0.0) / x.size();
            };
            const auto stdev = [&](const std::vector<double>& x) {
                if (x.size() < 2)
                    return 0.0;
                const double m = mean(x);
                double q = 0.0;
                for (double v : x)
                    q += (v - m) * (v - m);
                return std::sqrt(q / (x.size() - 1));
            };
            const bool allFinite = fv.size() == values.size();
            s.mean.push_back(mean(fv));
            s.stddev.push_back(stdev(fv));
            s.minimum.push_back(fv.empty() ? NAN : *std::min_element(fv.begin(), fv.end()));
            s.maximum.push_back(fv.empty() ? NAN : *std::max_element(fv.begin(), fv.end()));
            const double worst = allFinite ? *std::max_element(scores.begin(), scores.end()) : std::numeric_limits<double>::infinity();
            s.worstScore.push_back(worst);
            double robustScore = std::numeric_limits<double>::infinity();
            if (allFinite) {
                switch (p.robust.measure) {
                case RobustMeasure::Mean: robustScore = mean(fs); break;
                case RobustMeasure::MeanPlusSigma: robustScore = mean(fs) + p.robust.k * stdev(fs); break;
                case RobustMeasure::WorstCase: robustScore = worst; break;
                }
            }
            c.scores[i] = robustScore;
        }
        c.robust = std::move(s);
    }

    c.constraints.clear();
    c.hardViolation = 0.0;
    c.softPenalty = 0.0;
    bool feasible = nominal.ok();
    for (const DesignConstraint& k : p.constraints) {
        ConstraintResult r;
        r.id = k.id;
        r.limit = k.limit;
        r.hard = k.hard;
        r.value = constraintValue(k, nominal, r.message);
        r.evaluated = std::isfinite(r.value);
        r.violation = r.evaluated ? k.violation(r.value) : std::numeric_limits<double>::infinity();
        if (!samples.empty() && !k.isExpression()) {
            int met = 0;
            double worstViolation = r.violation, worstValue = r.value;
            for (const Evaluation& e : samples) {
                std::string m;
                const double v = constraintValue(k, e, m);
                const double viol = std::isfinite(v) ? k.violation(v) : std::numeric_limits<double>::infinity();
                if (viol <= 0.0)
                    ++met;
                if (viol > worstViolation) {
                    worstViolation = viol;
                    worstValue = v;
                }
            }
            const double probability = static_cast<double>(met) / samples.size();
            c.robust->constraintProbability.push_back(probability);
            if (k.hard && probability < p.robust.requiredProbability) {
                r.value = worstValue;
                r.violation = std::max(worstViolation, 1e-300);
                r.message = "met in " + std::to_string(met) + " of " + std::to_string(samples.size()) + " perturbed samples";
            }
        } else if (!samples.empty()) {
            c.robust->constraintProbability.push_back(r.violation <= 0.0 ? 1.0 : 0.0);
        }
        r.satisfied = r.evaluated && r.violation <= 0.0;
        const double normalized = std::isfinite(r.violation) ? r.violation / k.scale : 1e6;
        if (k.hard) {
            if (!r.satisfied) {
                feasible = false;
                c.hardViolation += normalized;
            }
        } else {
            c.softPenalty += k.penalty * normalized * normalized;
        }
        c.constraints.push_back(std::move(r));
    }
    // Bounds and locks are hard constraints of the design space itself.
    const auto bounds = p.space.boundViolations(c.point);
    if (!bounds.empty()) {
        feasible = false;
        c.hardViolation += 1.0;
        for (const std::string& b : bounds)
            c.evaluation.warnings.push_back(b);
    }
    c.feasible = feasible;
    double weighted = c.softPenalty;
    for (size_t i = 0; i < p.objectives.size(); ++i)
        weighted += p.objectives[i].weight * c.scores[i];
    c.weightedScore = nominal.ok() && std::isfinite(weighted) ? weighted : std::numeric_limits<double>::infinity();
    return c;
}

double EvaluationService::merit(const Candidate& c) const
{
    if (!c.evaluated() || !std::isfinite(c.weightedScore))
        return std::numeric_limits<double>::infinity();
    if (c.feasible)
        return c.weightedScore;
    return c.weightedScore + 1e3 * (c.hardViolation + c.hardViolation * c.hardViolation);
}

std::vector<Candidate> EvaluationService::evaluate(const std::vector<std::vector<double>>& unitPoints, const std::string& origin, int generation,
                                                   const std::vector<std::vector<std::string>>& parents)
{
    std::vector<DesignPoint> points;
    for (const auto& u : unitPoints) {
        auto p = problem_.space.fromUnit(u, constants_);
        if (!p) {
            DesignPoint bad = problem_.space.initialPoint(constants_);
            note("a derived value could not be computed: " + p.message());
            points.push_back(bad);
            continue;
        }
        points.push_back(p.take());
    }
    return evaluatePoints(points, origin, generation, parents);
}

std::vector<Candidate> EvaluationService::evaluatePoints(const std::vector<DesignPoint>& input, const std::string& origin, int generation,
                                                         const std::vector<std::vector<std::string>>& parents, const std::string& modelId,
                                                         bool ignoreBudget)
{
    std::vector<DesignPoint> points = input;
    if (!ignoreBudget && static_cast<int>(points.size()) > remaining())
        points.resize(static_cast<size_t>(remaining()));
    if (shouldStop() && !ignoreBudget)
        points.clear();
    if (cancelled())
        points.clear();
    const std::string model = modelId.empty() ? problem_.modelId : modelId;
    const size_t samples = problem_.robust.enabled ? robustSamples_.size() : 0;
    const size_t perDesign = 1 + samples;
    std::vector<Evaluation> results(points.size() * perDesign);
    std::vector<char> cachedFlags(results.size(), 0);

    // The work items: design d, sample s (s = 0 nominal). Worker threads
    // evaluate them; this thread stores each design as soon as it and every
    // design before it are complete, so candidates appear while a large
    // batch runs and their order never depends on thread timing.
    std::atomic<size_t> next{0};
    const size_t total = results.size();
    std::mutex doneMutex;
    std::condition_variable doneSignal;
    std::vector<int> remainingItems(points.size(), static_cast<int>(perDesign));
    const auto worker = [&] {
        for (;;) {
            const size_t i = next.fetch_add(1);
            if (i >= total)
                return;
            const size_t d = i / perDesign, s = i % perDesign;
            if (cancelled()) {
                results[i].status = Evaluation::Status::Cancelled;
            } else {
                DesignPoint point = points[d];
                if (s > 0) {
                    for (const auto& [binding, z] : robustSamples_[s - 1]) {
                        double delta = z;
                        for (const UncertainParameter& u : problem_.robust.parameters)
                            if (u.binding == binding && u.relative) {
                                const auto b = problem_.space.bindings(point, &perturbationBase_);
                                const auto it = b.find(binding);
                                delta = z * std::abs(it == b.end() ? 0.0 : it->second);
                            }
                        point.extra[binding] = delta;
                    }
                }
                bool cached = false;
                results[i] = evaluateOne(point, model, cached);
                cachedFlags[i] = cached ? 1 : 0;
            }
            {
                std::lock_guard lock(doneMutex);
                --remainingItems[d];
            }
            doneSignal.notify_all();
        }
    };
    const int threads = std::max(1, std::min<int>(options_.parallel, static_cast<int>(total)));
    std::vector<std::thread> pool;
    if (total > 0)
        for (int t = 0; t < threads; ++t)
            pool.emplace_back(worker);

    std::vector<Candidate> out;
    bool stopped = false;
    for (size_t d = 0; d < points.size(); ++d) {
        {
            std::unique_lock lock(doneMutex);
            doneSignal.wait(lock, [&] { return remainingItems[d] == 0; });
        }
        if (stopped)
            continue;
        bool anyCancelled = false;
        for (size_t s = 0; s < perDesign; ++s)
            anyCancelled = anyCancelled || results[d * perDesign + s].status == Evaluation::Status::Cancelled;
        if (anyCancelled) {
            stopped = true;  // a cancelled design is not stored: a restart evaluates it again
            continue;
        }
        Candidate c;
        c.generation = generation;
        c.origin = origin;
        c.point = points[d];
        c.evaluation = results[d * perDesign];
        c.fromCache = cachedFlags[d * perDesign] != 0;
        if (d < parents.size())
            c.parents = parents[d];
        std::vector<Evaluation> sampleResults;
        for (size_t s = 1; s < perDesign; ++s)
            sampleResults.push_back(results[d * perDesign + s]);
        c.seconds = 0.0;
        for (size_t s = 0; s < perDesign; ++s)
            c.seconds += results[d * perDesign + s].seconds;
        c = score(std::move(c), sampleResults);
        {
            std::lock_guard lock(mutex_);
            c.id = options_.runId + "-C" + pad(history_.size() + 1);
            bool restored = false;
            if (c.fromCache) {
                auto bindings = problem_.space.bindings(c.point, &perturbationBase_);
                if (problem_.algorithm.memoryBudgetBytes > 0.0)
                    bindings["memoryBudgetBytes"] = problem_.algorithm.memoryBudgetBytes;
                options_.cache->find(cacheKey(bindings, model), &restored);
            }
            c.replayed = restored;
            history_.push_back(c);
            if (!ignoreBudget)
                ++used_;
        }
        if (options_.onCandidate)
            options_.onCandidate(c);
        out.push_back(std::move(c));
    }
    for (auto& t : pool)
        t.join();
    return out;
}

std::vector<Candidate> EvaluationService::history() const
{
    std::lock_guard lock(mutex_);
    return history_;
}

size_t EvaluationService::historySize() const
{
    std::lock_guard lock(mutex_);
    return history_.size();
}

void EvaluationService::note(const std::string& text)
{
    std::lock_guard lock(mutex_);
    if (std::find(notes_.begin(), notes_.end(), text) == notes_.end())
        notes_.push_back(text);
}

std::vector<std::string> EvaluationService::notes() const
{
    std::lock_guard lock(mutex_);
    return notes_;
}

// --- run records ---------------------------------------------------------------------------------

nlohmann::json toJson(const RunRecord& r, const EvaluationCache* cache)
{
    nlohmann::json candidates = nlohmann::json::array();
    for (const Candidate& c : r.candidates)
        candidates.push_back(toJson(c));
    nlohmann::json j{{"format", kRunFormat},
                     {"version", kRunVersion},
                     {"runId", r.runId},
                     {"problem", r.problem},
                     {"algorithm", r.algorithm},
                     {"settings", toJson(r.settings)},
                     {"snapshotKey", r.snapshotKey},
                     {"startedUtc", r.startedUtc},
                     {"finishedUtc", r.finishedUtc},
                     {"complete", r.complete},
                     {"cancelled", r.cancelled},
                     {"resumed", r.resumed},
                     {"termination", {{"reason", r.termination.reason}, {"converged", r.termination.converged}, {"notes", r.termination.notes}}},
                     {"candidates", candidates},
                     {"physicsEvaluations", r.physicsEvaluations},
                     {"cacheHits", r.cacheHits},
                     {"seconds", r.seconds},
                     {"notes", r.notes},
                     {"pareto", r.pareto}};
    if (r.best)
        j["best"] = *r.best;
    if (cache)
        j["cache"] = cache->toJson();
    return j;
}

Result<RunRecord> runRecordFromJson(const nlohmann::json& j, EvaluationCache* cacheOut)
{
    using R = Result<RunRecord>;
    if (!j.is_object() || j.value("format", std::string()) != kRunFormat)
        return R::failure("not an optimization run record");
    if (j.value("version", 0) > kRunVersion)
        return R::failure("the run record has a newer version than this build reads");
    try {
        RunRecord r;
        r.runId = j.value("runId", std::string());
        r.problem = j.value("problem", nlohmann::json::object());
        r.algorithm = j.value("algorithm", std::string());
        r.settings = algorithmSettingsFromJson(j.value("settings", nlohmann::json::object()));
        r.snapshotKey = j.value("snapshotKey", std::string());
        r.startedUtc = j.value("startedUtc", std::string());
        r.finishedUtc = j.value("finishedUtc", std::string());
        r.complete = j.value("complete", false);
        r.cancelled = j.value("cancelled", false);
        r.resumed = j.value("resumed", false);
        const auto t = j.value("termination", nlohmann::json::object());
        r.termination.reason = t.value("reason", std::string());
        r.termination.converged = t.value("converged", false);
        r.termination.notes = t.value("notes", std::vector<std::string>());
        for (const auto& c : j.value("candidates", nlohmann::json::array()))
            r.candidates.push_back(candidateFromJson(c));
        r.physicsEvaluations = j.value("physicsEvaluations", 0);
        r.cacheHits = j.value("cacheHits", 0);
        r.seconds = j.value("seconds", 0.0);
        r.notes = j.value("notes", std::vector<std::string>());
        r.pareto = j.value("pareto", std::vector<size_t>());
        if (j.contains("best") && j["best"].is_number_unsigned())
            r.best = j["best"].get<size_t>();
        if (cacheOut && j.contains("cache"))
            cacheOut->load(j["cache"]);
        return r;
    } catch (const std::exception& e) {
        return R::failure(std::string("run record: ") + e.what());
    }
}

Result<ResumeState> resumeFromCheckpoint(const nlohmann::json& checkpoint)
{
    auto cache = std::make_shared<EvaluationCache>();
    auto record = runRecordFromJson(checkpoint, cache.get());
    if (!record)
        return Result<ResumeState>::failure(record.message());
    auto problem = problemFromJson(record.value().problem);
    if (!problem)
        return Result<ResumeState>::failure("the checkpoint's problem: " + problem.message());
    ResumeState s;
    s.problem = problem.take();
    s.runId = record.value().runId;
    s.cache = cache;
    s.recordedCandidates = record.value().candidates.size();
    return s;
}

Result<RunRecord> runOptimization(const OptimizationProblem& problem, std::shared_ptr<const DesignEvaluator> evaluator,
                                  const RunOptions& options, JobContext* context)
{
    using R = Result<RunRecord>;
    if (!evaluator)
        return R::failure("no evaluator");
    OptimizationProblem p = problem;
    std::vector<std::string> warnings;
    if (Status s = p.validateAgainst(evaluator->capabilities(), &warnings); !s)
        return R::failure(s.message());
    p.fillUnits(evaluator->capabilities());
    const Algorithm* algorithm = findAlgorithm(p.algorithm.algorithm);
    if (!algorithm)
        return R::failure("unknown optimization method '" + p.algorithm.algorithm + "'");
    const AlgorithmInfo info = algorithm->info();
    if (!info.handlesDiscrete && p.space.hasDiscrete())
        return R::failure("'" + info.name + "' works on continuous variables only; the design has integer, stepped or categorical "
                          "variables - choose differential evolution, a sweep or a sampling method");
    if (p.objectives.size() > 1 && !info.multiObjective)
        warnings.push_back("'" + info.name + "' optimizes one number: the objectives are combined as the weighted sum of their scores");

    RunRecord record;
    record.runId = options.runId.empty() ? newRunId() : options.runId;
    record.problem = toJson(p);
    record.algorithm = p.algorithm.algorithm;
    record.settings = p.algorithm;
    record.snapshotKey = evaluator->snapshotKey();
    record.startedUtc = utcNow();
    record.resumed = options.resumed;
    record.notes = warnings;

    ServiceOptions so;
    so.runId = record.runId;
    so.cache = options.cache ? options.cache : std::make_shared<EvaluationCache>();
    const int hardware = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    so.parallel = p.algorithm.parallel > 0 ? std::min(p.algorithm.parallel, hardware) : std::max(1, hardware / 2);
    std::mutex checkpointMutex;
    int sinceCheckpoint = 0;
    EvaluationService* servicePtr = nullptr;
    so.onCandidate = [&](const Candidate& c) {
        if (options.onCandidate)
            options.onCandidate(c);
        if (options.checkpoint && p.algorithm.checkpointEvery > 0) {
            std::lock_guard lock(checkpointMutex);
            if (++sinceCheckpoint >= p.algorithm.checkpointEvery && servicePtr) {
                sinceCheckpoint = 0;
                RunRecord partial = record;
                partial.candidates = servicePtr->history();  // already holds c
                options.checkpoint(toJson(partial, so.cache.get()));
            }
        }
    };
    const auto t0 = std::chrono::steady_clock::now();
    EvaluationService service(p, evaluator, so, context);
    servicePtr = &service;
    Termination termination;
    try {
        termination = algorithm->run(service);
    } catch (const std::exception& e) {
        termination.reason = std::string("the method failed: ") + e.what();
    }
    record.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    record.candidates = service.history();
    record.physicsEvaluations = service.physicsEvaluations();
    record.cacheHits = service.cacheHits();
    record.cancelled = service.cancelled();
    if (record.cancelled)
        termination.reason = "cancelled";
    record.termination = termination;
    for (const std::string& n : service.notes())
        record.notes.push_back(n);
    record.finishedUtc = utcNow();
    record.complete = !record.cancelled;

    // Ranking among candidates of the problem's model only.
    const std::vector<int> ranks = paretoRanks(record.candidates, true);
    for (size_t i = 0; i < record.candidates.size(); ++i)
        record.candidates[i].paretoRank = record.candidates[i].evaluation.model.modelId == p.modelId ? ranks[i] : -1;
    record.pareto = paretoFront(record.candidates, p.modelId);
    // One best candidate only for one objective: with competing objectives
    // the answer is the Pareto front, and the user chooses by priority.
    if (p.objectives.size() == 1)
        for (size_t i = 0; i < record.candidates.size(); ++i) {
            const Candidate& c = record.candidates[i];
            if (!c.feasible || c.evaluation.model.modelId != p.modelId)
                continue;
            if (!record.best || betterScalar(c, record.candidates[*record.best]))
                record.best = i;
        }
    if (p.objectives.size() > 1)
        record.notes.push_back("several objectives: no single candidate is optimal for all of them; inspect the Pareto front (" +
                               std::to_string(record.pareto.size()) + " non-dominated feasible candidates) and choose by priority");
    if (!record.best && record.pareto.empty() && !record.candidates.empty())
        record.notes.push_back("no evaluated candidate meets every hard constraint");
    if (options.checkpoint)
        options.checkpoint(toJson(record, so.cache.get()));
    return record;
}

// --- job -----------------------------------------------------------------------------------------

OptimizationJob::OptimizationJob(OptimizationProblem problem, std::shared_ptr<const DesignEvaluator> evaluator, RunOptions options,
                                 std::string checkpointPath)
    : problem_(std::move(problem)), evaluator_(std::move(evaluator)), options_(std::move(options)), checkpointPath_(std::move(checkpointPath))
{
}

std::string OptimizationJob::name() const { return "Optimization: " + (problem_.name.empty() ? problem_.id : problem_.name); }

JobEnd OptimizationJob::run(JobContext& context)
{
    RunOptions options = options_;
    const int budget = std::max(1, problem_.algorithm.maxEvaluations);
    options.onCandidate = [this, &context, budget, user = options_.onCandidate](const Candidate& c) {
        size_t count;
        {
            std::lock_guard lock(mutex_);
            live_.push_back(c);
            count = live_.size();
        }
        context.reportProgress(std::min(1.0, static_cast<double>(count) / budget),
                               std::to_string(count) + " candidate(s)" + (c.feasible ? "" : " - last infeasible"));
        if (user)
            user(c);
    };
    options.checkpoint = [this, user = options_.checkpoint](const nlohmann::json& j) {
        {
            std::lock_guard lock(mutex_);
            checkpoint_ = j;
        }
        if (!checkpointPath_.empty()) {
            const std::string temporary = checkpointPath_ + ".tmp";
            std::ofstream(temporary) << j.dump() << "\n";
            std::rename(temporary.c_str(), checkpointPath_.c_str());
        }
        if (user)
            user(j);
    };
    auto record = runOptimization(problem_, evaluator_, options, &context);
    if (!record)
        return JobEnd::failed(record.message());
    const bool cancelled = record.value().cancelled;
    const std::string summary = std::to_string(record.value().candidates.size()) + " candidates, " + record.value().termination.reason;
    {
        std::lock_guard lock(mutex_);
        result_ = record.take();
    }
    return cancelled ? JobEnd::cancelled() : JobEnd::completed(summary);
}

std::vector<Candidate> OptimizationJob::candidates() const
{
    std::lock_guard lock(mutex_);
    return live_;
}

std::optional<RunRecord> OptimizationJob::result() const
{
    std::lock_guard lock(mutex_);
    return result_;
}

nlohmann::json OptimizationJob::checkpoint() const
{
    std::lock_guard lock(mutex_);
    return checkpoint_;
}

}  // namespace resonant::optimization
