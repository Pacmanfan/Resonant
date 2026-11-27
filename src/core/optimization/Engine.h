#pragma once

// Running an optimization (RES-OPT-018, RES-OPT-019, RES-OPT-025).
//
// The EvaluationService is what an algorithm talks to. It turns points of
// the unit cube into complete designs, evaluates them through the bound
// adapter - in parallel up to a limit, each on its own reconstruction of the
// design snapshot - scores objectives and constraints, applies robust
// sampling when the problem asks for it, and stores every result as a
// Candidate in submission order (so the outcome does not depend on thread
// timing). It stops proposing work when the budget, the time limit or a
// cancellation is reached.
//
// Caching: an evaluation is reused only when the snapshot key, the model and
// every applied value are identical (the full dependency state).
//
// Checkpoints are run records with `complete: false`. Restarting reloads the
// evaluated designs into the cache and runs the (deterministic) algorithm
// again from its seed: it replays the recorded candidates without physics
// and continues where the interrupted run stopped, so the finished result
// equals that of an uninterrupted run.
#include "foundation/Result.h"
#include "optimization/Candidate.h"
#include "optimization/Problem.h"
#include "simulation/Job.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace resonant::optimization {

inline constexpr const char* kRunFormat = "resonant.optimization.run";
inline constexpr int kRunVersion = 1;

// Evaluations keyed by their full dependency state. Thread-safe.
class EvaluationCache {
public:
    std::optional<Evaluation> find(const std::string& key, bool* fromCheckpoint = nullptr) const;
    void store(const std::string& key, const Evaluation& evaluation, bool fromCheckpoint = false);
    size_t size() const;
    nlohmann::json toJson() const;
    void load(const nlohmann::json& entries);  // marks the entries as restored from a checkpoint

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::pair<Evaluation, bool>> entries_;
};

struct Termination {
    std::string reason;       // what stopped the method
    bool converged = false;   // the method's own convergence test was met
    std::vector<std::string> notes;
};

struct ServiceOptions {
    std::string runId = "RUN";
    int parallel = 1;
    std::shared_ptr<EvaluationCache> cache;  // null: a private one
    std::function<void(const Candidate&)> onCandidate;  // after each stored candidate (any thread, serialized)
};

class EvaluationService {
public:
    EvaluationService(OptimizationProblem problem, std::shared_ptr<const DesignEvaluator> evaluator, ServiceOptions options,
                      JobContext* context = nullptr);

    const OptimizationProblem& problem() const { return problem_; }
    const DesignSpace& space() const { return problem_.space; }
    const DesignEvaluator& evaluator() const { return *evaluator_; }
    size_t dimension() const { return free_.size(); }
    const std::map<std::string, double, std::less<>>& constants() const { return constants_; }
    std::vector<double> initialUnit() const;

    // Evaluates unit-cube points of the free variables (at most remaining()).
    std::vector<Candidate> evaluate(const std::vector<std::vector<double>>& unitPoints, const std::string& origin, int generation,
                                    const std::vector<std::vector<std::string>>& parents = {});
    // Evaluates complete design points, optionally with another model (re-evaluation).
    std::vector<Candidate> evaluatePoints(const std::vector<DesignPoint>& points, const std::string& origin, int generation,
                                          const std::vector<std::vector<std::string>>& parents = {}, const std::string& modelId = {},
                                          bool ignoreBudget = false);

    // The scalar an algorithm minimizes: weighted score + soft penalty,
    // plus an exterior penalty for hard violations; +infinity when the
    // design could not be evaluated.
    double merit(const Candidate& candidate) const;
    bool shouldStop() const;
    std::string stopReason() const;
    int remaining() const;
    int used() const { return used_; }
    bool cancelled() const;

    std::vector<Candidate> history() const;
    size_t historySize() const;
    int physicsEvaluations() const { return physics_.load(); }
    int cacheHits() const { return cacheHits_.load(); }
    void note(const std::string& text);
    std::vector<std::string> notes() const;
    // Robust optimization: the perturbations (by binding) of sample s.
    const std::vector<std::map<std::string, double, std::less<>>>& robustSamples() const { return robustSamples_; }

private:
    struct Job;
    Evaluation evaluateOne(const DesignPoint& point, const std::string& modelId, bool& cached);
    Candidate score(Candidate candidate, const std::vector<Evaluation>& samples) const;
    std::string cacheKey(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId) const;

    OptimizationProblem problem_;
    std::shared_ptr<const DesignEvaluator> evaluator_;
    ServiceOptions options_;
    JobContext* context_;
    std::vector<const DesignVariable*> free_;
    std::map<std::string, double, std::less<>> constants_;
    std::map<std::string, double, std::less<>> perturbationBase_;  // present values of perturbed bindings outside the design
    std::vector<std::map<std::string, double, std::less<>>> robustSamples_;  // relative or absolute deltas per binding
    std::chrono::steady_clock::time_point start_;
    int used_ = 0;
    std::atomic<int> physics_{0};
    std::atomic<int> cacheHits_{0};
    mutable std::mutex mutex_;
    std::vector<Candidate> history_;
    std::vector<std::string> notes_;
};

// The record of a run (also the checkpoint format).
struct RunRecord {
    std::string runId;
    nlohmann::json problem;  // the problem as it was run
    std::string algorithm;
    AlgorithmSettings settings;
    std::string snapshotKey;
    std::string startedUtc, finishedUtc;
    bool complete = false;
    bool cancelled = false;
    bool resumed = false;
    Termination termination;
    std::vector<Candidate> candidates;
    int physicsEvaluations = 0;
    int cacheHits = 0;
    double seconds = 0.0;
    std::vector<std::string> notes;
    std::optional<size_t> best;   // one objective only: the best feasible candidate
    std::vector<size_t> pareto;   // feasible non-dominated candidates (several objectives)
};

nlohmann::json toJson(const RunRecord& record, const EvaluationCache* cache = nullptr);
Result<RunRecord> runRecordFromJson(const nlohmann::json& value, EvaluationCache* cacheOut = nullptr);

struct RunOptions {
    std::string runId;                        // empty: generated
    std::shared_ptr<EvaluationCache> cache;   // preloaded when resuming
    bool resumed = false;
    // Called with a checkpoint (an incomplete run record with the cache)
    // every `checkpointEvery` candidates and at the end.
    std::function<void(const nlohmann::json&)> checkpoint;
    std::function<void(const Candidate&)> onCandidate;
};

// Runs the problem's algorithm to completion (or cancellation) on the
// calling thread. Fails only when the problem cannot run at all.
Result<RunRecord> runOptimization(const OptimizationProblem& problem, std::shared_ptr<const DesignEvaluator> evaluator,
                                  const RunOptions& options = {}, JobContext* context = nullptr);
// Prepares a restart from a checkpoint: the problem, the run id and the
// cache of evaluations it holds.
struct ResumeState {
    OptimizationProblem problem;
    std::string runId;
    std::shared_ptr<EvaluationCache> cache;
    size_t recordedCandidates = 0;
};
Result<ResumeState> resumeFromCheckpoint(const nlohmann::json& checkpoint);

// A run as a background job. Completed candidates can be inspected while it
// runs; the latest checkpoint is kept (and written to `checkpointPath` when
// one is given).
class OptimizationJob final : public resonant::Job {
public:
    OptimizationJob(OptimizationProblem problem, std::shared_ptr<const DesignEvaluator> evaluator, RunOptions options = {},
                    std::string checkpointPath = {});
    std::string name() const override;
    JobEnd run(JobContext& context) override;
    std::vector<Candidate> candidates() const;  // so far
    std::optional<RunRecord> result() const;
    nlohmann::json checkpoint() const;           // the latest one (null before the first)

private:
    OptimizationProblem problem_;
    std::shared_ptr<const DesignEvaluator> evaluator_;
    RunOptions options_;
    std::string checkpointPath_;
    mutable std::mutex mutex_;
    std::vector<Candidate> live_;
    std::optional<RunRecord> result_;
    nlohmann::json checkpoint_;
};

// Current UTC time in ISO 8601 (for records).
std::string utcNow();
// A new run id ("OPT-<yyyymmddThhmmss>-<4 hex>").
std::string newRunId();

}  // namespace resonant::optimization
