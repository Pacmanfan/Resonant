#include "ExperimentsPanel.h"

#include "ExperimentsController.h"
#include "OutputAuthorizationDialog.h"
#include "ui_ExperimentsPanel.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QTreeWidgetItem>

#include <cmath>
#include <fstream>

using namespace resonant;
using namespace resonant::experiments;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }
QString qs(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }

QTreeWidgetItem* add(QTreeWidgetItem* parent, const QString& name, const QString& value = {})
{
    auto* item = new QTreeWidgetItem(parent, {name, value});
    return item;
}

QString poseText(const Pose& p)
{
    return QStringLiteral("(%1, %2, %3) m, %4").arg(p.x, 0, 'g', 4).arg(p.y, 0, 'g', 4).arg(p.z, 0, 'g', 4).arg(qs(p.frame));
}

}  // namespace

ExperimentsPanel::ExperimentsPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::ExperimentsPanel>())
{
    ui_->setupUi(this);
    ui_->emergencyButton->setStyleSheet(QStringLiteral("QPushButton { background: #c62828; color: white; font-weight: bold; }"));
    ui_->emergencyButton->setToolTip(tr("Stops every output at once and latches until reset"));
    ui_->deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui_->calibrationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    liveTimer_.setInterval(200);
    connect(&liveTimer_, &QTimer::timeout, this, &ExperimentsPanel::refreshLive);

    connect(ui_->discoverButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        ui_->discoveryLabel->setText(tr("Discovering..."));
        controller_->discoverDevices();
    });
    connect(ui_->deviceTable, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto rows = ui_->deviceTable->selectionModel()->selectedRows();
        if (!controller_ || rows.isEmpty())
            return;
        const std::string id = ui_->deviceTable->item(rows.front().row(), 0)->data(Qt::UserRole).toString().toStdString();
        auto caps = controller_->capabilities(id);
        ui_->capabilitiesText->setPlainText(caps ? qs(toJson(caps.value()).dump(1)) : tr("Cannot open the device: %1").arg(qs(caps.message())));
        refreshDevices();
    });
    connect(ui_->addButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        auto id = controller_->addFromTemplate(ui_->templateCombo->currentData().toString());
        if (!id)
            setStatus(qs(id.message()), true);
        else
            selectExperiment(id.value());
    });
    connect(ui_->removeButton, &QPushButton::clicked, this, [this] {
        if (controller_ && !selectedExperiment().empty())
            if (Status ok = controller_->removeExperiment(selectedExperiment()); !ok)
                setStatus(qs(ok.message()), true);
    });
    connect(ui_->experimentList, &QListWidget::currentRowChanged, this, [this] {
        if (updating_)
            return;
        refreshDefinition();
        refreshCalibrations();
        refreshRun();
        refreshMeasurements();
        emit selectedExperimentChanged();
    });
    connect(ui_->selectCalibrationButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        const Status ok = controller_->selectCalibration(selectedExperiment(), ui_->sensorCombo->currentData().toString().toStdString(),
                                                          ui_->calibrationCombo->currentData().toString().toStdString());
        setStatus(ok ? tr("Calibration selected.") : qs(ok.message()), !ok);
    });
    connect(ui_->importCalibrationButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import Calibration Record"), {}, tr("Calibration record (*.json)"));
        if (!path.isEmpty() && controller_)
            if (Status ok = controller_->importCalibration(path); !ok)
                setStatus(qs(ok.message()), true);
    });
    connect(ui_->authorizeButton, &QPushButton::clicked, this, [this] { authorizeSelected(); });
    connect(ui_->runButton, &QPushButton::clicked, this, [this] { runSelected(); });
    connect(ui_->stopButton, &QPushButton::clicked, this, &ExperimentsPanel::stop);
    connect(ui_->emergencyButton, &QPushButton::clicked, this, &ExperimentsPanel::emergencyStop);
    connect(ui_->resetEmergencyButton, &QPushButton::clicked, this, [this] {
        if (controller_)
            controller_->resetEmergencyStop(QStringLiteral("operator"));
    });
    connect(ui_->measurementTree, &QTreeWidget::currentItemChanged, this, &ExperimentsPanel::showMeasurement);
    connect(ui_->viewCombo, &QComboBox::currentIndexChanged, this, &ExperimentsPanel::showMeasurement);
    connect(ui_->importTouchstoneButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import Touchstone"), {}, tr("Touchstone (*.s1p *.s2p *.s3p *.s4p *.S1P *.S2P)"));
        if (!path.isEmpty() && controller_) {
            const Status ok = controller_->importTouchstone(path, selectedExperiment());
            setStatus(ok ? tr("Imported %1 (an imported, not acquired, measurement).").arg(path) : qs(ok.message()), !ok);
        }
    });
    connect(ui_->compareButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        auto c = controller_->compareWithSimulation(selectedExperiment(), selectedMeasurement());
        ui_->comparisonText->setPlainText(c ? tr("Compared with: %1\n%2").arg(qs(c.value().simulatedLabel), qs(c.value().metrics.dump(1)))
                                            : tr("No comparison: %1").arg(qs(c.message())));
    });
    connect(ui_->reportButton, &QPushButton::clicked, this, [this] {
        if (!controller_)
            return;
        const QString html = controller_->reportHtml(selectedExperiment());
        if (html.isEmpty()) {
            setStatus(tr("The experiment has no report yet: run it first."), true);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, tr("Save Report"), {}, tr("HTML report (*.html)"));
        if (!path.isEmpty())
            std::ofstream(path.toStdString()) << html.toStdString();
    });
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getSaveFileName(this, tr("Export Experiment Archive (a new directory)"));
        if (!dir.isEmpty() && controller_) {
            const Status ok = controller_->exportArchive(selectedExperiment(), dir);
            setStatus(ok ? tr("Archive written to %1.").arg(dir) : qs(ok.message()), !ok);
        }
    });
    connect(ui_->importArchiveButton, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Import Experiment Archive"));
        if (!dir.isEmpty() && controller_) {
            const Status ok = controller_->importArchive(dir);
            setStatus(ok ? tr("Archive imported.") : qs(ok.message()), !ok);
        }
    });
}

ExperimentsPanel::~ExperimentsPanel() = default;

void ExperimentsPanel::setController(ExperimentsController* controller)
{
    controller_ = controller;
    ui_->templateCombo->clear();
    for (const auto& [id, title] : controller_->templates())
        ui_->templateCombo->addItem(title, id);
    ui_->driversLabel->setText(controller_->driverSummary());
    connect(controller_, &ExperimentsController::devicesChanged, this, &ExperimentsPanel::refreshDevices);
    connect(controller_, &ExperimentsController::experimentsChanged, this, [this] {
        refreshExperiments();
        refreshDefinition();
        refreshCalibrations();
        refreshMeasurements();
    });
    connect(controller_, &ExperimentsController::runStateChanged, this, &ExperimentsPanel::refreshRun);
    connect(controller_, &ExperimentsController::runFinished, this, [this](const QString& message) {
        liveTimer_.stop();
        setStatus(message);
        refreshMeasurements();
    });
    refreshDevices();
    refreshExperiments();
    refreshRun();
}

std::string ExperimentsPanel::selectedExperiment() const
{
    const auto* item = ui_->experimentList->currentItem();
    return item ? item->data(Qt::UserRole).toString().toStdString() : std::string();
}

void ExperimentsPanel::selectExperiment(const std::string& id)
{
    for (int i = 0; i < ui_->experimentList->count(); ++i)
        if (ui_->experimentList->item(i)->data(Qt::UserRole).toString().toStdString() == id)
            ui_->experimentList->setCurrentRow(i);
}

std::string ExperimentsPanel::selectedMeasurement() const
{
    const auto* item = ui_->measurementTree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString().toStdString() : std::string();
}

QString ExperimentsPanel::statusText() const { return ui_->statusLabel->text(); }

QString ExperimentsPanel::definitionText() const
{
    QStringList lines;
    std::function<void(QTreeWidgetItem*, int)> walk = [&](QTreeWidgetItem* item, int depth) {
        lines << QString(depth * 2, ' ') + item->text(0) + QStringLiteral(": ") + item->text(1);
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i), depth + 1);
    };
    for (int i = 0; i < ui_->definitionTree->topLevelItemCount(); ++i)
        walk(ui_->definitionTree->topLevelItem(i), 0);
    return lines.join(QStringLiteral("\n"));
}

int ExperimentsPanel::measurementCount() const
{
    int n = 0;
    for (int i = 0; i < ui_->measurementTree->topLevelItemCount(); ++i)
        n += ui_->measurementTree->topLevelItem(i)->childCount();
    return n;
}

void ExperimentsPanel::setStatus(const QString& text, bool error)
{
    ui_->statusLabel->setText(text);
    ui_->statusLabel->setStyleSheet(error ? QStringLiteral("color: #c62828") : QString());
}

void ExperimentsPanel::refreshDevices()
{
    if (!controller_)
        return;
    const QString discovery = controller_->discovering() ? tr("Discovering...") : controller_->discoveryReport();
    ui_->discoveryLabel->setText(discovery.isEmpty() ? tr("Discovery has not run.") : discovery.section('\n', 0, 0));
    const auto devices = controller_->devices();
    QSignalBlocker block(ui_->deviceTable);
    ui_->deviceTable->setRowCount(static_cast<int>(devices.size()));
    for (int r = 0; r < static_cast<int>(devices.size()); ++r) {
        const auto& d = devices[static_cast<size_t>(r)];
        auto* name = new QTableWidgetItem(qs(d.name));
        name->setData(Qt::UserRole, qs(d.id));
        name->setToolTip(qs(d.id));
        ui_->deviceTable->setItem(r, 0, name);
        ui_->deviceTable->setItem(r, 1, new QTableWidgetItem(qs(d.driver)));
        ui_->deviceTable->setItem(r, 2, new QTableWidgetItem(qs(toString(d.connection))));
        ui_->deviceTable->setItem(r, 3, new QTableWidgetItem(controller_->deviceStatus(d.id)));
        ui_->deviceTable->setItem(r, 4, new QTableWidgetItem(d.simulated ? tr("generated (simulated)") : tr("measured")));
    }
}

void ExperimentsPanel::refreshExperiments()
{
    if (!controller_)
        return;
    const std::string current = selectedExperiment();
    updating_ = true;
    ui_->experimentList->clear();
    const ExperimentWorkspace w = controller_->workspace();
    for (const auto& e : w.experiments) {
        auto* item = new QListWidgetItem(QStringLiteral("%1  [%2, %3]").arg(qs(e.name), qs(toString(e.domain)), qs(toString(e.state))));
        item->setData(Qt::UserRole, qs(e.id));
        ui_->experimentList->addItem(item);
    }
    updating_ = false;
    if (!current.empty())
        selectExperiment(current);
    if (!ui_->experimentList->currentItem() && ui_->experimentList->count() > 0)
        ui_->experimentList->setCurrentRow(0);
    refreshDefinition();
    refreshRun();
}

void ExperimentsPanel::refreshDefinition()
{
    ui_->definitionTree->clear();
    if (!controller_)
        return;
    const ExperimentWorkspace w = controller_->workspace();
    const Experiment* e = w.experiment(selectedExperiment());
    if (!e) {
        ui_->summaryLabel->setText(tr("No experiment selected."));
        ui_->validationText->clear();
        return;
    }
    ui_->summaryLabel->setText(tr("%1 (%2 domain, revision %3): %4").arg(qs(e->name), qs(toString(e->domain))).arg(e->revision).arg(qs(e->description)));
    auto* root = ui_->definitionTree->invisibleRootItem();
    auto* sensors = add(root, tr("Sensors"), QString::number(e->sensors.size()));
    for (const auto& s : e->sensors)
        add(sensors, qs(s.name), tr("%1 measuring %2 at %3; calibration %4")
                                     .arg(qs(toString(s.category)), qs(toString(s.measures)), poseText(s.pose),
                                          s.calibrationId.empty() ? tr("none (instrument units)") : qs(s.calibrationId)));
    auto* actuators = add(root, tr("Actuators"), QString::number(e->actuators.size()));
    for (const auto& a : e->actuators)
        add(actuators, qs(a.name), tr("%1: %2 command -> %3 output (commanded is not physical output)")
                                       .arg(qs(toString(a.category)), qs(toString(a.commandQuantity)), qs(toString(a.outputQuantity))));
    auto* channels = add(root, tr("Channel bindings"), QString::number(e->channels.size()));
    for (const auto& b : e->channels)
        add(channels, qs(b.id), tr("%1 %2/%3 -> %4").arg(qs(toString(b.direction)), qs(b.deviceId), qs(b.channelId),
                                                          qs(b.sensorId.empty() ? b.actuatorId : b.sensorId)));
    add(root, tr("Stimulus"), tr("%1, %2 ... %3 Hz, amplitude %4, %5 s")
                                  .arg(qs(toString(e->stimulus.kind)))
                                  .arg(e->stimulus.startHz)
                                  .arg(e->stimulus.endHz)
                                  .arg(e->stimulus.amplitude)
                                  .arg(e->stimulus.durationSeconds));
    add(root, tr("Acquisition"), tr("%1 Hz, %2 s, %3 repetition(s)").arg(e->acquisition.sampleRateHz).arg(e->acquisition.durationSeconds).arg(e->acquisition.repetitions));
    add(root, tr("Synchronization"), tr("%1%2").arg(e->synchronization.phaseSensitive ? tr("phase-sensitive, ") : QString(), qs(e->synchronization.strategy)));
    add(root, tr("Safety limits"), tr("max %1 %2, %3 ... %4 Hz, %5 permitted output(s)")
                                       .arg(e->safety.maxOutputAmplitude)
                                       .arg(qs(e->safety.amplitudeUnit))
                                       .arg(e->safety.minFrequencyHz)
                                       .arg(e->safety.maxFrequencyHz)
                                       .arg(e->safety.allowedOutputChannels.size()));
    if (!e->positions.empty())
        add(root, tr("Positions"), qs(toString(e->positions.kind)));
    auto* processing = add(root, tr("Processing"), QString::number(e->processing.size()));
    for (const auto& p : e->processing)
        add(processing, qs(p.operation), qs(p.parameters.dump()));
    add(root, tr("Runs"), QString::number(e->history.size()));
    ui_->definitionTree->expandAll();
    const ValidationReport report = controller_->validate(e->id);
    QStringList lines;
    for (const auto& err : report.errors)
        lines << tr("Error: %1").arg(qs(err));
    for (const auto& warn : report.warnings)
        lines << tr("Warning: %1").arg(qs(warn));
    ui_->validationText->setPlainText(lines.isEmpty() ? tr("Valid.") : lines.join(QStringLiteral("\n")));
}

void ExperimentsPanel::refreshCalibrations()
{
    if (!controller_)
        return;
    const ExperimentWorkspace w = controller_->workspace();
    const auto& records = w.calibrations.all();
    ui_->calibrationTable->setRowCount(static_cast<int>(records.size()));
    ui_->calibrationCombo->clear();
    ui_->calibrationCombo->addItem(tr("(none)"), QString());
    for (int r = 0; r < static_cast<int>(records.size()); ++r) {
        const auto& c = records[static_cast<size_t>(r)];
        const QString value = c.frequencyDependent() ? tr("%1 points").arg(c.response.size()) : QStringLiteral("%1 %2").arg(c.scalar).arg(qs(c.unit));
        const QStringList cells{qs(c.id), QString::number(c.version), qs(toString(c.type)),
                                qs(c.sensorId.empty() ? c.deviceId + "/" + c.channelId : c.sensorId), value,
                                c.validUntilUtc.empty() ? tr("not stated") : qs(c.validUntilUtc),
                                qs(c.source) + (c.fromSimulatedDevice ? tr(" (simulated)") : QString())};
        for (int k = 0; k < cells.size(); ++k)
            ui_->calibrationTable->setItem(r, k, new QTableWidgetItem(cells[k]));
        ui_->calibrationCombo->addItem(qs(c.id), qs(c.id));
    }
    ui_->calibrationText->setPlainText(tr("Calibration records are never changed or deleted: a new calibration is a new record that "
                                          "supersedes the previous one. %1 record(s).").arg(records.size()));
    ui_->sensorCombo->clear();
    if (const Experiment* e = w.experiment(selectedExperiment()))
        for (const auto& s : e->sensors)
            ui_->sensorCombo->addItem(qs(s.name), qs(s.id));
}

void ExperimentsPanel::refreshRun()
{
    if (!controller_)
        return;
    const std::string id = selectedExperiment();
    const bool running = controller_->running();
    const bool emergency = controller_->emergencyStopActive();
    const auto check = controller_->safetyCheck(id);
    QStringList lines;
    const ExperimentWorkspace w = controller_->workspace();
    const Experiment* e = w.experiment(id);
    if (!e)
        lines << tr("No experiment selected.");
    else if (!e->hasPhysicalOutput())
        lines << tr("This experiment drives no physical output.");
    else {
        lines << (check.allowed ? tr("The output lies within the safety limits.") : tr("The output is NOT within the safety limits:"));
        for (const auto& v : check.violations)
            lines << QStringLiteral("- ") + qs(v);
        for (const auto& warn : check.warnings)
            lines << QStringLiteral("! ") + qs(warn);
    }
    if (emergency)
        lines.prepend(tr("EMERGENCY STOP is latched: no output is possible until it is reset."));
    ui_->safetyText->setPlainText(lines.join(QStringLiteral("\n")));
    const bool needsOutput = e && e->hasPhysicalOutput();
    const bool authorized = e && controller_->isAuthorized(id);
    ui_->authorizationLabel->setText(!needsOutput ? tr("No output: no authorization needed.")
                                                  : authorized ? tr("Authorized for one run.") : tr("Not authorized: the run will not start."));
    ui_->authorizeButton->setEnabled(needsOutput && check.allowed && !running && !emergency);
    ui_->runButton->setEnabled(e && !running && (!needsOutput || authorized));
    ui_->stopButton->setEnabled(running);
    ui_->resetEmergencyButton->setEnabled(emergency && !running);
    const RunProgress& p = controller_->progress();
    if (running) {
        ui_->runProgress->setValue(static_cast<int>(std::lround(p.fraction * 100.0)));
        ui_->stateLabel->setText(tr("%1 - step %2: %3").arg(qs(toString(p.state))).arg(p.step).arg(qs(p.message)));
        ui_->statsLabel->setText(tr("%1 frames read, %2 dropped, buffer %3 / %4, %5 kB/s to disk")
                                     .arg(p.stats.framesRead)
                                     .arg(p.stats.framesDropped)
                                     .arg(p.stats.queueOccupancy)
                                     .arg(p.stats.queueCapacity)
                                     .arg(p.stats.diskBytesPerSecond / 1000.0, 0, 'f', 1));
        if (!liveTimer_.isActive())
            liveTimer_.start();
    } else if (const RunResult* r = controller_->lastResult()) {
        ui_->runProgress->setValue(r->state == ExperimentState::Completed ? 100 : ui_->runProgress->value());
        ui_->stateLabel->setText(qs(toString(r->state)) + QStringLiteral(": ") + qs(r->record.message));
        QStringList steps;
        for (const auto& s : r->record.steps)
            steps << QStringLiteral("%1. %2 - %3: %4").arg(s.step).arg(qs(s.name), qs(s.status), qs(s.message));
        ui_->stepsText->setPlainText(steps.join(QStringLiteral("\n")));
    } else {
        ui_->stateLabel->setText(e ? qs(toString(e->state)) : QString());
    }
}

bool ExperimentsPanel::authorizeSelected(const QString& operatorName)
{
    if (!controller_)
        return false;
    const std::string id = selectedExperiment();
    QString name = operatorName, acknowledgement;
    if (operatorName.isEmpty()) {
        OutputAuthorizationDialog dialog(controller_->outputRequest(id), controller_->safetyCheck(id), this);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        name = dialog.operatorName();
        acknowledgement = dialog.acknowledgement();
    } else {
        acknowledgement = tr("authorized programmatically");
    }
    const Status ok = controller_->authorize(id, name, acknowledgement);
    setStatus(ok ? tr("Output authorized by %1 for one run.").arg(name) : qs(ok.message()), !ok);
    refreshRun();
    return static_cast<bool>(ok);
}

bool ExperimentsPanel::runSelected()
{
    if (!controller_)
        return false;
    const Status ok = controller_->run(selectedExperiment());
    setStatus(ok ? tr("Running...") : qs(ok.message()), !ok);
    refreshRun();
    return static_cast<bool>(ok);
}

void ExperimentsPanel::stop()
{
    if (controller_)
        controller_->stop();
}

void ExperimentsPanel::emergencyStop()
{
    if (controller_)
        controller_->emergencyStop();
    setStatus(tr("EMERGENCY STOP: outputs stopped; reset before the next output."), true);
}

void ExperimentsPanel::refreshMeasurements()
{
    ui_->measurementTree->clear();
    if (!controller_)
        return;
    const auto records = controller_->measurements(selectedExperiment());
    std::map<std::string, QTreeWidgetItem*> runs;
    for (const auto& r : records) {
        const std::string key = r.runId.empty() ? std::string("imported") : r.runId;
        auto*& parent = runs[key];
        if (!parent)
            parent = new QTreeWidgetItem(ui_->measurementTree, {qs(key)});
        auto* item = new QTreeWidgetItem(parent, {qs(r.bindingId.empty() ? r.channelId : r.bindingId) + (r.history.empty() ? QString() : QStringLiteral(" > ") + qs(r.history.back().operation)),
                                                  qs(toString(r.category)) + (r.simulatedDevice ? tr(" (simulated device)") : QString()),
                                                  qs(toString(r.kind)), qs(r.quantity.unit())});
        item->setData(0, Qt::UserRole, qs(r.id));
    }
    ui_->measurementTree->expandAll();
}

void ExperimentsPanel::showMeasurement()
{
    ui_->measurementPlot->clear();
    const std::string id = selectedMeasurement();
    if (!controller_ || id.empty())
        return;
    const auto records = controller_->measurements(selectedExperiment());
    const auto it = std::find_if(records.begin(), records.end(), [&](const MeasurementRecord& r) { return r.id == id; });
    if (it == records.end())
        return;
    const MeasurementRecord& r = *it;
    QStringList meta;
    meta << tr("%1 (%2, %3)").arg(qs(r.id), qs(toString(r.category)), qs(toString(r.kind)));
    meta << tr("Quantity: %1").arg(qs(r.quantity.describe()));
    if (r.simulatedDevice)
        meta << tr("Acquired from a SIMULATED device: generated data, not a measurement.");
    meta << tr("Quality: %1").arg(qs(r.quality.summary()));
    meta << tr("Uncertainty: %1").arg(qs(r.uncertainty.describe(1.0, r.quantity.unit())));
    for (const auto& s : r.history)
        meta << tr("Step %1: %2").arg(qs(s.operation), qs(s.description));
    if (!r.note.empty())
        meta << qs(r.note);
    ui_->measurementText->setPlainText(meta.join(QStringLiteral("\n")));
    auto loaded = controller_->data(id);
    if (!loaded) {
        ui_->measurementText->appendPlainText(tr("Data: %1").arg(qs(loaded.message())));
        return;
    }
    const MeasurementData& d = loaded.value();
    const int view = ui_->viewCombo->currentIndex();
    QVector<double> x, y;
    const size_t n = d.size();
    const size_t step = std::max<size_t>(1, n / 4000);  // a display decimation only
    for (size_t i = 0; i < n; i += step) {
        x.push_back(d.axis.size() == n ? d.axis[i] : static_cast<double>(i) / std::max(1.0, r.timeBase.sampleRateHz));
        const std::complex<double> v = d.isComplex() ? d.complex[i] : std::complex<double>(d.real[i], 0.0);
        double value = std::abs(v);
        if (view == 1)
            value = 20.0 * std::log10(std::max(std::abs(v), 1e-15));
        else if (view == 2)
            value = std::arg(v) * 180.0 / 3.14159265358979323846;
        else if (view == 3)
            value = v.real();
        else if (view == 4)
            value = d.auxiliary.size() == n ? d.auxiliary[i] : 0.0;
        y.push_back(value);
    }
    const QString xLabel = d.axis.size() == n ? tr("Frequency (Hz)") : tr("Time (s)");
    ui_->measurementPlot->setCurve(x, y, qs(r.quantity.describe()) + QStringLiteral(" - ") + qs(toString(r.category)), xLabel,
                                   ui_->viewCombo->currentText());
}

void ExperimentsPanel::refreshLive()
{
    if (!controller_ || !controller_->running()) {
        liveTimer_.stop();
        return;
    }
    const auto snapshot = controller_->liveSnapshot();
    if (!snapshot || snapshot->channels.empty())
        return;
    const auto& ch = snapshot->channels.front();
    QVector<double> x(static_cast<int>(ch.size())), y(static_cast<int>(ch.size()));
    for (int i = 0; i < x.size(); ++i) {
        x[i] = i;
        y[i] = ch[static_cast<size_t>(i)];
    }
    ui_->livePlot->setCurve(x, y, tr("Live: channel 1 (raw reading, latest %1 samples)").arg(ch.size()), tr("Sample"), tr("Reading"));
}
