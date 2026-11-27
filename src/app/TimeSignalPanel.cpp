#include "TimeSignalPanel.h"

#include "FdtdController.h"
#include "ui_TimeSignalPanel.h"

#include <QFileDialog>

#include <cmath>

using namespace resonant;
using namespace resonant::fdtd;

TimeSignalPanel::TimeSignalPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::TimeSignalPanel>())
{
    ui_->setupUi(this);
    ui_->splitter->setStretchFactor(0, 3);
    ui_->splitter->setStretchFactor(1, 2);
    connect(ui_->resultCombo, &QComboBox::activated, this, [this] {
        refreshProbes();
        refreshPlot();
    });
    connect(ui_->probeCombo, &QComboBox::activated, this, [this] { refreshPlot(); });
    connect(ui_->viewCombo, &QComboBox::currentIndexChanged, this, [this] { refreshPlot(); });
    connect(ui_->windowCombo, &QComboBox::activated, this, [this] { refreshPlot(); });
    connect(ui_->snapshotSlider, &QSlider::valueChanged, this, [this](int value) { showSnapshot(value); });
    connect(ui_->steadyButton, &QPushButton::clicked, this, [this] {
        if (controller_ && ui_->resultCombo->currentIndex() >= 0)
            controller_->showSteadyState(static_cast<size_t>(ui_->resultCombo->currentIndex()));
    });
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        if (!controller_ || ui_->resultCombo->currentIndex() < 0)
            return;
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Probe Signals"), QString(), QStringLiteral("CSV (*.csv)"));
        if (path.isEmpty())
            return;
        if (const Status status = controller_->exportProbes(static_cast<size_t>(ui_->resultCombo->currentIndex()), path); !status)
            ui_->infoText->setPlainText(QString::fromStdString(status.message()));
    });
    refreshPlot();
}

TimeSignalPanel::~TimeSignalPanel() = default;

void TimeSignalPanel::setController(FdtdController* controller)
{
    controller_ = controller;
    connect(controller_, &FdtdController::resultsChanged, this, &TimeSignalPanel::refresh);
    refresh();
}

void TimeSignalPanel::refresh()
{
    ui_->resultCombo->clear();
    if (controller_)
        for (const auto& result : controller_->results())
            ui_->resultCombo->addItem(QString::fromStdString(result->name));
    ui_->resultCombo->setCurrentIndex(ui_->resultCombo->count() - 1);  // the newest
    refreshProbes();
    refreshPlot();
}

void TimeSignalPanel::refreshProbes()
{
    ui_->probeCombo->clear();
    const int index = ui_->resultCombo->currentIndex();
    const bool valid = controller_ && index >= 0;
    if (valid)
        for (const ProbeTrace& probe : controller_->results()[static_cast<size_t>(index)]->output.probes)
            ui_->probeCombo->addItem(QString::fromStdString(probe.label));
    const int snapshots = valid ? static_cast<int>(controller_->results()[static_cast<size_t>(index)]->output.snapshots.size()) : 0;
    const QSignalBlocker blocker(ui_->snapshotSlider);
    ui_->snapshotSlider->setEnabled(snapshots > 0);
    ui_->snapshotSlider->setRange(0, std::max(0, snapshots - 1));
    ui_->snapshotSlider->setValue(snapshots / 2);
    ui_->snapshotLabel->setText(snapshots > 0 ? QStringLiteral("%1 instants recorded").arg(snapshots) : QStringLiteral("none recorded"));
    ui_->steadyButton->setEnabled(valid && controller_->results()[static_cast<size_t>(index)]->harmonicField != nullptr);
}

bool TimeSignalPanel::showSnapshot(int index)
{
    const int result = ui_->resultCombo->currentIndex();
    if (!controller_ || result < 0 || index < 0)
        return false;
    if (!controller_->showSnapshot(static_cast<size_t>(result), static_cast<size_t>(index)))
        return false;
    const FdtdResult& r = *controller_->results()[static_cast<size_t>(result)];
    ui_->snapshotLabel->setText(QStringLiteral("t = %1 ms (instant %2 of %3)")
                                    .arg(r.output.snapshotTimes[static_cast<size_t>(index)] * 1e3, 0, 'g', 5)
                                    .arg(index + 1)
                                    .arg(r.output.snapshots.size()));
    return true;
}

void TimeSignalPanel::refreshPlot()
{
    const int resultIndex = ui_->resultCombo->currentIndex();
    if (!controller_ || resultIndex < 0) {
        ui_->plot->clear();
        ui_->infoText->setPlainText(QStringLiteral("Run a full-wave simulation to record signals. Probes and target points inside the "
                                                   "region are recorded at every time step."));
        return;
    }
    const FdtdResult& result = *controller_->results()[static_cast<size_t>(resultIndex)];
    const View view = static_cast<View>(ui_->viewCombo->currentIndex());
    QStringList info;
    info << QStringLiteral("%1: %2D, %3 steps of %4 us, %5 ms simulated in %6 s (%7 million cell updates per second).")
                .arg(QString::fromStdString(result.name))
                .arg(result.plan.dimensions)
                .arg(result.output.steps)
                .arg(result.output.dt * 1e6, 0, 'g', 4)
                .arg(result.output.steps * result.output.dt * 1e3, 0, 'g', 4)
                .arg(result.output.elapsedSeconds, 0, 'f', 2)
                .arg(result.output.cellUpdatesPerSecond / 1e6, 0, 'f', 1);

    if (view == EnergyOverTime) {
        QVector<double> x, y;
        for (size_t i = 0; i < result.output.energy.size(); ++i) {
            x.push_back(result.output.energyTimes[i] * 1e3);
            y.push_back(result.output.energy[i]);
        }
        ui_->plot->setCurve(x, y, QStringLiteral("Acoustic energy in the domain"), QStringLiteral("time (ms)"),
                            result.plan.dimensions == 3 ? QStringLiteral("energy (J)") : QStringLiteral("energy (J/m)"));
        info << QStringLiteral("Energy at the end: %1 % of the largest value during the run. In a closed lossless domain it stays "
                               "constant once the sources have ended; with absorbing layers it falls as sound leaves; growth would mean "
                               "instability.")
                    .arg(100.0 * result.finalEnergyFraction(), 0, 'g', 4);
        ui_->infoText->setPlainText(info.join(QStringLiteral("\n\n")));
        return;
    }

    const int probeIndex = ui_->probeCombo->currentIndex();
    if (probeIndex < 0 || static_cast<size_t>(probeIndex) >= result.output.probes.size()) {
        ui_->plot->clear();
        info << QStringLiteral("This run recorded no probe: add probes or target points inside the region and run again.");
        ui_->infoText->setPlainText(info.join(QStringLiteral("\n\n")));
        return;
    }
    const ProbeTrace& probe = result.output.probes[static_cast<size_t>(probeIndex)];
    const QString where = QStringLiteral("%1 at (%2, %3, %4) m")
                              .arg(QString::fromStdString(probe.label))
                              .arg(probe.position.x(), 0, 'g', 5)
                              .arg(probe.position.y(), 0, 'g', 5)
                              .arg(probe.position.z(), 0, 'g', 5);
    if (view == PressureOverTime) {
        QVector<double> x, y;
        double peak = 0.0;
        for (size_t n = 0; n < probe.pressure.size(); ++n) {
            x.push_back(static_cast<double>(n + 1) * result.output.dt * 1e3);
            y.push_back(probe.pressure[n]);
            peak = std::max(peak, std::abs(probe.pressure[n]));
        }
        ui_->plot->setCurve(x, y, where, QStringLiteral("time (ms)"), QStringLiteral("pressure (Pa)"));
        info << QStringLiteral("Instantaneous pressure at the centre of the cell that contains the probe. Largest magnitude %1 Pa.").arg(peak, 0, 'g', 6);
        info << QStringLiteral("Signal: %1.").arg(QString::fromStdString(result.settings.waveform.describe()));
    } else {
        const Window window = ui_->windowCombo->currentIndex() == 1 ? Window::Rectangular : Window::Hann;
        const Spectrum spectrum = result.probeSpectrum(static_cast<size_t>(probeIndex), window);
        // Up to the highest frequency the grid resolves; above it the values are numerical.
        const double limit = 1.5 * result.plan.highestFrequency;
        QVector<double> x, y;
        for (size_t k = 0; k < spectrum.frequencyHz.size() && spectrum.frequencyHz[k] <= limit; ++k) {
            x.push_back(spectrum.frequencyHz[k] / 1e3);
            y.push_back(view == SpectrumMagnitude ? std::abs(spectrum.amplitude[k]) : std::arg(spectrum.amplitude[k]) * 180.0 / 3.14159265358979323846);
        }
        ui_->plot->setCurve(x, y, where, QStringLiteral("frequency (kHz)"),
                            view == SpectrumMagnitude ? QStringLiteral("amplitude (Pa)") : QStringLiteral("phase (deg)"));
        info << QString::fromStdString(spectrum.describe()) + QStringLiteral(".");
        info << QStringLiteral("Shown up to %1 kHz. The grid was resolved for %2 kHz; content above that is not physical.")
                    .arg(limit / 1e3, 0, 'g', 4)
                    .arg(result.plan.highestFrequency / 1e3, 0, 'g', 4);
        if (view == SpectrumMagnitude) {
            const std::vector<SpectralPeak> peaks = findPeaks(spectrum, 0.05, 0.0, limit);
            QVector<double> markers;
            QStringList list;
            for (size_t i = 0; i < peaks.size() && i < 12; ++i) {
                markers.push_back(peaks[i].frequencyHz / 1e3);
                list << QStringLiteral("%1 Hz").arg(peaks[i].frequencyHz, 0, 'f', 0);
            }
            ui_->plot->setMarkers(markers);
            info << QStringLiteral("Response peaks (strongest first): %1. Each is uncertain by about half the resolution. They are peaks "
                                   "of the response at this probe for this excitation and indicate resonances; they are NOT calculated "
                                   "eigenmodes, and a mode with a node at the source or at the probe does not appear.")
                        .arg(list.isEmpty() ? QStringLiteral("none") : list.join(QStringLiteral(", ")));
            info << QStringLiteral("Amplitude scaling: a steady sinusoid reads its amplitude; for a transient the values depend on the "
                                   "record length and only ratios are meaningful.");
        }
    }
    ui_->infoText->setPlainText(info.join(QStringLiteral("\n\n")));
}

void TimeSignalPanel::setView(View view) { ui_->viewCombo->setCurrentIndex(view); }
void TimeSignalPanel::selectProbe(int index)
{
    ui_->probeCombo->setCurrentIndex(index);
    refreshPlot();
}
PlotWidget& TimeSignalPanel::plot() { return *ui_->plot; }
QString TimeSignalPanel::infoText() const { return ui_->infoText->toPlainText(); }
int TimeSignalPanel::resultCount() const { return ui_->resultCombo->count(); }
int TimeSignalPanel::probeCount() const { return ui_->probeCombo->count(); }
int TimeSignalPanel::snapshotCount() const { return ui_->snapshotSlider->isEnabled() ? ui_->snapshotSlider->maximum() + 1 : 0; }
