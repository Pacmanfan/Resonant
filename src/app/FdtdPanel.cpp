#include "FdtdPanel.h"

#include "AcousticController.h"
#include "DocumentController.h"
#include "FdtdController.h"
#include "ui_FdtdPanel.h"

using namespace resonant;
using namespace resonant::fdtd;

namespace {

constexpr WaveformKind kWaveforms[] = {WaveformKind::Continuous, WaveformKind::ToneBurst, WaveformKind::GaussianPulse,
                                       WaveformKind::Ricker, WaveformKind::Chirp};
constexpr BoundaryKind kBoundaries[] = {BoundaryKind::Absorbing, BoundaryKind::Rigid, BoundaryKind::PressureRelease, BoundaryKind::Impedance};

}  // namespace

FdtdPanel::FdtdPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::FdtdPanel>())
{
    ui_->setupUi(this);
    checkTimer_.setSingleShot(true);
    checkTimer_.setInterval(300);
    connect(&checkTimer_, &QTimer::timeout, this, &FdtdPanel::check);

    for (QDoubleSpinBox* spin : {ui_->cyclesSpin, ui_->bandwidthSpin, ui_->signalFrequencySpin, ui_->endFrequencySpin, ui_->sweepDurationSpin,
                                 ui_->durationSpin, ui_->resolutionSpin, ui_->courantSpin, ui_->impedanceSpin, ui_->periodsSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { commit(); });
    for (QSpinBox* spin : {ui_->absorbingSpin, ui_->snapshotSpin, ui_->threadsSpin})
        connect(spin, &QSpinBox::valueChanged, this, [this] { commit(); });
    for (QComboBox* combo : {ui_->waveformCombo, ui_->boundaryCombo, ui_->computeCombo})
        connect(combo, &QComboBox::activated, this, [this] { commit(); });
    connect(ui_->regionCombo, &QComboBox::activated, this, [this] { check(); });
    connect(ui_->checkButton, &QPushButton::clicked, this, &FdtdPanel::check);
    connect(ui_->runButton, &QPushButton::clicked, this, [this] { run(); });
    connect(ui_->cancelButton, &QPushButton::clicked, this, [this] {
        if (solver_)
            solver_->cancel();
    });
}

FdtdPanel::~FdtdPanel() = default;

void FdtdPanel::setControllers(AcousticController* acoustics, FdtdController* solver)
{
    acoustics_ = acoustics;
    solver_ = solver;
    connect(solver_, &FdtdController::settingsChanged, this, [this] {
        refreshSettings();
        checkTimer_.start();
    });
    connect(solver_, &FdtdController::runningChanged, this, [this] {
        updateEnabled();
        if (!solver_->isRunning() && ui_->statusLabel->text().startsWith(QStringLiteral("Running")))
            ui_->statusLabel->clear();
    });
    connect(solver_, &FdtdController::message, this, [this](const QString& text) { ui_->statusLabel->setText(text); });
    connect(acoustics_, &AcousticController::settingsChanged, this, [this] { checkTimer_.start(); });
    connect(&acoustics_->document(), &DocumentController::sceneChanged, this, [this](const SceneEvent& event) {
        if (event.kind != SceneEvent::Kind::Changed)
            refreshRegions();
        // The plan depends on the scene; recalculate once edits pause.
        checkTimer_.start();
    });
    connect(&acoustics_->document(), &DocumentController::projectReplaced, this, [this] {
        refreshRegions();
        refreshSettings();
        check();
    });
    refreshRegions();
    refreshSettings();
    check();
}

void FdtdPanel::refreshRegions()
{
    const Id current = selectedRegion();
    ui_->regionCombo->clear();
    for (const auto& [id, name] : acoustics_->objectsOfType(acoustics::object_types::Region))
        ui_->regionCombo->addItem(name, QString::fromStdString(id.str()));
    if (!current.isNull())
        selectRegion(current);
}

void FdtdPanel::selectRegion(const Id& id)
{
    const int index = ui_->regionCombo->findData(QString::fromStdString(id.str()));
    if (index >= 0)
        ui_->regionCombo->setCurrentIndex(index);
}

Id FdtdPanel::selectedRegion() const
{
    const QString stored = ui_->regionCombo->currentData().toString();
    return stored.isEmpty() ? Id() : Id(stored.toStdString());
}

void FdtdPanel::refreshSettings()
{
    if (!solver_)
        return;
    updating_ = true;
    const FdtdSettings s = solver_->settings();
    int waveform = 0;
    for (int i = 0; i < 5; ++i)
        if (kWaveforms[i] == s.waveform.kind)
            waveform = i;
    ui_->waveformCombo->setCurrentIndex(waveform);
    ui_->cyclesSpin->setValue(s.waveform.cycles);
    ui_->bandwidthSpin->setValue(s.waveform.fractionalBandwidth);
    ui_->signalFrequencySpin->setValue(s.waveform.frequencyHz);
    ui_->endFrequencySpin->setValue(s.waveform.endFrequencyHz);
    ui_->sweepDurationSpin->setValue(s.waveform.duration * 1e3);
    ui_->durationSpin->setValue(s.duration * 1e3);
    ui_->resolutionSpin->setValue(s.pointsPerWavelength);
    ui_->courantSpin->setValue(s.courantFraction);
    int boundary = 0;
    for (int i = 0; i < 4; ++i)
        if (kBoundaries[i] == s.boundary[0])
            boundary = i;
    ui_->boundaryCombo->setCurrentIndex(boundary);
    ui_->impedanceSpin->setValue(s.boundaryImpedance);
    ui_->absorbingSpin->setValue(s.absorbingCells);
    ui_->snapshotSpin->setValue(s.snapshotCount);
    ui_->periodsSpin->setValue(s.harmonicPeriods);
    ui_->computeCombo->setCurrentIndex(s.compute.backend == compute::ComputeBackend::Cuda ? 1 : 0);
    ui_->threadsSpin->setValue(s.compute.threads);
    updating_ = false;
    updateEnabled();
}

void FdtdPanel::updateEnabled()
{
    const WaveformKind kind = kWaveforms[ui_->waveformCombo->currentIndex()];
    ui_->cyclesSpin->setEnabled(kind == WaveformKind::ToneBurst);
    ui_->bandwidthSpin->setEnabled(kind == WaveformKind::GaussianPulse);
    // A Ricker wavelet and a sweep are defined by the signal itself; the
    // other kinds use each emitter's own frequency.
    ui_->signalFrequencySpin->setEnabled(kind == WaveformKind::Ricker || kind == WaveformKind::Chirp);
    ui_->endFrequencySpin->setEnabled(kind == WaveformKind::Chirp);
    ui_->sweepDurationSpin->setEnabled(kind == WaveformKind::Chirp);
    ui_->periodsSpin->setEnabled(kind == WaveformKind::Continuous);
    const BoundaryKind boundary = kBoundaries[ui_->boundaryCombo->currentIndex()];
    ui_->impedanceSpin->setEnabled(boundary == BoundaryKind::Impedance);
    ui_->absorbingSpin->setEnabled(boundary == BoundaryKind::Absorbing);
    ui_->threadsSpin->setEnabled(ui_->computeCombo->currentIndex() == 0);
    const bool running = solver_ && solver_->isRunning();
    ui_->runButton->setEnabled(!running);
    ui_->cancelButton->setEnabled(running);
}

void FdtdPanel::commit()
{
    if (updating_ || !solver_)
        return;
    FdtdSettings s = solver_->settings();
    s.waveform.kind = kWaveforms[ui_->waveformCombo->currentIndex()];
    s.waveform.cycles = ui_->cyclesSpin->value();
    s.waveform.fractionalBandwidth = ui_->bandwidthSpin->value();
    s.waveform.frequencyHz = ui_->signalFrequencySpin->value();
    s.waveform.endFrequencyHz = ui_->endFrequencySpin->value();
    s.waveform.duration = ui_->sweepDurationSpin->value() * 1e-3;
    s.duration = ui_->durationSpin->value() * 1e-3;
    s.pointsPerWavelength = ui_->resolutionSpin->value();
    s.courantFraction = ui_->courantSpin->value();
    s.boundary.fill(kBoundaries[ui_->boundaryCombo->currentIndex()]);
    s.boundaryImpedance = ui_->impedanceSpin->value();
    s.absorbingCells = ui_->absorbingSpin->value();
    s.snapshotCount = ui_->snapshotSpin->value();
    s.harmonicPeriods = ui_->periodsSpin->value();
    s.compute.backend = ui_->computeCombo->currentIndex() == 1 ? compute::ComputeBackend::Cuda : compute::ComputeBackend::Cpu;
    s.compute.threads = ui_->threadsSpin->value();
    if (const Status status = solver_->setSettings(s); !status) {
        ui_->statusLabel->setText(QString::fromStdString(status.message()));
        refreshSettings();
    }
    updateEnabled();
}

void FdtdPanel::check()
{
    if (!solver_)
        return;
    checkTimer_.stop();
    const Id region = selectedRegion();
    if (region.isNull()) {
        ui_->planText->setPlainText(QStringLiteral("Add a field plane or a field volume: it defines the computational domain."));
        ui_->assessmentText->clear();
        return;
    }
    const Prepared prepared = solver_->prepare(region);
    const FdtdPlan& plan = prepared.plan;
    QStringList lines;
    if (!plan.allowed)
        lines << QStringLiteral("CANNOT RUN: %1").arg(QString::fromStdString(plan.refusal)) << QString();
    if (plan.dx > 0.0)
        lines << QString::fromStdString(plan.summary());
    if (plan.allowed)
        lines << QStringLiteral("Sources: %1 simulated, %2 left out. Probes recorded: %3. Cells: %4 rigid, %5 pressure-release, %6 material.")
                     .arg(plan.sourceCount)
                     .arg(plan.excludedSources)
                     .arg(plan.probeCount)
                     .arg(plan.rigidCells)
                     .arg(plan.pressureReleaseCells)
                     .arg(plan.materialCells);
    if (!plan.warnings.empty()) {
        lines << QString() << QStringLiteral("WARNINGS");
        for (const std::string& warning : plan.warnings)
            lines << QStringLiteral("- ") + QString::fromStdString(warning);
    }
    if (!plan.objects.empty()) {
        lines << QString() << QStringLiteral("OBJECTS IN THE CALCULATION");
        for (const std::string& object : plan.objects)
            lines << QStringLiteral("- ") + QString::fromStdString(object);
    }
    if (!plan.assumptions.empty()) {
        lines << QString() << QStringLiteral("WHAT IS CALCULATED");
        for (const std::string& assumption : plan.assumptions)
            lines << QStringLiteral("- ") + QString::fromStdString(assumption);
    }
    ui_->planText->setPlainText(lines.join(QLatin1Char('\n')));
    ui_->assessmentText->setPlainText(solver_->assessment(region, plan));
}

bool FdtdPanel::run()
{
    if (!solver_)
        return false;
    check();
    const Id region = selectedRegion();
    if (region.isNull()) {
        ui_->statusLabel->setText(QStringLiteral("Add a field plane or a field volume first."));
        return false;
    }
    const auto started = solver_->run(region);
    if (!started) {
        ui_->statusLabel->setText(QStringLiteral("Not started: %1").arg(QString::fromStdString(started.message())));
        return false;
    }
    ui_->statusLabel->setText(QStringLiteral("Running. Progress is shown under Simulation Status."));
    updateEnabled();
    return true;
}

QString FdtdPanel::planText() const { return ui_->planText->toPlainText(); }
QString FdtdPanel::assessmentText() const { return ui_->assessmentText->toPlainText(); }
QString FdtdPanel::statusText() const { return ui_->statusLabel->text(); }
