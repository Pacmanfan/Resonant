#include "DemoStudioPanel.h"

#include "DemoController.h"
#include "ui_DemoStudioPanel.h"

#include <QFileDialog>
#include <QFileInfo>

using namespace resonant::demo;

namespace {
QString qs(const std::string& s)
{
    return QString::fromStdString(s);
}
QString esc(const std::string& s)
{
    return QString::fromStdString(s).toHtmlEscaped();
}
}  // namespace

DemoStudioPanel::DemoStudioPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::DemoStudioPanel>())
{
    ui_->setupUi(this);
    connect(ui_->profileList, &QListWidget::currentRowChanged, this, &DemoStudioPanel::showProfile);
    connect(ui_->stepsList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!controller_)
            return;
        const LoadedDemo* d = controller_->library().find(selectedProfile().toStdString());
        if (d && row >= 0 && row < static_cast<int>(d->sequence.steps.size()))
            ui_->notesView->setPlainText(qs(d->sequence.steps[static_cast<size_t>(row)].notes));
    });
    connect(ui_->runButton, &QPushButton::clicked, this, [this] { run(ui_->attendedCheck->isChecked(), ui_->presenterCheck->isChecked()); });
    connect(ui_->nextButton, &QPushButton::clicked, this, [this] {
        if (controller_)
            controller_->next();
    });
    connect(ui_->pauseButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        if (controller_->isPaused())
            controller_->resume();
        else
            controller_->pause();
    });
    connect(ui_->stopButton, &QPushButton::clicked, this, [this] {
        if (controller_)
            controller_->stop();
    });
    connect(ui_->restartButton, &QPushButton::clicked, this, [this] {
        if (controller_ && (!confirm_ || confirm_()))
            controller_->restart();
    });
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        const QString path = QFileDialog::getSaveFileName(this, tr("Export Run Manifest"), controller_->manifestPath(), tr("JSON (*.json)"));
        if (!path.isEmpty())
            ui_->statusLabel->setText(controller_->exportManifest(path) ? tr("Manifest written to %1").arg(QFileInfo(path).fileName())
                                                                        : tr("The manifest could not be written."));
    });
    updateButtons();
}

DemoStudioPanel::~DemoStudioPanel() = default;

void DemoStudioPanel::setController(DemoController* controller)
{
    controller_ = controller;
    connect(controller_, &DemoController::stateChanged, this, &DemoStudioPanel::updateButtons);
    connect(controller_, &DemoController::runStarted, this, [this] {
        showProfile();
        ui_->statusLabel->setText(tr("Running..."));
    });
    connect(controller_, &DemoController::stepStarted, this, [this](int index) {
        markStep(index, QStringLiteral("▶"));
        ui_->stepsList->setCurrentRow(index);
    });
    connect(controller_, &DemoController::stepFinished, this, [this](int index, bool ok, const QString& message) {
        markStep(index, ok ? QStringLiteral("✓") : QStringLiteral("✗"));
        if (!ok)
            ui_->statusLabel->setText(tr("Step %1 failed: %2").arg(index + 1).arg(message));
    });
    connect(controller_, &DemoController::waitingForPresenter, this, [this](int, const QString& text) {
        ui_->statusLabel->setText(tr("Waiting for Next: %1").arg(text));
    });
    connect(controller_, &DemoController::runFinished, this, [this](const QString& status, const QString& message) {
        ui_->statusLabel->setText(message.isEmpty() ? tr("Finished: %1. Manifest: %2").arg(status, QFileInfo(controller_->manifestPath()).fileName())
                                                    : tr("Finished: %1 (%2)").arg(status, message));
    });
    refresh();
}

void DemoStudioPanel::refresh()
{
    const QString selected = selectedProfile();
    ui_->profileList->clear();
    if (!controller_)
        return;
    for (const LoadedDemo& d : controller_->library().demos) {
        QString label = qs(d.profile.name);
        if (d.profile.status != "available")
            label += QStringLiteral(" (%1)").arg(qs(d.profile.status));
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, qs(d.profile.id));
        item->setToolTip(qs(d.profile.description));
        ui_->profileList->addItem(item);
    }
    if (!controller_->library().errors.empty()) {
        QStringList errors;
        for (const auto& e : controller_->library().errors)
            errors.append(qs(e));
        ui_->statusLabel->setText(tr("Not loaded: %1").arg(errors.join(QStringLiteral("; "))));
    }
    if (!selectProfile(selected) && ui_->profileList->count() > 0)
        ui_->profileList->setCurrentRow(0);
}

QStringList DemoStudioPanel::profileIds() const
{
    QStringList out;
    for (int k = 0; k < ui_->profileList->count(); ++k)
        out.append(ui_->profileList->item(k)->data(Qt::UserRole).toString());
    return out;
}

bool DemoStudioPanel::selectProfile(const QString& id)
{
    for (int k = 0; k < ui_->profileList->count(); ++k)
        if (ui_->profileList->item(k)->data(Qt::UserRole).toString() == id) {
            ui_->profileList->setCurrentRow(k);
            return true;
        }
    return false;
}

QString DemoStudioPanel::selectedProfile() const
{
    const auto* item = ui_->profileList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void DemoStudioPanel::showProfile()
{
    ui_->stepsList->clear();
    ui_->notesView->clear();
    if (!controller_)
        return;
    const LoadedDemo* d = controller_->library().find(selectedProfile().toStdString());
    if (!d) {
        ui_->profileView->clear();
        return;
    }
    const DemoProfile& p = d->profile;
    const auto list = [](const std::vector<std::string>& items) {
        QString out;
        for (const auto& s : items)
            out += QStringLiteral("<li>%1</li>").arg(esc(s));
        return out.isEmpty() ? QString() : QStringLiteral("<ul>%1</ul>").arg(out);
    };
    QString html = QStringLiteral("<h3>%1</h3><p>%2</p><p><b>Status:</b> %3 &nbsp; <b>Domain:</b> %4 &nbsp; <b>Audience:</b> %5 &nbsp; "
                                  "<b>About</b> %6 min</p>")
                       .arg(esc(p.name), esc(p.description), esc(p.status), esc(p.domain), esc(p.audience))
                       .arg(p.durationMinutes, 0, 'f', 0);
    html += QStringLiteral("<p><b>Problem:</b> %1</p><p><b>You configure:</b> %2</p><p><b>Resonant calculates:</b> %3</p>"
                           "<p><b>What it means:</b> %4</p><p><b>Inspect it:</b> %5</p><p><b>Why it matters:</b> %6</p>"
                           "<p><b>Assumptions and limits:</b> %7</p>")
                .arg(esc(p.story.problem), esc(p.story.configure), esc(p.story.calculate), esc(p.story.meaning), esc(p.story.inspect),
                     esc(p.story.value), esc(p.story.assumptions));
    if (!p.expected.empty())
        html += QStringLiteral("<p><b>Expected results:</b></p>") + list(p.expected);
    if (!p.remaining.empty())
        html += QStringLiteral("<p><b>Not yet part of this demonstration:</b></p>") + list(p.remaining);
    if (!p.project.empty())
        html += QStringLiteral("<p><b>Project:</b> <code>%1</code></p>").arg(esc(p.project));
    ui_->profileView->setHtml(html);
    for (const DemoStep& s : d->sequence.steps)
        ui_->stepsList->addItem(QStringLiteral("   %1  %2").arg(qs(s.id), qs(s.description.empty() ? s.action : s.description)));
    updateButtons();
}

void DemoStudioPanel::markStep(int index, const QString& state)
{
    if (index < 0 || index >= ui_->stepsList->count())
        return;
    QListWidgetItem* item = ui_->stepsList->item(index);
    item->setText(state + item->text().mid(1));
}

int DemoStudioPanel::stepRows() const
{
    return ui_->stepsList->count();
}

QString DemoStudioPanel::stepText(int row) const
{
    return row >= 0 && row < ui_->stepsList->count() ? ui_->stepsList->item(row)->text() : QString();
}

QString DemoStudioPanel::profileText() const
{
    return ui_->profileView->toPlainText();
}

QString DemoStudioPanel::statusText() const
{
    return ui_->statusLabel->text();
}

bool DemoStudioPanel::run(bool attended, bool presenter)
{
    if (!controller_ || controller_->isRunning())
        return false;
    if (confirm_ && !confirm_())
        return false;
    showProfile();
    if (presenter && presenter_)
        presenter_(true);
    const bool started = controller_->startProfile(selectedProfile(), attended);
    if (!started) {
        ui_->statusLabel->setText(tr("The demonstration could not start (see the Console)."));
        if (presenter && presenter_)
            presenter_(false);
    }
    return started;
}

void DemoStudioPanel::updateButtons()
{
    const bool running = controller_ && controller_->isRunning();
    ui_->runButton->setEnabled(!running && !selectedProfile().isEmpty());
    ui_->nextButton->setEnabled(running && (controller_->isWaitingForPresenter() || controller_->isPaused()));
    ui_->pauseButton->setEnabled(running);
    ui_->pauseButton->setText(controller_ && controller_->isPaused() ? tr("Resume") : tr("Pause"));
    ui_->stopButton->setEnabled(running);
    ui_->restartButton->setEnabled(!running && controller_ && !controller_->sequence().steps.empty());
    ui_->exportButton->setEnabled(controller_ && !controller_->manifestPath().isEmpty());
}
