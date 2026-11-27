#pragma once

// The Optimization Studio workspace (RES-OPT-023): problem definition from
// templates, the design-variable, objective and constraint editors, the
// method, model fidelity and resource limits, run controls with live
// progress and convergence, the candidate browser with comparison,
// re-evaluation, promotion and export, the Pareto view with priorities,
// sensitivity, inverse design and surrogates, and the topology proof of
// concept. Laid out in forms/OptimizationPanel.ui.
//
// The selected physics model and its fidelity are shown on every tab that
// runs something.
#include "optimization/Engine.h"
#include "optimization/Problem.h"

#include <QWidget>

#include <memory>
#include <optional>
#include <string>

class OptimizationController;

namespace Ui {
class OptimizationPanel;
}

class OptimizationPanel : public QWidget {
    Q_OBJECT
public:
    explicit OptimizationPanel(QWidget* parent = nullptr);
    ~OptimizationPanel() override;

    void setController(OptimizationController* controller);

    std::string selectedProblem() const;
    void selectProblem(const std::string& id);
    std::string selectedRun() const;
    void selectRun(const std::string& id);
    std::vector<std::string> selectedCandidates() const;
    void selectCandidateRow(int row, bool add = false);
    int candidateRows() const;
    QString statusText() const;
    QString detailsText() const;
    void showTab(const QString& objectName);

    // The actions behind the buttons (dialogs are skipped where a value is given).
    bool addFromTemplate(const std::string& templateId);
    bool applyEdits();
    bool start();
    void stop();
    bool promoteSelected(bool acceptChangedDesign = false);
    bool reevaluateSelected(const std::string& modelId);
    bool analyzeSensitivity();
    bool solveInverse();
    bool runTopology();

private:
    void refreshProblems();
    void refreshEditor();
    void refreshVariables();
    void refreshObjectives();
    void refreshRuns();
    void refreshCandidates();
    void refreshPareto();
    void refreshRun();
    void refreshMethodNote();
    void showCandidateDetails();
    void collectEdits();
    void setStatus(const QString& text, bool error = false);
    const resonant::optimization::Capabilities* capabilities();
    std::optional<resonant::optimization::RunRecord> currentRun() const;

    std::unique_ptr<Ui::OptimizationPanel> ui_;
    OptimizationController* controller_ = nullptr;
    std::optional<resonant::optimization::OptimizationProblem> editing_;
    std::shared_ptr<const resonant::optimization::DesignEvaluator> evaluator_;
    std::string evaluatorKey_;
    bool updating_ = false;
};
