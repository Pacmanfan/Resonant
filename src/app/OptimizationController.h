#pragma once

// Optimization in the application (RES-OPT-018, RES-OPT-019, RES-OPT-022,
// RES-OPT-023): the project's problems, runs as background jobs with live
// candidates and checkpoints, re-evaluation with another model, sensitivity,
// linear inverse design, surrogates, promotion of a candidate into the
// design as an undoable revision, export, and the topology proof of concept.
//
// Nothing here changes the design except promote(), which applies one
// candidate as a single undoable step and records the revision with its
// lineage and the values it replaced. A candidate evaluated on a design that
// has changed since is not promoted unless the caller accepts that.
#include "mechanics/TopologyOptimization.h"
#include "optimization/Engine.h"
#include "optimization/LinearInverse.h"
#include "optimization/MultiFidelity.h"
#include "optimization/Persistence.h"
#include "optimization/Sensitivity.h"
#include "optimization/Surrogate.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <map>
#include <memory>
#include <optional>

class DocumentController;

class OptimizationController : public QObject {
    Q_OBJECT
public:
    explicit OptimizationController(DocumentController* document, QObject* parent = nullptr);
    ~OptimizationController() override;

    // --- problems (undoable project changes) ---
    resonant::optimization::OptimizationWorkspace workspace() const;
    std::vector<const resonant::optimization::ProblemTemplate*> templates() const;
    resonant::Result<std::string> addFromTemplate(const std::string& templateId);
    resonant::Status setProblem(const resonant::optimization::OptimizationProblem& problem, const QString& description);
    resonant::Status removeProblem(const std::string& id);
    // The problem's adapter bound to the present design.
    resonant::Result<std::shared_ptr<const resonant::optimization::DesignEvaluator>> evaluator(
        const resonant::optimization::OptimizationProblem& problem) const;

    // --- runs ---
    resonant::Status start(const std::string& problemId);
    resonant::Status resume(const std::string& runId);  // from the run's latest checkpoint
    bool canResume(const std::string& runId) const;
    void stop();
    bool running() const { return job_ != nullptr; }
    std::vector<resonant::optimization::Candidate> liveCandidates() const;
    const std::string& liveRunId() const { return liveRunId_; }
    const std::string& liveProblemId() const { return liveProblemId_; }
    QString runStatus() const { return status_; }
    double progress() const { return progress_; }
    // Waits (processing events) until no optimization job is running; for tests and the command line.
    bool waitUntilIdle(int timeoutMs);

    // --- results ---
    resonant::Result<resonant::optimization::MultiFidelityReport> reevaluate(const std::string& runId, const std::vector<std::string>& candidateIds,
                                                                            const std::string& modelId);
    resonant::Result<resonant::optimization::SensitivityReport> sensitivity(const std::string& runId, const std::string& candidateId, double step);
    resonant::Result<resonant::optimization::LinearInverseResult> solveInverse(const std::string& problemId,
                                                                              const resonant::optimization::LinearInverseOptions& options);
    resonant::Result<resonant::optimization::SurrogateInfo> fitSurrogate(const std::string& runId, size_t objectiveIndex, const std::string& kind);
    resonant::Status promote(const std::string& runId, const std::string& candidateId, bool acceptChangedDesign = false);
    resonant::Status exportCandidate(const std::string& runId, const std::string& candidateId, const QString& directory);
    resonant::Status exportRun(const std::string& runId, const QString& path);  // .json: the run record; .csv: the candidates

    // --- topology (proof of concept) ---
    resonant::Status startTopology(const resonant::mechanics::TopologySettings& settings);
    const std::optional<resonant::mechanics::TopologyResult>& topologyResult() const { return topology_; }
    resonant::Status exportTopology(const QString& path);

signals:
    void problemsChanged();
    void runsChanged();
    void candidatesChanged();
    void runFinished(const QString& message);
    void topologyFinished(const QString& message);

private:
    resonant::Status storeWorkspace(const resonant::optimization::OptimizationWorkspace& w, const QString& description);
    resonant::Status launch(resonant::optimization::OptimizationProblem problem, resonant::optimization::RunOptions options);
    void poll();
    void finish();
    std::string checkpointFile(const std::string& runId) const;
    const resonant::optimization::RunRecord* findRun(const resonant::optimization::OptimizationWorkspace& w, const std::string& runId) const;

    DocumentController* document_;
    QTimer timer_;
    std::shared_ptr<resonant::optimization::OptimizationJob> job_;
    resonant::Id jobId_;
    std::string liveRunId_, liveProblemId_;
    QString status_;
    double progress_ = 0.0;
    size_t shownCandidates_ = 0;
    std::map<std::string, nlohmann::json> checkpoints_;  // by run id, for unsaved projects and runs in progress
    std::shared_ptr<resonant::Job> topologyJob_;
    resonant::Id topologyJobId_;
    std::optional<resonant::mechanics::TopologyResult> topology_;
};
