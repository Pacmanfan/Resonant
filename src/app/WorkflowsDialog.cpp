#include "WorkflowsDialog.h"

#include "DocumentController.h"
#include "automation/WorkflowEngine.h"
#include "project/ProjectSerializer.h"
#include "ui_WorkflowsDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QPushButton>
#include <QStandardPaths>

using namespace resonant;
using namespace resonant::automation;

WorkflowsDialog::WorkflowsDialog(DocumentController& document, Apply apply, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::WorkflowsDialog>()), document_(document), apply_(std::move(apply))
{
    ui_->setupUi(this);
    setProperty("helpTopic", QStringLiteral("py.workflows"));
    ui_->splitter->setStretchFactor(1, 2);
    ui_->splitter->setSizes({360, 740});
    ui_->parametersTable->horizontalHeader()->setStretchLastSection(true);
    // Shown with ~ for the home directory (expanded when the workflow runs).
    QString out = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/Resonant workflows");
    if (out.startsWith(QDir::homePath()))
        out = QStringLiteral("~") + out.mid(QDir::homePath().size());
    ui_->outputEdit->setText(out);
    connect(ui_->workflowList, &QListWidget::currentRowChanged, this, &WorkflowsDialog::showWorkflow);
    connect(ui_->runButton, &QPushButton::clicked, this, &WorkflowsDialog::run);
    connect(ui_->stopButton, &QPushButton::clicked, this, &WorkflowsDialog::stop);
    connect(ui_->browseButton, &QPushButton::clicked, this, [this] {
        QString start = ui_->outputEdit->text();
        if (start.startsWith(QLatin1Char('~')))
            start = QDir::homePath() + start.mid(1);
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Output Folder"), start);
        if (!dir.isEmpty())
            ui_->outputEdit->setText(dir);
    });
    connect(ui_->buttonBox->button(QDialogButtonBox::Help), &QPushButton::clicked, this, [this] { emit helpRequested(QStringLiteral("py.workflows")); });
    connect(&document_, &DocumentController::jobChanged, this, &WorkflowsDialog::onJob);
    connect(&document_, &DocumentController::projectReplaced, this, &WorkflowsDialog::fillHistory);
    connect(&document_, &DocumentController::documentStateChanged, this, &WorkflowsDialog::fillHistory);  // undo / redo
    ui_->stopButton->setEnabled(false);
    fillList();
    fillHistory();
}

WorkflowsDialog::~WorkflowsDialog() = default;

void WorkflowsDialog::fillList()
{
    ui_->workflowList->clear();
    for (const Workflow& w : WorkflowLibrary::instance().workflows()) {
        auto* item = new QListWidgetItem(QString::fromStdString(w.name));
        item->setData(Qt::UserRole, QString::fromStdString(w.id));
        item->setToolTip(QString::fromStdString(w.id + " (" + WorkflowLibrary::instance().sourceOf(w.id) + ")"));
        ui_->workflowList->addItem(item);
    }
    if (ui_->workflowList->count() > 0)
        ui_->workflowList->setCurrentRow(0);
}

void WorkflowsDialog::fillHistory()
{
    ui_->historyList->clear();
    for (const WorkflowRun& r : workflowHistory(document_.project()))
        ui_->historyList->addItem(QStringLiteral("%1  %2  %3 (%4 s)%5")
                                      .arg(QString::fromStdString(r.startedUtc), QString::fromStdString(r.workflowId),
                                           QString::fromStdString(std::string(toString(r.status))), QString::number(r.seconds, 'f', 2),
                                           r.message.empty() ? QString() : QStringLiteral(": ") + QString::fromStdString(r.message)));
}

QStringList WorkflowsDialog::workflowIds() const
{
    QStringList out;
    for (int k = 0; k < ui_->workflowList->count(); ++k)
        out.append(ui_->workflowList->item(k)->data(Qt::UserRole).toString());
    return out;
}

bool WorkflowsDialog::selectWorkflow(const QString& id)
{
    for (int k = 0; k < ui_->workflowList->count(); ++k)
        if (ui_->workflowList->item(k)->data(Qt::UserRole).toString() == id) {
            ui_->workflowList->setCurrentRow(k);
            return true;
        }
    return false;
}

QString WorkflowsDialog::selectedWorkflow() const
{
    const auto* item = ui_->workflowList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void WorkflowsDialog::showWorkflow()
{
    const auto w = WorkflowLibrary::instance().find(selectedWorkflow().toStdString());
    ui_->parametersTable->setRowCount(0);
    if (!w) {
        ui_->descriptionView->clear();
        return;
    }
    QString html = QStringLiteral("<h3>%1</h3><p>%2</p><p><i>%3 steps; about %4 s; from %5.</i></p>")
                       .arg(QString::fromStdString(w->name).toHtmlEscaped(), QString::fromStdString(w->description).toHtmlEscaped())
                       .arg(w->steps.size())
                       .arg(w->expectedSeconds)
                       .arg(QString::fromStdString(WorkflowLibrary::instance().sourceOf(w->id)).toHtmlEscaped());
    html += QStringLiteral("<p>Steps: ");
    for (size_t k = 0; k < w->steps.size(); ++k)
        html += (k ? QStringLiteral(" → ") : QString()) + QString::fromStdString(w->steps[k].action).toHtmlEscaped();
    html += QStringLiteral("</p>");
    ui_->descriptionView->setHtml(html);
    ui_->parametersTable->setRowCount(static_cast<int>(w->parameters.size()));
    for (int r = 0; r < static_cast<int>(w->parameters.size()); ++r) {
        const WorkflowParameter& p = w->parameters[static_cast<size_t>(r)];
        auto* name = new QTableWidgetItem(QString::fromStdString(p.label.empty() ? p.id : p.label));
        name->setData(Qt::UserRole, QString::fromStdString(p.id));
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        name->setToolTip(QString::fromStdString(p.description));
        ui_->parametersTable->setItem(r, 0, name);
        ui_->parametersTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(p.defaultValue.dump())));
        auto* unit = new QTableWidgetItem(QString::fromStdString(p.unit));
        unit->setFlags(unit->flags() & ~Qt::ItemIsEditable);
        ui_->parametersTable->setItem(r, 2, unit);
    }
}

bool WorkflowsDialog::setParameter(const QString& id, const QString& jsonValue)
{
    for (int r = 0; r < ui_->parametersTable->rowCount(); ++r)
        if (ui_->parametersTable->item(r, 0)->data(Qt::UserRole).toString() == id) {
            ui_->parametersTable->item(r, 1)->setText(jsonValue);
            return true;
        }
    return false;
}

void WorkflowsDialog::setOutputDirectory(const QString& directory)
{
    ui_->outputEdit->setText(directory);
}

bool WorkflowsDialog::isRunning() const
{
    return static_cast<bool>(job_);
}

QString WorkflowsDialog::resultText() const
{
    return ui_->resultView->toPlainText();
}

int WorkflowsDialog::historyCount() const
{
    return ui_->historyList->count();
}

bool WorkflowsDialog::run()
{
    if (job_)
        return false;
    const auto w = WorkflowLibrary::instance().find(selectedWorkflow().toStdString());
    if (!w)
        return false;
    nlohmann::json params = nlohmann::json::object();
    for (int r = 0; r < ui_->parametersTable->rowCount(); ++r) {
        const std::string id = ui_->parametersTable->item(r, 0)->data(Qt::UserRole).toString().toStdString();
        const std::string text = ui_->parametersTable->item(r, 1)->text().toStdString();
        try {
            params[id] = nlohmann::json::parse(text);
        } catch (const std::exception&) {
            params[id] = text;  // plain text for a string parameter
        }
    }
    auto resolved = resolveParameters(*w, params);
    if (!resolved) {
        ui_->resultView->setPlainText(tr("The parameters are not valid: %1").arg(QString::fromStdString(resolved.message())));
        return false;
    }
    QString out = ui_->outputEdit->text().trimmed();
    if (out.startsWith(QLatin1Char('~')))
        out = QDir::homePath() + out.mid(1);
    QDir().mkpath(out);
    job_ = std::make_shared<WorkflowJob>(serializeProject(document_.project()), *w, params, out.toStdString());
    ui_->resultView->setPlainText(tr("Running %1 on a copy of the project...").arg(QString::fromStdString(w->name)));
    ui_->progressBar->setValue(0);
    ui_->runButton->setEnabled(false);
    ui_->stopButton->setEnabled(true);
    jobId_ = document_.submitJob(job_);
    return true;
}

void WorkflowsDialog::stop()
{
    if (job_)
        document_.cancelJob(jobId_);
}

void WorkflowsDialog::onJob(const JobInfo& info)
{
    if (!job_ || info.id != jobId_)
        return;
    ui_->progressBar->setValue(static_cast<int>(info.progress * 100.0));
    if (info.state == JobState::Queued || info.state == JobState::Running)
        return;
    const auto job = job_;
    job_.reset();
    ui_->runButton->setEnabled(true);
    ui_->stopButton->setEnabled(false);
    const auto result = job->result();
    QString text;
    bool ok = false;
    if (!result)
        text = tr("The workflow did not run: %1").arg(QString::fromStdString(info.message));
    else {
        const WorkflowRun& r = *result;
        ok = r.status == RunStatus::Completed;
        text = tr("%1: %2 in %3 s%4\n\n")
                   .arg(QString::fromStdString(r.workflowId), QString::fromStdString(std::string(toString(r.status))))
                   .arg(r.seconds, 0, 'f', 2)
                   .arg(r.message.empty() ? QString() : QStringLiteral(" (") + QString::fromStdString(r.message) + QLatin1Char(')'));
        for (const StepRecord& s : r.steps)
            text += QStringLiteral("  %1  %2  %3\n").arg(QString::fromStdString(s.stepId + (s.iteration >= 0 ? "[" + std::to_string(s.iteration) + "]" : std::string())), QString::fromStdString(std::string(toString(s.status))),
                                                       QString::fromStdString(s.message));
        text += QStringLiteral("\nOutputs:\n") + QString::fromStdString(r.outputs.dump(2)).left(20000);
        // Record the run (any outcome) and, for a completed run, the changes it made.
        auto loaded = deserializeProject(ok && ui_->applyCheck->isChecked() ? job->resultingProject() : serializeProject(document_.project()));
        if (loaded) {
            Project& changed = *loaded.value().project;
            changed.extensions[kAutomationExtensionKey] = appendHistory(document_.project(), r);
            const Status applied = apply_(changed, QStringLiteral("Workflow: ") + QString::fromStdString(r.workflowId));
            if (!applied)
                text += tr("\n\nThe changes could not be applied: %1").arg(QString::fromStdString(applied.message()));
        }
    }
    ui_->resultView->setPlainText(text);
    fillHistory();
    emit runFinished(ok, text.section(QLatin1Char('\n'), 0, 0));
}
