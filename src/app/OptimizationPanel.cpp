#include "OptimizationPanel.h"

#include "OptimizationController.h"
#include "PlotWidget.h"
#include "ui_OptimizationPanel.h"

#include "optimization/Algorithms.h"
#include "optimization/MultiFidelity.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QImage>
#include <QMessageBox>
#include <QPixmap>

#include <cmath>
#include <set>
#include <sstream>

using namespace resonant;
using namespace resonant::optimization;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }

QString number(double v)
{
    if (!std::isfinite(v))
        return QStringLiteral("-");
    return QString::number(v, 'g', 6);
}

QTableWidgetItem* item(const QString& text, bool editable = false)
{
    auto* i = new QTableWidgetItem(text);
    if (!editable)
        i->setFlags(i->flags() & ~Qt::ItemIsEditable);
    return i;
}

QTableWidgetItem* check(bool on)
{
    auto* i = new QTableWidgetItem;
    i->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    i->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    return i;
}

double cellValue(QTableWidget* t, int row, int column, double fallback)
{
    bool ok = false;
    const QTableWidgetItem* i = t->item(row, column);
    const double v = i ? i->text().toDouble(&ok) : 0.0;
    return ok ? v : fallback;
}

QComboBox* comboIn(QTableWidget* t, int row, int column) { return qobject_cast<QComboBox*>(t->cellWidget(row, column)); }

}  // namespace

OptimizationPanel::OptimizationPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::OptimizationPanel>())
{
    ui_->setupUi(this);
    for (const Algorithm* a : algorithms())
        ui_->algorithmCombo->addItem(qs(a->info().name), qs(a->info().id));
    for (const auto& [label, value] : {std::pair{"Mean", "mean"}, {"Mean + k sigma", "mean-plus-k-sigma"}, {"Worst case", "worst-case"}})
        ui_->robustMeasureCombo->addItem(QString::fromUtf8(label), QString::fromUtf8(value));
    ui_->inverseMethodCombo->addItem(QStringLiteral("Regularized (Tikhonov)"), 0);
    ui_->inverseMethodCombo->addItem(QStringLiteral("Minimum norm (pseudo-inverse)"), 1);
    for (const std::string& k : surrogateKinds())
        ui_->surrogateKindCombo->addItem(qs(k), qs(k));
    ui_->benchmarkCombo->addItem(QStringLiteral("Cantilever (load at mid-height)"), 0);
    ui_->benchmarkCombo->addItem(QStringLiteral("Half MBB beam"), 1);
    ui_->stopButton->setEnabled(false);

    connect(ui_->problemCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_)
            refreshEditor();
    });
    connect(ui_->templateCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!controller_)
            return;
        const std::string id = ui_->templateCombo->currentData().toString().toStdString();
        for (const ProblemTemplate* t : controller_->templates())
            if (t->id == id)
                ui_->templateDescription->setText(qs(t->description));
    });
    connect(ui_->addTemplateButton, &QPushButton::clicked, this,
            [this] { addFromTemplate(ui_->templateCombo->currentData().toString().toStdString()); });
    connect(ui_->removeProblemButton, &QPushButton::clicked, this, [this] {
        if (controller_ && !selectedProblem().empty())
            if (Status s = controller_->removeProblem(selectedProblem()); !s)
                setStatus(qs(s.message()), true);
    });
    connect(ui_->applyProblemButton, &QPushButton::clicked, this, [this] { applyEdits(); });
    connect(ui_->algorithmCombo, &QComboBox::currentIndexChanged, this, [this] { refreshMethodNote(); });
    connect(ui_->modelCombo, &QComboBox::currentIndexChanged, this, [this] { refreshMethodNote(); });
    connect(ui_->addVariableButton, &QPushButton::clicked, this, [this] {
        const Capabilities* caps = capabilities();
        if (!caps || !editing_)
            return;
        collectEdits();
        const std::string binding = ui_->availableVariableCombo->currentData().toString().toStdString();
        if (const DesignVariable* v = caps->variable(binding)) {
            for (const DesignVariable& existing : editing_->space.variables)
                if (existing.key() == binding)
                    return;
            editing_->space.variables.push_back(*v);
            refreshVariables();
        }
    });
    connect(ui_->removeVariableButton, &QPushButton::clicked, this, [this] {
        if (!editing_)
            return;
        collectEdits();
        const int row = ui_->variablesTable->currentRow();
        if (row >= 0 && row < static_cast<int>(editing_->space.variables.size())) {
            editing_->space.variables.erase(editing_->space.variables.begin() + row);
            refreshVariables();
        }
    });
    const auto addQuantity = [this](bool constraint) {
        const Capabilities* caps = capabilities();
        if (!caps || !editing_)
            return;
        collectEdits();
        const std::string key = ui_->quantityCombo->currentData().toString().toStdString();
        const QuantityInfo* q = caps->quantity(key);
        if (!q)
            return;
        if (constraint) {
            DesignConstraint c;
            c.id = "c" + std::to_string(editing_->constraints.size() + 1);
            c.label = q->label;
            c.quantity = key;
            c.unit = q->unit;
            editing_->constraints.push_back(c);
        } else {
            Objective o;
            o.id = "o" + std::to_string(editing_->objectives.size() + 1);
            o.label = q->label;
            o.quantity = key;
            o.unit = q->unit;
            editing_->objectives.push_back(o);
        }
        refreshObjectives();
    };
    connect(ui_->addObjectiveButton, &QPushButton::clicked, this, [addQuantity] { addQuantity(false); });
    connect(ui_->addConstraintButton, &QPushButton::clicked, this, [addQuantity] { addQuantity(true); });
    connect(ui_->removeObjectiveButton, &QPushButton::clicked, this, [this] {
        if (!editing_)
            return;
        collectEdits();
        const int row = ui_->objectivesTable->currentRow();
        if (row >= 0 && row < static_cast<int>(editing_->objectives.size())) {
            editing_->objectives.erase(editing_->objectives.begin() + row);
            refreshObjectives();
        }
    });
    connect(ui_->removeConstraintButton, &QPushButton::clicked, this, [this] {
        if (!editing_)
            return;
        collectEdits();
        const int row = ui_->constraintsTable->currentRow();
        if (row >= 0 && row < static_cast<int>(editing_->constraints.size())) {
            editing_->constraints.erase(editing_->constraints.begin() + row);
            refreshObjectives();
        }
    });
    connect(ui_->quantityCombo, &QComboBox::currentIndexChanged, this, [this] {
        const Capabilities* caps = capabilities();
        if (!caps)
            return;
        if (const QuantityInfo* q = caps->quantity(ui_->quantityCombo->currentData().toString().toStdString()))
            ui_->definitionLabel->setText(qs(q->label + " [" + (q->unit.empty() ? std::string("1") : q->unit) + "]: " + q->definition));
    });
    connect(ui_->addUncertainButton, &QPushButton::clicked, this, [this] {
        if (!editing_)
            return;
        collectEdits();
        UncertainParameter u;
        u.binding = ui_->uncertainCombo->currentData().toString().toStdString();
        u.label = ui_->uncertainCombo->currentText().toStdString();
        u.width = 0.01;
        if (!u.binding.empty())
            editing_->robust.parameters.push_back(u);
        refreshEditor();
    });
    connect(ui_->removeUncertainButton, &QPushButton::clicked, this, [this] {
        if (!editing_)
            return;
        collectEdits();
        const int row = ui_->uncertainTable->currentRow();
        if (row >= 0 && row < static_cast<int>(editing_->robust.parameters.size())) {
            editing_->robust.parameters.erase(editing_->robust.parameters.begin() + row);
            refreshEditor();
        }
    });

    connect(ui_->startButton, &QPushButton::clicked, this, [this] { start(); });
    connect(ui_->stopButton, &QPushButton::clicked, this, [this] { stop(); });
    connect(ui_->resumeButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        if (Status s = controller_->resume(selectedRun()); !s)
            setStatus(qs(s.message()), true);
        refreshRun();
    });
    connect(ui_->runCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_) {
            refreshCandidates();
            refreshPareto();
        }
    });
    connect(ui_->candidatesTable, &QTableWidget::itemSelectionChanged, this, [this] { showCandidateDetails(); });
    connect(ui_->compareButton, &QPushButton::clicked, this, [this] {
        const auto run = currentRun();
        const auto ids = selectedCandidates();
        if (!run || ids.size() != 2) {
            setStatus(QStringLiteral("Select two candidates to compare."), true);
            return;
        }
        auto problem = problemFromJson(run->problem);
        const Candidate *a = nullptr, *b = nullptr;
        for (const Candidate& c : run->candidates) {
            if (c.id == ids[0])
                a = &c;
            if (c.id == ids[1])
                b = &c;
        }
        if (!problem || !a || !b)
            return;
        const DesignComparison cmp = compare(problem.value(), *a, *b);
        QString text = qs(a->id) + QStringLiteral("  vs  ") + qs(b->id) + QStringLiteral("\n");
        for (const auto* section : {&cmp.variables, &cmp.objectives, &cmp.constraints})
            for (const DesignComparison::Row& r : *section)
                text += qs(r.what) + QStringLiteral(": ") + number(r.a) + QStringLiteral(" -> ") + number(r.b) + QStringLiteral(" ") + qs(r.unit) +
                        (r.differs ? QStringLiteral("  (") + number(r.difference) + QStringLiteral(")") : QString()) + QStringLiteral("\n");
        for (const std::string& n : cmp.notes)
            text += QStringLiteral("Note: ") + qs(n) + QStringLiteral("\n");
        ui_->detailsText->setPlainText(text);
    });
    connect(ui_->promoteButton, &QPushButton::clicked, this, [this] {
        if (!promoteSelected(false) && controller_ && statusText().contains(QStringLiteral("changed since"))) {
            if (QMessageBox::question(this, QStringLiteral("Promote candidate"),
                                      statusText() + QStringLiteral("\n\nApply its values to the present design anyway?")) == QMessageBox::Yes)
                promoteSelected(true);
        }
    });
    connect(ui_->reevaluateButton, &QPushButton::clicked, this,
            [this] { reevaluateSelected(ui_->reevaluateModelCombo->currentData().toString().toStdString()); });
    connect(ui_->exportCandidateButton, &QPushButton::clicked, this, [this] {
        const auto ids = selectedCandidates();
        if (!controller_ || ids.empty())
            return;
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Export candidate to"));
        if (dir.isEmpty())
            return;
        const Status s = controller_->exportCandidate(selectedRun(), ids.front(), dir);
        setStatus(s ? QStringLiteral("Exported ") + qs(ids.front()) + QStringLiteral(" to ") + dir : qs(s.message()), !s);
    });
    connect(ui_->exportRunButton, &QPushButton::clicked, this, [this] {
        if (!controller_ || selectedRun().empty())
            return;
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export run"), qs(selectedRun()) + QStringLiteral(".json"),
                                                          QStringLiteral("Run record (*.json);;Candidates (*.csv)"));
        if (path.isEmpty())
            return;
        const Status s = controller_->exportRun(selectedRun(), path);
        setStatus(s ? QStringLiteral("Exported to ") + path : qs(s.message()), !s);
    });
    connect(ui_->xObjectiveCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPareto(); });
    connect(ui_->yObjectiveCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPareto(); });
    connect(ui_->rankButton, &QPushButton::clicked, this, [this] {
        const auto run = currentRun();
        if (!run)
            return;
        std::vector<double> weights;
        for (const QString& part : ui_->prioritiesEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
            weights.push_back(part.trimmed().toDouble());
        const auto order = rankByPriorities(run->candidates, weights);
        QString text = QStringLiteral("Ranked by your priorities (a choice made after the run; feasible first):\n");
        for (size_t i = 0; i < order.size() && i < 10; ++i) {
            const Candidate& c = run->candidates[order[i]];
            text += QString::number(i + 1) + QStringLiteral(". ") + qs(c.id) + (c.feasible ? QString() : QStringLiteral(" (infeasible)"));
            for (double v : c.objectiveValues)
                text += QStringLiteral("  ") + number(v);
            text += QStringLiteral("\n");
        }
        ui_->rankingText->setPlainText(text);
    });
    connect(ui_->sensitivityButton, &QPushButton::clicked, this, [this] { analyzeSensitivity(); });
    connect(ui_->solveInverseButton, &QPushButton::clicked, this, [this] { solveInverse(); });
    connect(ui_->fitSurrogateButton, &QPushButton::clicked, this, [this] {
        if (!controller_ || selectedRun().empty())
            return;
        auto info = controller_->fitSurrogate(selectedRun(), static_cast<size_t>(std::max(0, ui_->surrogateObjectiveCombo->currentIndex())),
                                              ui_->surrogateKindCombo->currentData().toString().toStdString());
        if (!info) {
            ui_->surrogateText->setPlainText(qs(info.message()));
            return;
        }
        QString text = qs(info.value().kind) + QStringLiteral(" (version ") + qs(info.value().version) + QStringLiteral(") of ") +
                       qs(info.value().output) + QStringLiteral("\ntraining points ") + QString::number(info.value().trainingPoints) +
                       QStringLiteral(", training RMSE ") + number(info.value().trainingRmse) + QStringLiteral(", leave-one-out RMSE ") +
                       number(info.value().validationRmse) + QStringLiteral("\n");
        for (const std::string& n : info.value().notes)
            text += qs(n) + QStringLiteral("\n");
        ui_->surrogateText->setPlainText(text);
    });
    connect(ui_->topologyButton, &QPushButton::clicked, this, [this] { runTopology(); });
    connect(ui_->exportTopologyButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export topology"), QStringLiteral("topology.stl"),
                                                          QStringLiteral("STL (*.stl)"));
        if (path.isEmpty())
            return;
        const Status s = controller_->exportTopology(path);
        setStatus(s ? QStringLiteral("Exported the cleaned, extruded design to ") + path : qs(s.message()), !s);
    });
}

OptimizationPanel::~OptimizationPanel() = default;

void OptimizationPanel::setController(OptimizationController* controller)
{
    controller_ = controller;
    updating_ = true;
    ui_->templateCombo->clear();
    for (const ProblemTemplate* t : controller_->templates())
        ui_->templateCombo->addItem(qs(t->title), qs(t->id));
    updating_ = false;
    connect(controller_, &OptimizationController::problemsChanged, this, [this] { refreshProblems(); });
    connect(controller_, &OptimizationController::runsChanged, this, [this] { refreshRuns(); });
    connect(controller_, &OptimizationController::candidatesChanged, this, [this] {
        refreshRun();
        if (controller_->running() && selectedRun() == controller_->liveRunId())
            refreshCandidates();
    });
    connect(controller_, &OptimizationController::runFinished, this, [this](const QString& message) {
        setStatus(message);
        refreshRun();
        refreshRuns();
        selectRun(controller_->liveRunId());
    });
    connect(controller_, &OptimizationController::topologyFinished, this, [this](const QString& message) {
        const auto& r = controller_->topologyResult();
        if (!r) {
            ui_->topologyText->setPlainText(QStringLiteral("The topology optimization did not finish: ") + message);
            return;
        }
        const auto& s = r->settings;
        QImage image(s.nelx, s.nely, QImage::Format_Grayscale8);
        for (int i = 0; i < s.nelx; ++i)
            for (int j = 0; j < s.nely; ++j)
                image.setPixel(i, j, qRgb(0, 0, 0) | static_cast<unsigned>(255 - std::lround(255 * r->density[static_cast<size_t>(i * s.nely + j)])) * 0x010101u);
        ui_->topologyImage->setPixmap(QPixmap::fromImage(image.scaled(s.nelx * 4, s.nely * 4)));
        QString text = QStringLiteral("Compliance ") + number(r->initialCompliance) + QStringLiteral(" J (uniform) -> ") +
                       number(r->compliance.empty() ? NAN : r->compliance.back()) + QStringLiteral(" J after ") + QString::number(r->iterations) +
                       QStringLiteral(" iterations") + (r->converged ? QString() : QStringLiteral(" (not converged)")) +
                       QStringLiteral("; cleaned design ") + number(r->cleanedCompliance) + QStringLiteral(" J at volume ") +
                       number(r->cleanedVolumeFraction) + QStringLiteral(", ") + QString::number(r->islandsRemoved) + QStringLiteral(" island(s) removed.\n");
        for (const std::string& n : r->notes)
            text += qs(n) + QStringLiteral("\n");
        ui_->topologyText->setPlainText(text);
    });
    refreshProblems();
    refreshRuns();
}

std::string OptimizationPanel::selectedProblem() const { return ui_->problemCombo->currentData().toString().toStdString(); }

void OptimizationPanel::selectProblem(const std::string& id)
{
    const int index = ui_->problemCombo->findData(qs(id));
    if (index >= 0)
        ui_->problemCombo->setCurrentIndex(index);
}

std::string OptimizationPanel::selectedRun() const { return ui_->runCombo->currentData().toString().toStdString(); }

void OptimizationPanel::selectRun(const std::string& id)
{
    const int index = ui_->runCombo->findData(qs(id));
    if (index >= 0)
        ui_->runCombo->setCurrentIndex(index);
    refreshCandidates();
    refreshPareto();
}

std::vector<std::string> OptimizationPanel::selectedCandidates() const
{
    std::vector<std::string> out;
    std::set<int> rows;
    for (const QTableWidgetItem* i : ui_->candidatesTable->selectedItems())
        rows.insert(i->row());
    for (int r : rows)
        if (const QTableWidgetItem* i = ui_->candidatesTable->item(r, 0))
            out.push_back(i->text().toStdString());
    return out;
}

void OptimizationPanel::selectCandidateRow(int row, bool add)
{
    auto* model = ui_->candidatesTable->selectionModel();
    const QModelIndex index = ui_->candidatesTable->model()->index(row, 0);
    model->select(index, (add ? QItemSelectionModel::Select : QItemSelectionModel::ClearAndSelect) | QItemSelectionModel::Rows);
}

int OptimizationPanel::candidateRows() const { return ui_->candidatesTable->rowCount(); }
QString OptimizationPanel::statusText() const { return ui_->summaryLabel->text(); }
QString OptimizationPanel::detailsText() const { return ui_->detailsText->toPlainText(); }

void OptimizationPanel::showTab(const QString& objectName)
{
    if (QWidget* tab = findChild<QWidget*>(objectName))
        ui_->tabs->setCurrentWidget(tab);
}

void OptimizationPanel::setStatus(const QString& text, bool error)
{
    ui_->summaryLabel->setText(text);
    ui_->summaryLabel->setStyleSheet(error ? QStringLiteral("color: #c0392b;") : QString());
}

const Capabilities* OptimizationPanel::capabilities()
{
    if (!controller_ || !editing_)
        return nullptr;
    const std::string key = editing_->adapterId + "|" + editing_->adapterConfig.dump();
    if (!evaluator_ || evaluatorKey_ != key) {
        auto ev = controller_->evaluator(*editing_);
        evaluator_ = ev ? ev.take() : nullptr;
        evaluatorKey_ = key;
        if (!evaluator_)
            setStatus(qs(ev.message()), true);
    }
    return evaluator_ ? &evaluator_->capabilities() : nullptr;
}

std::optional<RunRecord> OptimizationPanel::currentRun() const
{
    if (!controller_)
        return std::nullopt;
    const std::string id = selectedRun();
    if (controller_->running() && id == controller_->liveRunId()) {
        RunRecord live;
        live.runId = id;
        const OptimizationWorkspace w = controller_->workspace();
        if (const OptimizationProblem* p = w.findProblem(controller_->liveProblemId()))
            live.problem = toJson(*p);
        live.candidates = controller_->liveCandidates();
        return live;
    }
    const OptimizationWorkspace w = controller_->workspace();
    if (const RunRecord* r = w.findRun(id))
        return *r;
    return std::nullopt;
}

// --- problem editor ----------------------------------------------------------------------------------------

void OptimizationPanel::refreshProblems()
{
    const std::string keep = selectedProblem();
    updating_ = true;
    ui_->problemCombo->clear();
    for (const OptimizationProblem& p : controller_->workspace().problems)
        ui_->problemCombo->addItem(qs(p.name.empty() ? p.id : p.name), qs(p.id));
    const int index = ui_->problemCombo->findData(qs(keep));
    ui_->problemCombo->setCurrentIndex(index >= 0 ? index : ui_->problemCombo->count() - 1);
    updating_ = false;
    refreshEditor();
}

void OptimizationPanel::refreshEditor()
{
    editing_.reset();
    evaluator_.reset();  // rebind: the design may have changed since the last refresh
    if (controller_) {
        const OptimizationWorkspace w = controller_->workspace();
        if (const OptimizationProblem* p = w.findProblem(selectedProblem()))
            editing_ = *p;
    }
    const bool has = editing_.has_value();
    ui_->tabs->setEnabled(true);
    ui_->startButton->setEnabled(has && !(controller_ && controller_->running()));
    if (!has) {
        setStatus(QStringLiteral("No optimization problem: create one from a template."));
        ui_->variablesTable->setRowCount(0);
        ui_->objectivesTable->setRowCount(0);
        ui_->constraintsTable->setRowCount(0);
        return;
    }
    updating_ = true;
    const OptimizationProblem& p = *editing_;
    ui_->nameEdit->setText(qs(p.name));
    const Capabilities* caps = capabilities();
    ui_->adapterLabel->setText(caps ? qs(caps->adapterId + " (" + caps->domain + "), adapter version " + caps->version) : qs(p.adapterId));
    ui_->modelCombo->clear();
    ui_->validationModelCombo->clear();
    ui_->reevaluateModelCombo->clear();
    ui_->validationModelCombo->addItem(QStringLiteral("(none)"), QString());
    if (caps)
        for (const ModelInfo& m : caps->models) {
            const QString text = qs(m.name) + QStringLiteral(" - ") + qs(std::string(toString(m.fidelity))) +
                                 (m.available ? QString() : QStringLiteral(" (unavailable)"));
            ui_->modelCombo->addItem(text, qs(m.id));
            ui_->validationModelCombo->addItem(text, qs(m.id));
            ui_->reevaluateModelCombo->addItem(text, qs(m.id));
        }
    ui_->modelCombo->setCurrentIndex(std::max(0, ui_->modelCombo->findData(qs(p.modelId))));
    ui_->validationModelCombo->setCurrentIndex(std::max(0, ui_->validationModelCombo->findData(qs(p.validationModelId))));
    ui_->reevaluateModelCombo->setCurrentIndex(std::max(0, ui_->reevaluateModelCombo->findData(qs(p.validationModelId))));
    ui_->algorithmCombo->setCurrentIndex(std::max(0, ui_->algorithmCombo->findData(qs(p.algorithm.algorithm))));
    ui_->maxEvaluationsSpin->setValue(p.algorithm.maxEvaluations);
    ui_->seedSpin->setValue(static_cast<int>(p.algorithm.seed));
    ui_->populationSpin->setValue(p.algorithm.populationSize);
    ui_->pointsSpin->setValue(p.algorithm.pointsPerVariable);
    ui_->samplesSpin->setValue(p.algorithm.samples);
    ui_->parallelSpin->setValue(p.algorithm.parallel);
    ui_->timeLimitSpin->setValue(p.algorithm.maxSeconds);
    ui_->memorySpin->setValue(p.algorithm.memoryBudgetBytes / (1024.0 * 1024.0));
    ui_->robustCheck->setChecked(p.robust.enabled);
    ui_->robustSamplesSpin->setValue(p.robust.samples);
    ui_->robustMeasureCombo->setCurrentIndex(std::max(0, ui_->robustMeasureCombo->findData(qs(std::string(toString(p.robust.measure))))));
    ui_->uncertainTable->setRowCount(static_cast<int>(p.robust.parameters.size()));
    for (int r = 0; r < static_cast<int>(p.robust.parameters.size()); ++r) {
        const UncertainParameter& u = p.robust.parameters[static_cast<size_t>(r)];
        ui_->uncertainTable->setItem(r, 0, item(qs(u.label.empty() ? u.binding : u.label)));
        auto* d = new QComboBox;
        d->addItem(QStringLiteral("uniform (+/- width)"));
        d->addItem(QStringLiteral("normal (sigma = width)"));
        d->setCurrentIndex(u.distribution == Distribution::Normal ? 1 : 0);
        ui_->uncertainTable->setCellWidget(r, 1, d);
        ui_->uncertainTable->setItem(r, 2, item(number(u.width), true));
        ui_->uncertainTable->setItem(r, 3, check(u.relative));
        ui_->uncertainTable->setItem(r, 4, item(qs(u.source), true));
    }
    ui_->uncertainCombo->clear();
    ui_->availableVariableCombo->clear();
    ui_->quantityCombo->clear();
    if (caps) {
        for (const DesignVariable& v : caps->variables) {
            ui_->availableVariableCombo->addItem(qs(v.label) + (v.unit.empty() ? QString() : QStringLiteral(" [") + qs(v.unit) + QStringLiteral("]")),
                                                 qs(v.key()));
            if (v.kind != VariableKind::Categorical)
                ui_->uncertainCombo->addItem(qs(v.label), qs(v.key()));
        }
        for (const QuantityInfo& q : caps->quantities)
            ui_->quantityCombo->addItem(qs(q.label) + (q.unit.empty() ? QString() : QStringLiteral(" [") + qs(q.unit) + QStringLiteral("]")), qs(q.key));
    }
    ui_->surrogateObjectiveCombo->clear();
    ui_->xObjectiveCombo->clear();
    ui_->yObjectiveCombo->clear();
    for (const Objective& o : p.objectives) {
        ui_->surrogateObjectiveCombo->addItem(qs(o.label.empty() ? o.id : o.label));
        ui_->xObjectiveCombo->addItem(qs(o.label.empty() ? o.id : o.label));
        ui_->yObjectiveCombo->addItem(qs(o.label.empty() ? o.id : o.label));
    }
    if (p.objectives.size() > 1)
        ui_->yObjectiveCombo->setCurrentIndex(1);
    updating_ = false;
    refreshVariables();
    refreshObjectives();
    refreshMethodNote();
    setStatus(qs(p.name) + QStringLiteral(": ") + QString::number(p.space.dimension()) + QStringLiteral(" free variable(s), ") +
              QString::number(p.objectives.size()) + QStringLiteral(" objective(s), ") + QString::number(p.constraints.size()) +
              QStringLiteral(" constraint(s)."));
}

void OptimizationPanel::refreshVariables()
{
    if (!editing_)
        return;
    updating_ = true;
    QTableWidget* t = ui_->variablesTable;
    t->setRowCount(static_cast<int>(editing_->space.variables.size()));
    for (int r = 0; r < t->rowCount(); ++r) {
        const DesignVariable& v = editing_->space.variables[static_cast<size_t>(r)];
        t->setItem(r, 0, check(v.active));
        t->setItem(r, 1, check(v.locked));
        auto* name = item(qs(v.label.empty() ? v.id : v.label));
        name->setToolTip(qs(v.key() + " (" + std::string(toString(v.kind)) + ", " + std::string(toString(v.role)) + ")"));
        t->setItem(r, 2, name);
        t->setItem(r, 3, item(qs(v.unit)));
        const bool categorical = v.kind == VariableKind::Categorical;
        t->setItem(r, 4, item(categorical ? QStringLiteral("-") : number(v.lower), !categorical));
        t->setItem(r, 5, item(categorical ? QString::number(v.choices.size()) + QStringLiteral(" choices") : number(v.upper), !categorical));
        QString initial = number(v.initial);
        if (categorical && v.initial >= 0 && v.initial < v.choices.size())
            initial = qs(v.choiceLabels.empty() ? v.choices[static_cast<size_t>(v.initial)] : v.choiceLabels[static_cast<size_t>(v.initial)]);
        t->setItem(r, 6, item(initial, !categorical));
        t->setItem(r, 7, item(number(v.step), v.kind == VariableKind::Continuous));
    }
    std::string derived;
    for (const DerivedVariable& d : editing_->space.derived)
        derived += (derived.empty() ? "" : ", ") + d.id + " = " + d.expression;
    ui_->variablesLabel->setText(derived.empty() ? QString() : QStringLiteral("Derived: ") + qs(derived));
    updating_ = false;
}

void OptimizationPanel::refreshObjectives()
{
    if (!editing_)
        return;
    updating_ = true;
    const Capabilities* caps = capabilities();
    QTableWidget* t = ui_->objectivesTable;
    t->setRowCount(static_cast<int>(editing_->objectives.size()));
    for (int r = 0; r < t->rowCount(); ++r) {
        const Objective& o = editing_->objectives[static_cast<size_t>(r)];
        const QuantityInfo* q = caps ? caps->quantity(o.quantity) : nullptr;
        auto* name = item(qs(o.label.empty() ? o.quantity : o.label));
        name->setToolTip(q ? qs(q->definition) : qs(o.quantity));
        t->setItem(r, 0, name);
        auto* sense = new QComboBox;
        for (const char* s : {"minimize", "maximize", "target", "at-most", "at-least"})
            sense->addItem(QString::fromUtf8(s), QString::fromUtf8(s));
        sense->setCurrentIndex(sense->findData(qs(std::string(toString(o.sense)))));
        t->setCellWidget(r, 1, sense);
        t->setItem(r, 2, item(number(o.target), true));
        t->setItem(r, 3, item(number(o.weight), true));
        t->setItem(r, 4, item(number(o.scale), true));
        t->setItem(r, 5, item(qs(q ? q->unit : o.unit)));
    }
    QTableWidget* c = ui_->constraintsTable;
    c->setRowCount(static_cast<int>(editing_->constraints.size()));
    for (int r = 0; r < c->rowCount(); ++r) {
        const DesignConstraint& k = editing_->constraints[static_cast<size_t>(r)];
        c->setItem(r, 0, item(qs(k.label.empty() ? k.quantity : k.label)));
        c->item(r, 0)->setToolTip(qs(k.quantity));
        auto* relation = new QComboBox;
        for (const char* s : {"<=", ">=", "="})
            relation->addItem(QString::fromUtf8(s), QString::fromUtf8(s));
        relation->setCurrentIndex(relation->findData(qs(std::string(toString(k.relation)))));
        c->setCellWidget(r, 1, relation);
        c->setItem(r, 2, item(number(k.limit), true));
        c->setItem(r, 3, check(k.hard));
        auto* category = new QComboBox;
        for (const char* s : {"geometric", "physical", "manufacturing", "computational"})
            category->addItem(QString::fromUtf8(s), QString::fromUtf8(s));
        category->setCurrentIndex(category->findData(qs(std::string(toString(k.category)))));
        c->setCellWidget(r, 4, category);
        c->setItem(r, 5, item(number(k.scale), true));
    }
    updating_ = false;
}

void OptimizationPanel::refreshMethodNote()
{
    if (updating_ || !editing_)
        return;
    const Algorithm* a = findAlgorithm(ui_->algorithmCombo->currentData().toString().toStdString());
    QString text;
    if (a)
        text = qs(a->info().description) + QStringLiteral("\nStops: ") + qs(a->info().convergence);
    if (const Capabilities* caps = capabilities()) {
        OptimizationProblem probe = *editing_;
        probe.modelId = ui_->modelCombo->currentData().toString().toStdString();
        const auto recommended = recommendAlgorithms(traitsOf(probe, *caps));
        if (!recommended.empty())
            text += QStringLiteral("\nSuggested: ") + qs(recommended.front().algorithm) + QStringLiteral(" - ") + qs(recommended.front().reason);
        if (const ModelInfo* m = caps->model(probe.modelId))
            ui_->modelBanner->setText(QStringLiteral("Physical model: ") + qs(m->name) + QStringLiteral(" (") + qs(std::string(toString(m->fidelity))) +
                                      QStringLiteral(" fidelity). Assumes: ") + qs(m->assumptions.empty() ? std::string("-") : m->assumptions.front()));
    }
    ui_->methodLabel->setText(text);
}

void OptimizationPanel::collectEdits()
{
    if (!editing_ || updating_)
        return;
    OptimizationProblem& p = *editing_;
    p.name = ui_->nameEdit->text().toStdString();
    p.modelId = ui_->modelCombo->currentData().toString().toStdString();
    p.validationModelId = ui_->validationModelCombo->currentData().toString().toStdString();
    p.algorithm.algorithm = ui_->algorithmCombo->currentData().toString().toStdString();
    p.algorithm.maxEvaluations = ui_->maxEvaluationsSpin->value();
    p.algorithm.seed = static_cast<std::uint64_t>(ui_->seedSpin->value());
    p.algorithm.populationSize = ui_->populationSpin->value();
    p.algorithm.pointsPerVariable = ui_->pointsSpin->value();
    p.algorithm.samples = ui_->samplesSpin->value();
    p.algorithm.parallel = ui_->parallelSpin->value();
    p.algorithm.maxSeconds = ui_->timeLimitSpin->value();
    p.algorithm.memoryBudgetBytes = ui_->memorySpin->value() * 1024.0 * 1024.0;
    p.robust.enabled = ui_->robustCheck->isChecked();
    p.robust.samples = ui_->robustSamplesSpin->value();
    const std::string measure = ui_->robustMeasureCombo->currentData().toString().toStdString();
    p.robust.measure = measure == "worst-case" ? RobustMeasure::WorstCase : measure == "mean-plus-k-sigma" ? RobustMeasure::MeanPlusSigma : RobustMeasure::Mean;
    for (int r = 0; r < ui_->uncertainTable->rowCount() && r < static_cast<int>(p.robust.parameters.size()); ++r) {
        UncertainParameter& u = p.robust.parameters[static_cast<size_t>(r)];
        if (QComboBox* d = comboIn(ui_->uncertainTable, r, 1))
            u.distribution = d->currentIndex() == 1 ? Distribution::Normal : Distribution::Uniform;
        u.width = cellValue(ui_->uncertainTable, r, 2, u.width);
        u.relative = ui_->uncertainTable->item(r, 3)->checkState() == Qt::Checked;
        u.source = ui_->uncertainTable->item(r, 4)->text().toStdString();
    }
    QTableWidget* t = ui_->variablesTable;
    for (int r = 0; r < t->rowCount() && r < static_cast<int>(p.space.variables.size()); ++r) {
        DesignVariable& v = p.space.variables[static_cast<size_t>(r)];
        v.active = t->item(r, 0)->checkState() == Qt::Checked;
        v.locked = t->item(r, 1)->checkState() == Qt::Checked;
        if (v.kind != VariableKind::Categorical) {
            v.lower = cellValue(t, r, 4, v.lower);
            v.upper = cellValue(t, r, 5, v.upper);
            v.initial = cellValue(t, r, 6, v.initial);
        }
        if (v.kind == VariableKind::Continuous)
            v.step = cellValue(t, r, 7, v.step);
    }
    QTableWidget* o = ui_->objectivesTable;
    for (int r = 0; r < o->rowCount() && r < static_cast<int>(p.objectives.size()); ++r) {
        Objective& x = p.objectives[static_cast<size_t>(r)];
        if (QComboBox* s = comboIn(o, r, 1))
            x.sense = senseFromString(s->currentData().toString().toStdString()).value_or(x.sense);
        x.target = cellValue(o, r, 2, x.target);
        x.weight = cellValue(o, r, 3, x.weight);
        x.scale = cellValue(o, r, 4, x.scale);
    }
    QTableWidget* c = ui_->constraintsTable;
    for (int r = 0; r < c->rowCount() && r < static_cast<int>(p.constraints.size()); ++r) {
        DesignConstraint& k = p.constraints[static_cast<size_t>(r)];
        if (QComboBox* rel = comboIn(c, r, 1))
            k.relation = relationFromString(rel->currentData().toString().toStdString()).value_or(k.relation);
        k.limit = cellValue(c, r, 2, k.limit);
        k.hard = c->item(r, 3)->checkState() == Qt::Checked;
        if (QComboBox* cat = comboIn(c, r, 4))
            k.category = constraintCategoryFromString(cat->currentData().toString().toStdString()).value_or(k.category);
        k.scale = cellValue(c, r, 5, k.scale);
    }
}

bool OptimizationPanel::addFromTemplate(const std::string& templateId)
{
    if (!controller_)
        return false;
    auto id = controller_->addFromTemplate(templateId);
    if (!id) {
        setStatus(qs(id.message()), true);
        return false;
    }
    selectProblem(id.value());
    return true;
}

bool OptimizationPanel::applyEdits()
{
    if (!controller_ || !editing_)
        return false;
    collectEdits();
    const Capabilities* caps = capabilities();
    if (caps)
        if (Status s = editing_->validateAgainst(*caps); !s) {
            setStatus(qs(s.message()), true);
            return false;
        }
    const OptimizationWorkspace w = controller_->workspace();
    const OptimizationProblem* stored = w.findProblem(editing_->id);
    if (stored && toJson(*stored) == toJson(*editing_))
        return true;  // nothing changed
    if (Status s = controller_->setProblem(*editing_, QStringLiteral("Edit optimization problem")); !s) {
        setStatus(qs(s.message()), true);
        return false;
    }
    return true;
}

// --- run ----------------------------------------------------------------------------------------------------

bool OptimizationPanel::start()
{
    if (!controller_ || !applyEdits())
        return false;
    if (Status s = controller_->start(selectedProblem()); !s) {
        setStatus(qs(s.message()), true);
        return false;
    }
    ui_->runLog->appendPlainText(QStringLiteral("Started ") + qs(controller_->liveRunId()));
    refreshRuns();
    selectRun(controller_->liveRunId());
    refreshRun();
    return true;
}

void OptimizationPanel::stop()
{
    if (controller_)
        controller_->stop();
}

void OptimizationPanel::refreshRun()
{
    if (!controller_)
        return;
    const bool running = controller_->running();
    ui_->startButton->setEnabled(!running && editing_.has_value());
    ui_->stopButton->setEnabled(running);
    ui_->resumeButton->setEnabled(!running && !selectedRun().empty() && controller_->canResume(selectedRun()));
    ui_->progressBar->setValue(static_cast<int>(std::lround(100.0 * controller_->progress())));
    ui_->runStatusLabel->setText(controller_->runStatus());
    // Convergence: the best weighted score of feasible candidates so far.
    std::vector<Candidate> candidates = running ? controller_->liveCandidates() : std::vector<Candidate>{};
    if (!running)
        if (const auto run = currentRun())
            candidates = run->candidates;
    QVector<double> x, y;
    double best = INFINITY;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].feasible && std::isfinite(candidates[i].weightedScore))
            best = std::min(best, candidates[i].weightedScore);
        if (std::isfinite(best)) {
            x.push_back(static_cast<double>(i + 1));
            y.push_back(best);
        }
    }
    ui_->convergencePlot->setCurve(x, y, QStringLiteral("Best weighted score of the feasible candidates"), QStringLiteral("candidate"),
                                   QStringLiteral("score (lower is better)"));
}

void OptimizationPanel::refreshRuns()
{
    if (!controller_)
        return;
    const std::string keep = selectedRun();
    updating_ = true;
    ui_->runCombo->clear();
    const OptimizationWorkspace w = controller_->workspace();
    for (const RunRecord& r : w.runs)
        ui_->runCombo->addItem(qs(r.runId + " - " + r.algorithm + (r.complete ? "" : " (interrupted)")), qs(r.runId));
    if (controller_->running() && ui_->runCombo->findData(qs(controller_->liveRunId())) < 0)
        ui_->runCombo->addItem(qs(controller_->liveRunId() + " (running)"), qs(controller_->liveRunId()));
    const int index = ui_->runCombo->findData(qs(keep));
    ui_->runCombo->setCurrentIndex(index >= 0 ? index : ui_->runCombo->count() - 1);
    updating_ = false;
    refreshCandidates();
    refreshPareto();
    refreshRun();
}

void OptimizationPanel::refreshCandidates()
{
    const auto run = currentRun();
    QTableWidget* t = ui_->candidatesTable;
    if (!run) {
        t->setRowCount(0);
        return;
    }
    auto problem = problemFromJson(run->problem);
    const size_t objectives = problem ? problem.value().objectives.size() : 0;
    QStringList headers{QStringLiteral("Candidate"), QStringLiteral("Origin"), QStringLiteral("Model"), QStringLiteral("Feasible")};
    if (problem)
        for (const Objective& o : problem.value().objectives)
            headers << qs(o.label.empty() ? o.id : o.label) + (o.unit.empty() ? QString() : QStringLiteral(" [") + qs(o.unit) + QStringLiteral("]"));
    headers << QStringLiteral("Failed constraints");
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setRowCount(static_cast<int>(run->candidates.size()));
    for (int r = 0; r < t->rowCount(); ++r) {
        const Candidate& c = run->candidates[static_cast<size_t>(r)];
        t->setItem(r, 0, item(qs(c.id)));
        t->setItem(r, 1, item(qs(c.origin)));
        t->setItem(r, 2, item(qs(c.evaluation.model.modelId + " (" + std::string(toString(c.evaluation.model.fidelity)) + ")")));
        QString feasible = c.evaluated() ? (c.feasible ? QStringLiteral("yes") : QStringLiteral("no")) : qs(std::string(toString(c.evaluation.status)));
        if (c.paretoRank == 0 && objectives > 1)
            feasible += QStringLiteral(" (Pareto)");
        if (c.robust)
            feasible += QStringLiteral(" (robust)");
        t->setItem(r, 3, item(feasible));
        for (size_t k = 0; k < objectives; ++k)
            t->setItem(r, static_cast<int>(4 + k), item(number(k < c.objectiveValues.size() ? c.objectiveValues[k] : NAN)));
        QStringList failed;
        if (problem)
            for (const std::string& f : c.failedConstraints(problem.value()))
                failed << qs(f);
        t->setItem(r, static_cast<int>(4 + objectives), item(failed.join(QStringLiteral("; "))));
    }
}

void OptimizationPanel::showCandidateDetails()
{
    const auto run = currentRun();
    const auto ids = selectedCandidates();
    if (!run || ids.size() != 1)
        return;
    for (const Candidate& c : run->candidates) {
        if (c.id != ids.front())
            continue;
        std::ostringstream s;
        s.precision(6);
        s << c.id << " (" << c.origin << ")";
        if (!c.parents.empty()) {
            s << ", parents:";
            for (const std::string& p : c.parents)
                s << " " << p;
        }
        s << "\nModel: " << c.evaluation.model.modelName << " [" << c.evaluation.model.modelId << " v" << c.evaluation.model.version << ", "
          << toString(c.evaluation.model.fidelity) << "], snapshot " << c.evaluation.model.snapshotKey
          << (c.fromCache ? " (reused from the cache)" : "") << (c.replayed ? " (restored from a checkpoint)" : "") << "\n";
        if (!c.evaluation.ok())
            s << "Not evaluated: " << toString(c.evaluation.status) << " - " << c.evaluation.message << "\n";
        s << "Design:\n";
        for (const auto& [k, v] : c.point.values)
            s << "  " << k << " = " << v << "\n";
        s << "Quantities:\n";
        for (const auto& [k, v] : c.evaluation.quantities)
            s << "  " << k << " = " << v << "\n";
        if (c.robust) {
            s << "Robust (" << c.robust->samples << " samples, " << c.robust->measure << "):";
            for (size_t i = 0; i < c.robust->mean.size(); ++i)
                s << " mean " << c.robust->mean[i] << ", std " << c.robust->stddev[i] << ", min " << c.robust->minimum[i] << ", max " << c.robust->maximum[i];
            s << "\n";
        }
        for (const std::string& n : c.evaluation.notes)
            s << "Note: " << n << "\n";
        for (const std::string& w : c.evaluation.warnings)
            s << "Warning: " << w << "\n";
        for (const std::string& a : c.evaluation.model.assumptions)
            s << "Assumes: " << a << "\n";
        ui_->detailsText->setPlainText(qs(s.str()));
    }
}

void OptimizationPanel::refreshPareto()
{
    const auto run = currentRun();
    ui_->paretoPlot->clear();
    if (!run)
        return;
    const int xi = ui_->xObjectiveCombo->currentIndex(), yi = ui_->yObjectiveCombo->currentIndex();
    if (xi < 0 || yi < 0)
        return;
    QVector<double> x, y, fx, fy;
    std::vector<std::pair<double, double>> front;
    const auto ranks = paretoRanks(run->candidates, true);
    for (size_t i = 0; i < run->candidates.size(); ++i) {
        const Candidate& c = run->candidates[i];
        if (!c.feasible || static_cast<size_t>(std::max(xi, yi)) >= c.objectiveValues.size())
            continue;
        x.push_back(c.objectiveValues[static_cast<size_t>(xi)]);
        y.push_back(c.objectiveValues[static_cast<size_t>(yi)]);
        if (ranks[i] == 0)
            front.emplace_back(x.back(), y.back());
    }
    std::sort(front.begin(), front.end());
    for (const auto& [a, b] : front) {
        fx.push_back(a);
        fy.push_back(b);
    }
    ui_->paretoPlot->setCurve(x, y, QStringLiteral("Feasible candidates and their Pareto front"), ui_->xObjectiveCombo->currentText(),
                              ui_->yObjectiveCombo->currentText());
    ui_->paretoPlot->addCurve(fx, fy, QStringLiteral("non-dominated"), QColor(220, 90, 40), false);
}

// --- actions --------------------------------------------------------------------------------------------------

bool OptimizationPanel::promoteSelected(bool acceptChangedDesign)
{
    const auto ids = selectedCandidates();
    if (!controller_ || ids.size() != 1) {
        setStatus(QStringLiteral("Select one candidate to promote."), true);
        return false;
    }
    if (Status s = controller_->promote(selectedRun(), ids.front(), acceptChangedDesign); !s) {
        setStatus(qs(s.message()), true);
        return false;
    }
    setStatus(QStringLiteral("Promoted ") + qs(ids.front()) + QStringLiteral(" into the design as a new revision (Edit > Undo restores the previous design)."));
    return true;
}

bool OptimizationPanel::reevaluateSelected(const std::string& modelId)
{
    const auto ids = selectedCandidates();
    if (!controller_ || ids.empty()) {
        setStatus(QStringLiteral("Select the candidates to re-evaluate."), true);
        return false;
    }
    auto report = controller_->reevaluate(selectedRun(), ids, modelId);
    if (!report) {
        setStatus(qs(report.message()), true);
        return false;
    }
    QString text = QStringLiteral("Re-evaluated with ") + qs(report.value().toModel) + QStringLiteral(" (") +
                   qs(std::string(toString(report.value().toFidelity))) + QStringLiteral(") - values side by side, never combined:\n");
    for (const FidelityComparisonRow& r : report.value().rows) {
        text += qs(r.screenedId) + QStringLiteral(" -> ") + qs(r.validatedId) + QStringLiteral(":");
        for (size_t k = 0; k < r.screened.size() && k < r.validated.size(); ++k)
            text += QStringLiteral(" ") + number(r.screened[k]) + QStringLiteral(" -> ") + number(r.validated[k]);
        text += r.validatedFeasible ? QStringLiteral(" (feasible)\n") : QStringLiteral(" (NOT feasible)\n");
    }
    for (const std::string& n : report.value().notes)
        text += qs(n) + QStringLiteral("\n");
    ui_->detailsText->setPlainText(text);
    setStatus(QStringLiteral("Re-evaluated ") + QString::number(report.value().validated.size()) + QStringLiteral(" candidate(s): a new run record."));
    return true;
}

bool OptimizationPanel::analyzeSensitivity()
{
    const auto ids = selectedCandidates();
    if (!controller_ || ids.size() != 1) {
        setStatus(QStringLiteral("Select one evaluated candidate for the sensitivity analysis."), true);
        return false;
    }
    auto report = controller_->sensitivity(selectedRun(), ids.front(), ui_->sensitivityStepSpin->value());
    if (!report) {
        setStatus(qs(report.message()), true);
        return false;
    }
    const SensitivityReport& r = report.value();
    QTableWidget* t = ui_->sensitivityTable;
    QStringList headers{QStringLiteral("Variable")};
    for (const std::string& o : r.objectives)
        headers << qs(o);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setRowCount(static_cast<int>(r.variables.size()));
    double largest = 0.0;
    for (const auto& row : r.heatmap)
        for (double v : row)
            largest = std::max(largest, v);
    for (int i = 0; i < t->rowCount(); ++i) {
        t->setItem(i, 0, item(qs(r.variables[static_cast<size_t>(i)])));
        for (size_t k = 0; k < r.objectives.size() && static_cast<size_t>(i) < r.heatmap.size(); ++k) {
            const double v = r.heatmap[static_cast<size_t>(i)][k];
            auto* cell = item(number(v));
            const int shade = largest > 0.0 ? static_cast<int>(255 - 120 * v / largest) : 255;
            cell->setBackground(QColor(255, shade, shade));
            cell->setForeground(QColor(20, 20, 20));
            t->setItem(i, static_cast<int>(k + 1), cell);
        }
    }
    QString text = QStringLiteral("Normalized sensitivity: change of the objective's score across the variable's range (linear estimate). Ranking: ");
    for (const std::string& v : r.ranking)
        text += qs(v) + QStringLiteral(" ");
    if (!r.negligible.empty()) {
        text += QStringLiteral("\nNegligible effect: ");
        for (const std::string& v : r.negligible)
            text += qs(v) + QStringLiteral(" ");
    }
    for (const std::string& w : r.warnings)
        text += QStringLiteral("\n") + qs(w);
    ui_->sensitivityLabel->setText(text);
    return true;
}

bool OptimizationPanel::solveInverse()
{
    if (!controller_ || selectedProblem().empty())
        return false;
    LinearInverseOptions o;
    o.method = ui_->inverseMethodCombo->currentData().toInt() == 1 ? LinearInverseOptions::Method::MinimumNorm : LinearInverseOptions::Method::Regularized;
    o.regularization = ui_->regularizationSpin->value();
    o.powerPenalty = ui_->powerPenaltySpin->value();
    if (ui_->maxWeightSpin->value() > 0.0)
        o.maxMagnitude = ui_->maxWeightSpin->value();
    o.maxActive = ui_->activeSourcesSpin->value();
    auto r = controller_->solveInverse(selectedProblem(), o);
    if (!r) {
        ui_->inverseText->setPlainText(qs(r.message()));
        return false;
    }
    QString text = qs(r.value().classification) + QStringLiteral("\nrelative residual ") + number(r.value().relativeResidual) +
                   QStringLiteral(", condition number ") + number(r.value().conditionNumber) + QStringLiteral(", rank ") +
                   QString::number(r.value().rank) + QStringLiteral(", active sources ") + QString::number(r.value().activeSources.size()) +
                   (r.value().boundLimited ? QStringLiteral(", weights at the bound") : QString()) + QStringLiteral("\n");
    for (const std::string& n : r.value().notes)
        text += qs(n) + QStringLiteral("\n");
    text += QStringLiteral("The solution was evaluated with the physical model and stored as a candidate (Candidates tab).");
    ui_->inverseText->setPlainText(text);
    return true;
}

bool OptimizationPanel::runTopology()
{
    if (!controller_)
        return false;
    mechanics::TopologySettings s;
    s.benchmark = ui_->benchmarkCombo->currentData().toInt() == 1 ? mechanics::TopologyBenchmark::HalfMbbBeam : mechanics::TopologyBenchmark::Cantilever;
    s.nelx = ui_->nelxSpin->value();
    s.nely = ui_->nelySpin->value();
    s.width = s.nelx * 0.001;  // 1 mm elements
    s.volumeFraction = ui_->volumeSpin->value();
    s.penalty = ui_->penaltySpin->value();
    s.filterRadius = ui_->filterSpin->value() * 1e-3;
    s.maxIterations = ui_->iterationsSpin->value();
    if (Status st = controller_->startTopology(s); !st) {
        ui_->topologyText->setPlainText(qs(st.message()));
        return false;
    }
    ui_->topologyText->setPlainText(QStringLiteral("Running..."));
    return true;
}
