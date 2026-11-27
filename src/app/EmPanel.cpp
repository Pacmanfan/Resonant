#include "EmPanel.h"

#include "EmController.h"
#include "PlotWidget.h"
#include "electromagnetics/Lines.h"
#include "ui_EmPanel.h"

#include <QFileDialog>
#include <QSignalBlocker>

#include <cmath>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr double kDeg = kPi / 180.0;
constexpr FieldComponent kComponents[] = {FieldComponent::EMagnitude, FieldComponent::Ex, FieldComponent::Ey, FieldComponent::Ez,
                                          FieldComponent::HMagnitude, FieldComponent::Hx, FieldComponent::Hy, FieldComponent::Hz,
                                          FieldComponent::PowerDensity};
constexpr dsp::WaveformKind kWaveforms[] = {dsp::WaveformKind::Continuous, dsp::WaveformKind::ToneBurst, dsp::WaveformKind::GaussianPulse};

QString complexText(const Complex& z, const char* unit)
{
    return QStringLiteral("%1 %2 j%3 %4").arg(z.real(), 0, 'f', 2).arg(z.imag() < 0 ? QLatin1Char('-') : QLatin1Char('+')).arg(std::abs(z.imag()), 0, 'f', 2)
        .arg(QLatin1String(unit));
}

QString dbi(double linear)
{
    return linear > 0.0 ? QStringLiteral("%1 dBi").arg(10.0 * std::log10(linear), 0, 'f', 2) : QStringLiteral("-");
}

QString lines(const std::vector<std::string>& items, const QString& prefix = QStringLiteral("- "))
{
    QString out;
    for (const std::string& s : items)
        out += prefix + QString::fromStdString(s) + QLatin1Char('\n');
    return out;
}

QString reportOf(const AntennaReport& r)
{
    QString t = QStringLiteral("Model: %1\nFrequency %2 MHz, wavelength %3 m.\n").arg(QString::fromStdString(r.model)).arg(r.frequencyHz / 1e6, 0, 'g', 7)
                    .arg(r.wavelength, 0, 'g', 5);
    if (r.impedance) {
        t += QStringLiteral("Input impedance: %1%2\n").arg(complexText(r.impedance->input, "ohm"),
                                                           r.impedance->valid ? QString() : QStringLiteral(" (NOT VALID: near anti-resonance)"));
        if (r.matching)
            t += QStringLiteral("Reflection |Gamma| %1, return loss %2 dB, VSWR %3, mismatch efficiency %4 %\n")
                     .arg(std::abs(r.matching->reflection), 0, 'f', 3).arg(r.matching->returnLossDb, 0, 'f', 2)
                     .arg(std::isfinite(r.matching->vswr) ? QString::number(r.matching->vswr, 'f', 2) : QStringLiteral("infinite"))
                     .arg(100.0 * r.matching->mismatchEfficiency, 0, 'f', 1);
    }
    if (r.directivity > 0.0) {
        t += QStringLiteral("Directivity %1 (%2), radiation efficiency %3 % (conductor loss)\n").arg(r.directivity, 0, 'f', 3).arg(dbi(r.directivity))
                 .arg(100.0 * r.radiationEfficiency, 0, 'f', 2);
        t += QStringLiteral("Gain %1 = directivity x efficiency; realized gain %2 = gain x mismatch efficiency\n").arg(dbi(r.gain), dbi(r.realizedGain));
        t += QStringLiteral("Far field beyond about %1 m\n").arg(r.farFieldDistance, 0, 'g', 4);
    }
    if (!r.polarization.empty())
        t += QStringLiteral("Polarization: %1\n").arg(QString::fromStdString(r.polarization));
    if (!r.estimates.empty())
        t += QStringLiteral("Design estimates (not calculated fields):\n") + lines(r.estimates);
    if (!r.notes.empty())
        t += QStringLiteral("Notes:\n") + lines(r.notes);
    return t;
}

QString reportOf(const GuideReport& g)
{
    if (!g.valid)
        return QStringLiteral("Not an analysable guide.\n") + lines(g.notes);
    QString t = QStringLiteral("Model: %1\nFilling er = %2, mur = %3; frequency of interest %4 GHz.\n").arg(QString::fromStdString(g.model))
                    .arg(g.relativePermittivity, 0, 'g', 4).arg(g.relativePermeability, 0, 'g', 4).arg(g.frequencyHz / 1e9, 0, 'g', 6);
    if (g.characteristicImpedance)
        t += QStringLiteral("Characteristic impedance (TEM): %1 ohm\n").arg(*g.characteristicImpedance, 0, 'f', 2);
    for (size_t i = 0; i < g.modes.size(); ++i) {
        const GuideMode& m = g.modes[i];
        const ModePropagation& p = g.propagation[i];
        t += QStringLiteral("%1: cut-off %2 GHz").arg(QString::fromStdString(m.label())).arg(m.cutoffHz / 1e9, 0, 'f', 4);
        if (p.propagating)
            t += QStringLiteral(", propagates: guide wavelength %1 mm, wave impedance %2 ohm, group velocity %3 c\n").arg(p.guideWavelength * 1e3, 0, 'f', 2)
                     .arg(p.waveImpedance, 0, 'f', 1).arg(p.groupVelocity / kSpeedOfLight, 0, 'f', 3);
        else
            t += QStringLiteral(", evanescent: decays %1 dB/m\n").arg(8.686 * p.attenuation, 0, 'g', 4);
    }
    for (const CavityMode& c : g.cavityModes)
        t += QStringLiteral("%1_%2%3%4 resonance %5 GHz\n").arg(c.kind == ModeKind::TE ? QStringLiteral("TE") : QStringLiteral("TM")).arg(c.m).arg(c.n)
                 .arg(c.p).arg(c.frequencyHz / 1e9, 0, 'f', 4);
    return t + lines(g.notes);
}

}  // namespace

EmPanel::EmPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::EmPanel>())
{
    ui_->setupUi(this);
}

EmPanel::~EmPanel() = default;

void EmPanel::setController(EmController* controller)
{
    controller_ = controller;
    connect(controller_, &EmController::settingsChanged, this, &EmPanel::refreshSettings);
    connect(controller_, &EmController::resultsChanged, this, &EmPanel::refreshResults);
    connect(controller_, &EmController::displayChanged, this, &EmPanel::refreshDisplay);
    connect(controller_, &EmController::runningChanged, this, &EmPanel::updateEnabled);
    connect(controller_, &EmController::measurementsChanged, this, &EmPanel::refreshMeasurements);
    connect(ui_->objectCombo, &QComboBox::activated, this, [this] { analyse(); });
    connect(ui_->analyseButton, &QPushButton::clicked, this, [this] { analyse(); });
    connect(ui_->sweepButton, &QPushButton::clicked, this, [this] { calculateSweep(); });
    connect(ui_->sweepViewCombo, &QComboBox::activated, this, &EmPanel::plotSweep);
    connect(ui_->measurementCombo, &QComboBox::activated, this, &EmPanel::plotSweep);
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export calculated one-port data"), QString(),
                                                          QStringLiteral("Touchstone (*.s1p)"));
        if (!path.isEmpty())
            if (const Status status = controller_->exportTouchstone(selectedObject(), path); !status)
                fail(QString::fromStdString(status.message()));
    });
    connect(ui_->importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import measured one-port data"), QString(),
                                                          QStringLiteral("Touchstone (*.s1p *.S1P)"));
        if (!path.isEmpty())
            if (const Status status = controller_->importTouchstone(path); !status)
                fail(QString::fromStdString(status.message()));
    });
    connect(ui_->patternButton, &QPushButton::clicked, this, [this] { calculatePattern(); });
    connect(ui_->cutCombo, &QComboBox::activated, this, [this] { calculatePattern(); });
    connect(ui_->dbCheck, &QCheckBox::clicked, this, [this] { calculatePattern(); });
    connect(ui_->show3DButton, &QPushButton::clicked, this, [this] { showPattern3D(); });
    connect(ui_->hide3DButton, &QPushButton::clicked, this, [this] { controller_->hidePattern3D(); });
    connect(ui_->steerButton, &QPushButton::clicked, this, [this] { steer(); });
    connect(ui_->chebyshevButton, &QPushButton::clicked, this, [this] { applyChebyshev(); });
    connect(ui_->analyseArrayButton, &QPushButton::clicked, this, [this] { analyseArray(); });
    connect(ui_->synthesizeButton, &QPushButton::clicked, this, [this] { synthesize(false); });
    connect(ui_->applySynthesisButton, &QPushButton::clicked, this, [this] { synthesize(true); });
    const auto addBeam = [this](bool null) {
        EmSettings s = controller_->settings();
        s.beams.push_back({ui_->steerThetaSpin->value(), ui_->steerPhiSpin->value(), 0.0, null});
        if (const Status status = controller_->setSettings(s); !status)
            fail(QString::fromStdString(status.message()));
    };
    connect(ui_->addBeamButton, &QPushButton::clicked, this, [addBeam] { addBeam(false); });
    connect(ui_->addNullButton, &QPushButton::clicked, this, [addBeam] { addBeam(true); });
    connect(ui_->removeBeamButton, &QPushButton::clicked, this, [this] {
        EmSettings s = controller_->settings();
        const int row = ui_->beamTable->currentRow();
        if (row >= 0 && row < static_cast<int>(s.beams.size())) {
            s.beams.erase(s.beams.begin() + row);
            if (const Status status = controller_->setSettings(s); !status)
                fail(QString::fromStdString(status.message()));
        }
    });
    connect(ui_->beamTable, &QTableWidget::itemChanged, this, [this] {
        if (!updating_)
            commitSettings();
    });
    connect(ui_->evaluateButton, &QPushButton::clicked, this, [this] { evaluateRegion(); });
    connect(ui_->guideFieldButton, &QPushButton::clicked, this, [this] { showGuideField(); });
    connect(ui_->probeButton, &QPushButton::clicked, this, [this] { probe(); });
    connect(ui_->planButton, &QPushButton::clicked, this, [this] { planFullWave(); });
    connect(ui_->regionCombo, &QComboBox::activated, this, [this] { planFullWave(); });
    connect(ui_->runFullWaveButton, &QPushButton::clicked, this, [this] { runFullWave(); });
    connect(ui_->lineButton, &QPushButton::clicked, this, [this] { calculateLine(); });
    connect(ui_->lineTypeCombo, &QComboBox::activated, this, [this](int index) {
        static const char* names[4][2] = {{"Inner conductor radius", "Outer conductor inner radius"},
                                          {"Strip width", "Substrate height"},
                                          {"Strip width", "Ground plane spacing"},
                                          {"Centre strip width", "Gap"}};
        ui_->lineText->setPlainText(QStringLiteral("Dimension 1: %1\nDimension 2: %2").arg(QLatin1String(names[index][0]), QLatin1String(names[index][1])));
    });
    for (QDoubleSpinBox* spin : {ui_->sweepMinSpin, ui_->sweepMaxSpin, ui_->resolutionSpin, ui_->steerThetaSpin, ui_->steerPhiSpin, ui_->chebyshevSpin,
                                 ui_->regularizationSpin, ui_->amplitudeLimitSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { commitSettings(); });
    connect(ui_->sweepPointsSpin, &QSpinBox::valueChanged, this, [this] { commitSettings(); });
    connect(ui_->ttdCheck, &QCheckBox::clicked, this, [this] { commitSettings(); });
    for (QDoubleSpinBox* spin : {ui_->cellsSpin, ui_->courantSpin, ui_->cyclesSpin, ui_->periodsSpin, ui_->durationSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { commitFullWave(); });
    for (QSpinBox* spin : {ui_->absorbingSpin, ui_->memorySpin, ui_->threadsSpin})
        connect(spin, &QSpinBox::valueChanged, this, [this] { commitFullWave(); });
    for (QComboBox* combo : {ui_->waveformCombo, ui_->computeCombo})
        connect(combo, &QComboBox::activated, this, [this] { commitFullWave(); });
    // Display.
    connect(ui_->resultCombo, &QComboBox::activated, this, [this](int index) { controller_->display(index - 1); });
    connect(ui_->componentCombo, &QComboBox::activated, this, [this](int index) { controller_->setComponentShown(kComponents[index]); });
    connect(ui_->quantityCombo, &QComboBox::activated, this,
            [this](int index) { controller_->setQuantity(static_cast<EmController::Quantity>(index)); });
    const auto scale = [this] { controller_->setNormalized(ui_->normalizedCheck->isChecked(), ui_->absoluteSpin->value()); };
    connect(ui_->normalizedCheck, &QCheckBox::clicked, this, scale);
    connect(ui_->absoluteSpin, &QDoubleSpinBox::valueChanged, this, scale);
    connect(ui_->animateCheck, &QCheckBox::clicked, this, [this](bool on) { controller_->setAnimating(on); });
    connect(ui_->removeResultButton, &QPushButton::clicked, this, [this] { controller_->removeResult(controller_->displayedIndex()); });
    connect(ui_->cancelButton, &QPushButton::clicked, this, [this] { controller_->cancel(); });
    ui_->normalizedCheck->setChecked(true);
    refreshSettings();
    refreshObjects();
    refreshRegions();
    refreshResults();
    refreshMeasurements();
    updateEnabled();
}

// --- selection and refresh ----------------------------------------------------------------

void EmPanel::refreshObjects()
{
    const Id selected = selectedObject();
    const QSignalBlocker block(ui_->objectCombo);
    ui_->objectCombo->clear();
    for (const auto& [id, name] : controller_->objects())
        ui_->objectCombo->addItem(name, QString::fromStdString(id.str()));
    const int index = ui_->objectCombo->findData(QString::fromStdString(selected.str()));
    if (index >= 0)
        ui_->objectCombo->setCurrentIndex(index);
}

void EmPanel::refreshRegions()
{
    const QString current = ui_->regionCombo->currentData().toString();
    const QSignalBlocker block(ui_->regionCombo);
    ui_->regionCombo->clear();
    for (const auto& [id, name] : controller_->regions())
        ui_->regionCombo->addItem(name, QString::fromStdString(id.str()));
    const int index = ui_->regionCombo->findData(current);
    if (index >= 0)
        ui_->regionCombo->setCurrentIndex(index);
}

void EmPanel::selectObject(const Id& id)
{
    refreshObjects();
    refreshRegions();
    const int index = ui_->objectCombo->findData(QString::fromStdString(id.str()));
    if (index >= 0)
        ui_->objectCombo->setCurrentIndex(index);
    analyse();
}

Id EmPanel::selectedObject() const
{
    const QString id = ui_->objectCombo->currentData().toString();
    return id.isEmpty() ? Id() : Id(id.toStdString());
}

void EmPanel::selectRegion(const Id& id)
{
    refreshRegions();
    const int index = ui_->regionCombo->findData(QString::fromStdString(id.str()));
    if (index >= 0)
        ui_->regionCombo->setCurrentIndex(index);
}

void EmPanel::showTab(const QString& objectName)
{
    if (QWidget* tab = ui_->tabs->findChild<QWidget*>(objectName))
        ui_->tabs->setCurrentWidget(tab);
}

void EmPanel::refreshSettings()
{
    updating_ = true;
    const EmSettings s = controller_->settings();
    const QSignalBlocker b1(ui_->sweepMinSpin), b2(ui_->sweepMaxSpin), b3(ui_->sweepPointsSpin), b4(ui_->resolutionSpin), b5(ui_->steerThetaSpin),
        b6(ui_->steerPhiSpin), b7(ui_->chebyshevSpin), b8(ui_->regularizationSpin), b9(ui_->amplitudeLimitSpin);
    ui_->sweepMinSpin->setValue(s.sweepMinHz / 1e6);
    ui_->sweepMaxSpin->setValue(s.sweepMaxHz / 1e6);
    ui_->sweepPointsSpin->setValue(s.sweepPoints);
    ui_->resolutionSpin->setValue(s.patternResolutionDeg);
    ui_->steerThetaSpin->setValue(s.steerThetaDeg);
    ui_->steerPhiSpin->setValue(s.steerPhiDeg);
    ui_->ttdCheck->setChecked(s.trueTimeDelay);
    ui_->chebyshevSpin->setValue(s.chebyshevSidelobeDb);
    ui_->regularizationSpin->setValue(s.regularization);
    ui_->amplitudeLimitSpin->setValue(s.amplitudeLimit);
    ui_->beamTable->setRowCount(static_cast<int>(s.beams.size()));
    for (int i = 0; i < static_cast<int>(s.beams.size()); ++i) {
        const EmSettings::Beam& b = s.beams[static_cast<size_t>(i)];
        ui_->beamTable->setItem(i, 0, new QTableWidgetItem(QString::number(b.thetaDeg)));
        ui_->beamTable->setItem(i, 1, new QTableWidgetItem(QString::number(b.phiDeg)));
        ui_->beamTable->setItem(i, 2, new QTableWidgetItem(b.null ? QStringLiteral("-") : QString::number(b.levelDb)));
        auto* null = new QTableWidgetItem(b.null ? QStringLiteral("null") : QStringLiteral("beam"));
        null->setFlags(null->flags() & ~Qt::ItemIsEditable);
        ui_->beamTable->setItem(i, 3, null);
    }
    const EmFullWaveSettings f = controller_->fullWaveSettings();
    const QSignalBlocker c1(ui_->cellsSpin), c2(ui_->courantSpin), c3(ui_->absorbingSpin), c4(ui_->cyclesSpin), c5(ui_->periodsSpin),
        c6(ui_->durationSpin), c7(ui_->memorySpin), c8(ui_->threadsSpin);
    ui_->cellsSpin->setValue(f.cellsPerWavelength);
    ui_->courantSpin->setValue(f.courantFraction);
    ui_->absorbingSpin->setValue(f.absorbingCells);
    int w = 0;
    for (int i = 0; i < 3; ++i)
        if (kWaveforms[i] == f.waveform.kind)
            w = i;
    ui_->waveformCombo->setCurrentIndex(w);
    ui_->cyclesSpin->setValue(f.waveform.cycles);
    ui_->periodsSpin->setValue(f.harmonicPeriods);
    ui_->durationSpin->setValue(f.duration * 1e9);
    ui_->memorySpin->setValue(static_cast<int>(f.memoryLimitBytes / (1024 * 1024)));
    ui_->computeCombo->setCurrentIndex(f.compute.backend == compute::ComputeBackend::Cuda ? 1 : 0);
    ui_->threadsSpin->setValue(f.compute.threads);
    ui_->threadsSpin->setEnabled(f.compute.backend == compute::ComputeBackend::Cpu);
    updating_ = false;
}

void EmPanel::commitSettings()
{
    if (updating_ || !controller_)
        return;
    EmSettings s = controller_->settings();
    s.sweepMinHz = ui_->sweepMinSpin->value() * 1e6;
    s.sweepMaxHz = ui_->sweepMaxSpin->value() * 1e6;
    s.sweepPoints = ui_->sweepPointsSpin->value();
    s.patternResolutionDeg = ui_->resolutionSpin->value();
    s.steerThetaDeg = ui_->steerThetaSpin->value();
    s.steerPhiDeg = ui_->steerPhiSpin->value();
    s.trueTimeDelay = ui_->ttdCheck->isChecked();
    s.chebyshevSidelobeDb = ui_->chebyshevSpin->value();
    s.regularization = ui_->regularizationSpin->value();
    s.amplitudeLimit = ui_->amplitudeLimitSpin->value();
    for (int i = 0; i < ui_->beamTable->rowCount() && i < static_cast<int>(s.beams.size()); ++i) {
        EmSettings::Beam& b = s.beams[static_cast<size_t>(i)];
        bool ok = false;
        if (const double v = ui_->beamTable->item(i, 0)->text().toDouble(&ok); ok)
            b.thetaDeg = v;
        if (const double v = ui_->beamTable->item(i, 1)->text().toDouble(&ok); ok)
            b.phiDeg = v;
        if (const double v = ui_->beamTable->item(i, 2)->text().toDouble(&ok); ok && !b.null)
            b.levelDb = v;
    }
    if (const Status status = controller_->setSettings(s); !status) {
        fail(QString::fromStdString(status.message()));
        refreshSettings();
    }
}

void EmPanel::commitFullWave()
{
    if (updating_ || !controller_)
        return;
    EmFullWaveSettings f = controller_->fullWaveSettings();
    f.cellsPerWavelength = ui_->cellsSpin->value();
    f.courantFraction = ui_->courantSpin->value();
    f.absorbingCells = ui_->absorbingSpin->value();
    f.waveform.kind = kWaveforms[std::clamp(ui_->waveformCombo->currentIndex(), 0, 2)];
    f.waveform.cycles = ui_->cyclesSpin->value();
    f.harmonicPeriods = ui_->periodsSpin->value();
    f.duration = ui_->durationSpin->value() * 1e-9;
    f.memoryLimitBytes = static_cast<size_t>(ui_->memorySpin->value()) * 1024 * 1024;
    f.compute.backend = ui_->computeCombo->currentIndex() == 1 ? compute::ComputeBackend::Cuda : compute::ComputeBackend::Cpu;
    f.compute.threads = ui_->threadsSpin->value();
    if (const Status status = controller_->setFullWaveSettings(f); !status) {
        fail(QString::fromStdString(status.message()));
        refreshSettings();
    }
}

void EmPanel::refreshResults()
{
    const QSignalBlocker block(ui_->resultCombo);
    ui_->resultCombo->clear();
    ui_->resultCombo->addItem(QStringLiteral("None (hide)"));
    for (const auto& r : controller_->results()) {
        const bool fullWave = r->metadata.solverId == kEmFullWaveSolverId;
        ui_->resultCombo->addItem(QStringLiteral("%1 [%2]").arg(QString::fromStdString(r->name),
                                                                fullWave ? QStringLiteral("full-wave") : QStringLiteral("analytical")));
    }
    ui_->resultCombo->setCurrentIndex(controller_->displayedIndex() + 1);
    refreshDisplay();
}

void EmPanel::refreshMeasurements()
{
    const QSignalBlocker block(ui_->measurementCombo);
    ui_->measurementCombo->clear();
    ui_->measurementCombo->addItem(QStringLiteral("None"));
    for (const auto& [name, measured] : controller_->measurements())
        ui_->measurementCombo->addItem(QStringLiteral("%1 (%2 points, Z0 %3 ohm)").arg(name).arg(measured.frequencyHz.size()).arg(measured.referenceImpedance));
    plotSweep();
}

void EmPanel::refreshDisplay()
{
    {
        const QSignalBlocker b1(ui_->resultCombo), b2(ui_->componentCombo), b3(ui_->quantityCombo);
        ui_->resultCombo->setCurrentIndex(controller_->displayedIndex() + 1);
        for (int i = 0; i < 9; ++i)
            if (kComponents[i] == controller_->componentShown())
                ui_->componentCombo->setCurrentIndex(i);
        const render::FieldQuantity q = controller_->fieldDisplay().quantity;
        ui_->quantityCombo->setCurrentIndex(q == render::FieldQuantity::Phase           ? 1
                                            : q == render::FieldQuantity::Instantaneous ? 2
                                            : q == render::FieldQuantity::Decibels      ? 3
                                                                                        : 0);
    }
    QString text = controller_->displayDescription();
    if (const int index = controller_->displayedIndex(); index >= 0) {
        const auto& r = controller_->results()[static_cast<size_t>(index)];
        if (!r->hasH)
            text += QStringLiteral("\nOnly the electric field is available for this result.");
        text += QStringLiteral("\n") + lines(r->assumptions);
    }
    ui_->displayText->setPlainText(text);
    updateEnabled();
}

void EmPanel::updateEnabled()
{
    const EmController::Kind kind = controller_->kindOf(selectedObject());
    const bool antenna = kind == EmController::Kind::Antenna, array = kind == EmController::Kind::Array;
    ui_->sweepButton->setEnabled(antenna);
    ui_->exportButton->setEnabled(antenna);
    ui_->patternButton->setEnabled(antenna || array);
    ui_->show3DButton->setEnabled(antenna || array);
    for (QPushButton* b : {ui_->steerButton, ui_->chebyshevButton, ui_->analyseArrayButton, ui_->synthesizeButton, ui_->applySynthesisButton})
        b->setEnabled(array);
    ui_->guideFieldButton->setEnabled(kind == EmController::Kind::Guide);
    const bool running = controller_->isRunning();
    ui_->runFullWaveButton->setEnabled(!running);
    ui_->cancelButton->setEnabled(running);
    ui_->statusLabel->setText(running ? QStringLiteral("Full-wave run in progress...") : QString());
    const bool shown = controller_->displayedIndex() >= 0;
    ui_->removeResultButton->setEnabled(shown);
    ui_->absoluteSpin->setEnabled(!ui_->normalizedCheck->isChecked());
}

void EmPanel::fail(const QString& text)
{
    ui_->statusLabel->setText(text);
    emit controller_->message(text);
}

// --- analysis -----------------------------------------------------------------------------

bool EmPanel::analyse()
{
    refreshObjects();
    refreshRegions();
    updateEnabled();
    const Id id = selectedObject();
    switch (controller_->kindOf(id)) {
    case EmController::Kind::Antenna: ui_->reportText->setPlainText(reportOf(controller_->antennaReport(id))); return true;
    case EmController::Kind::Guide: ui_->reportText->setPlainText(reportOf(controller_->guideReport(id))); return true;
    case EmController::Kind::Array: {
        auto a = controller_->analyzeArray(id);
        if (!a) {
            ui_->reportText->setPlainText(QString::fromStdString(a.message()));
            return false;
        }
        const PatternAnalysis& p = a.value().pattern;
        ui_->reportText->setPlainText(
            QStringLiteral("Model: array factor x element pattern, no mutual coupling (idealized).\nMain lobe toward theta = %1 deg, phi = %2 deg "
                           "(local); directivity %3 of the idealized model; half-power beamwidths %4 deg (theta) x %5 deg (phi); highest "
                           "sidelobe %6 dB (sphere sampled every %7 deg in theta, %8 deg in phi).\n")
                .arg(p.mainTheta / kDeg, 0, 'f', 1).arg(p.mainPhi / kDeg, 0, 'f', 1).arg(dbi(p.directivity)).arg(p.beamwidthTheta / kDeg, 0, 'f', 1)
                .arg(p.beamwidthPhi / kDeg, 0, 'f', 1).arg(p.sidelobeDb, 0, 'f', 1).arg(p.resolutionDeg).arg(2.0 * p.resolutionDeg)
            + a.value().warnings.join(QLatin1Char('\n')));
        return true;
    }
    case EmController::Kind::Source:
        ui_->reportText->setPlainText(QStringLiteral("An ideal source: its field is evaluated analytically on regions (Fields tab). Electric "
                                                     "dipoles are also represented by the full-wave solver; magnetic dipoles and plane "
                                                     "waves are not."));
        return true;
    case EmController::Kind::None: break;
    }
    ui_->reportText->setPlainText(QStringLiteral("Select an antenna, RF array, source or waveguide (Electromagnetics menu)."));
    return false;
}

bool EmPanel::calculateSweep()
{
    const Id id = selectedObject();
    const auto points = controller_->sweep(id);
    lastSweep_.clear();
    if (points.empty()) {
        fail(QStringLiteral("No impedance is calculated for this antenna kind (design estimates only)."));
        plotSweep();
        return false;
    }
    const double z0 = controller_->referenceImpedance(id);
    for (const auto& [f, z] : points)
        lastSweep_.push_back({f, z.input.real(), z.input.imag(), std::abs(match(z.input, z0).reflection)});
    plotSweep();
    return true;
}

void EmPanel::plotSweep()
{
    PlotWidget& plot = *ui_->sweepPlot;
    plot.clear();
    const int view = ui_->sweepViewCombo->currentIndex();
    const auto measured = controller_ ? controller_->measurements() : std::vector<std::pair<QString, OnePortData>>();
    const int m = ui_->measurementCombo->currentIndex() - 1;
    if (lastSweep_.empty() && (m < 0 || m >= static_cast<int>(measured.size())))
        return;
    const auto rl = [](double gamma) { return -20.0 * std::log10(std::max(gamma, 1e-12)); };
    const auto vswr = [](double gamma) { return gamma < 1.0 ? (1.0 + gamma) / (1.0 - gamma) : 1e3; };
    QVector<double> f, a, b;
    for (const SweepPoint& p : lastSweep_) {
        f.push_back(p.frequencyHz / 1e6);
        a.push_back(view == 0 ? p.resistance : view == 1 ? rl(p.reflection) : vswr(p.reflection));
        b.push_back(p.reactance);
    }
    const QString y = view == 0 ? QStringLiteral("ohm") : view == 1 ? QStringLiteral("return loss (dB)") : QStringLiteral("VSWR");
    if (!f.isEmpty()) {
        plot.setCurve(f, a, view == 0 ? QStringLiteral("Input impedance (analytical model)") : QStringLiteral("Calculated (analytical model)"),
                      QStringLiteral("MHz"), y);
        if (view == 0)
            plot.addCurve(f, b, QStringLiteral("Reactance X"), QColor(200, 80, 60));
    }
    if (m >= 0 && m < static_cast<int>(measured.size())) {
        const OnePortData& d = measured[static_cast<size_t>(m)].second;
        QVector<double> mf, ma, mb;
        const auto z = d.impedance();
        for (size_t i = 0; i < d.frequencyHz.size(); ++i) {
            mf.push_back(d.frequencyHz[i] / 1e6);
            const double g = std::abs(d.reflection[i]);
            ma.push_back(view == 0 ? z[i].real() : view == 1 ? rl(g) : vswr(g));
            mb.push_back(z[i].imag());
        }
        if (f.isEmpty())
            plot.setCurve(mf, ma, QStringLiteral("Measured: ") + measured[static_cast<size_t>(m)].first, QStringLiteral("MHz"), y);
        else
            plot.addCurve(mf, ma, QStringLiteral("Measured: ") + measured[static_cast<size_t>(m)].first, QColor(40, 160, 90), true);
        if (view == 0)
            plot.addCurve(mf, mb, QStringLiteral("Measured X"), QColor(160, 120, 40), true);
    }
}

bool EmPanel::calculatePattern()
{
    const Id id = selectedObject();
    auto grid = controller_->pattern(id);
    if (!grid) {
        ui_->patternText->setPlainText(QString::fromStdString(grid.message()));
        ui_->patternPlot->clear();
        return false;
    }
    const PatternGrid& p = grid.value();
    const int cut = ui_->cutCombo->currentIndex();
    const bool db = ui_->dbCheck->isChecked();
    QVector<double> angle, value;
    const auto push = [&](double degrees, double v) {
        angle.push_back(degrees);
        value.push_back(db ? std::max(-60.0, 20.0 * std::log10(std::max(v, 1e-12))) : v);
    };
    if (cut == 2) {
        const int i = (p.thetaCount - 1) / 2;  // theta = 90 deg
        for (int j = 0; j < p.phiCount; ++j)
            push(j * p.stepRad / kDeg, p.value(i, j));
    } else {
        // theta from 0 to 360 through the plane phi = phi0 (back half at phi0 + 180).
        const int j0 = cut == 0 ? 0 : static_cast<int>(std::lround(0.5 * kPi / p.stepRad)) % p.phiCount;
        const int j1 = (j0 + p.phiCount / 2) % p.phiCount;
        for (int i = 0; i < p.thetaCount; ++i)
            push(i * p.stepRad / kDeg, p.value(i, j0));
        for (int i = p.thetaCount - 2; i >= 0; --i)
            push(360.0 - i * p.stepRad / kDeg, p.value(i, j1));
    }
    ui_->patternPlot->setCurve(angle, value, QStringLiteral("Normalized far-field pattern |E| / max"), cut == 2 ? QStringLiteral("phi (deg)")
                                                                                                                  : QStringLiteral("theta (deg)"),
                               db ? QStringLiteral("dB re maximum") : QStringLiteral("relative |E|"));
    ui_->patternText->setPlainText(QStringLiteral("Model: %1\nDirectivity from the sampled pattern: %2 (%3), angular step %4 deg. The pattern "
                                                  "is normalized (relative), not an absolute field strength.")
                                       .arg(QString::fromStdString(p.model)).arg(p.directivity, 0, 'f', 3).arg(dbi(p.directivity))
                                       .arg(p.stepRad / kDeg, 0, 'g', 3));
    return true;
}

bool EmPanel::showPattern3D()
{
    if (const Status status = controller_->showPattern3D(selectedObject()); !status) {
        fail(QString::fromStdString(status.message()));
        return false;
    }
    return true;
}

bool EmPanel::steer()
{
    if (const Status status = controller_->steerArray(selectedObject()); !status) {
        fail(QString::fromStdString(status.message()));
        return false;
    }
    return analyseArray();
}

bool EmPanel::applyChebyshev()
{
    if (const Status status = controller_->applyChebyshev(selectedObject()); !status) {
        fail(QString::fromStdString(status.message()));
        return false;
    }
    return analyseArray();
}

bool EmPanel::analyseArray()
{
    const Id id = selectedObject();
    auto a = controller_->analyzeArray(id);
    if (!a) {
        ui_->arrayText->setPlainText(QString::fromStdString(a.message()));
        return false;
    }
    analyse();
    const PatternAnalysis& p = a.value().pattern;
    ui_->arrayText->setPlainText(QStringLiteral("Array factor x element pattern (no mutual coupling).\nMain lobe: theta %1 deg, phi %2 deg. "
                                                "Directivity %3. Beamwidths %4 x %5 deg. Highest sidelobe %6 dB.\n%7")
                                     .arg(p.mainTheta / kDeg, 0, 'f', 1).arg(p.mainPhi / kDeg, 0, 'f', 1).arg(dbi(p.directivity))
                                     .arg(p.beamwidthTheta / kDeg, 0, 'f', 1).arg(p.beamwidthPhi / kDeg, 0, 'f', 1).arg(p.sidelobeDb, 0, 'f', 1)
                                     .arg(a.value().warnings.join(QLatin1Char('\n'))));
    // A cut through the main lobe along theta (phi fixed).
    auto grid = controller_->pattern(id);
    if (grid) {
        const PatternGrid& g = grid.value();
        const int j = static_cast<int>(std::lround(p.mainPhi / g.stepRad + g.phiCount)) % g.phiCount;
        QVector<double> t, v;
        for (int i = 0; i < g.thetaCount; ++i) {
            t.push_back(i * g.stepRad / kDeg);
            v.push_back(std::max(-60.0, 20.0 * std::log10(std::max(g.value(i, j), 1e-12))));
        }
        ui_->arrayPlot->setCurve(t, v, QStringLiteral("Cut through the main lobe (phi = %1 deg)").arg(p.mainPhi / kDeg, 0, 'f', 1),
                                 QStringLiteral("theta (deg)"), QStringLiteral("dB re maximum"));
    }
    return true;
}

bool EmPanel::synthesize(bool apply)
{
    auto result = controller_->synthesize(selectedObject(), apply);
    if (!result) {
        ui_->arrayText->setPlainText(QString::fromStdString(result.message()));
        return false;
    }
    const BeamSynthesis& b = result.value();
    QString text = QStringLiteral("Least-squares synthesis (Tikhonov regularization), array-factor model.\nRelative residual %1, largest weight "
                                  "%2%3.\n")
                       .arg(b.relativeResidual, 0, 'g', 3).arg(b.largestWeight, 0, 'g', 4)
                       .arg(b.amplitudeLimited ? QStringLiteral(" (amplitude limit active)") : QString());
    const EmSettings s = controller_->settings();
    for (size_t i = 0; i < s.beams.size() && i < b.achieved.size(); ++i)
        text += QStringLiteral("%1 at (%2, %3) deg: achieved %4 dB\n").arg(s.beams[i].null ? QStringLiteral("Null") : QStringLiteral("Beam"))
                    .arg(s.beams[i].thetaDeg).arg(s.beams[i].phiDeg).arg(20.0 * std::log10(std::max(std::abs(b.achieved[i]), 1e-12)), 0, 'f', 1);
    text += QString::fromStdString(b.notes);
    if (apply)
        analyseArray();
    ui_->arrayText->setPlainText(text + (apply ? QStringLiteral("\nApplied to the array's elements (undoable).") : QString()));
    return true;
}

// --- fields -------------------------------------------------------------------------------

bool EmPanel::evaluateRegion()
{
    refreshRegions();
    const QString id = ui_->regionCombo->currentData().toString();
    if (const Status status = controller_->evaluateRegion(Id(id.toStdString())); !status) {
        fail(QString::fromStdString(status.message()));
        return false;
    }
    return true;
}

bool EmPanel::showGuideField()
{
    if (const Status status = controller_->showGuideField(selectedObject()); !status) {
        fail(QString::fromStdString(status.message()));
        return false;
    }
    return true;
}

bool EmPanel::probe()
{
    const Vec3 p(ui_->probeXSpin->value(), ui_->probeYSpin->value(), ui_->probeZSpin->value());
    auto result = controller_->probe(p);
    if (!result) {
        ui_->probeText->setPlainText(QString::fromStdString(result.message()));
        return false;
    }
    const EmFieldResult& r = *result.value();
    if (!r.excluded.empty() && r.excluded[0]) {
        ui_->probeText->setPlainText(QStringLiteral("The point lies on a source or behind a ground plane: no field value."));
        return false;
    }
    const auto c = [&](const std::array<std::vector<std::complex<float>>, 3>& v, int i) { return Complex(v[static_cast<size_t>(i)][0]); };
    QString text = QStringLiteral("Analytical field at (%1, %2, %3) m, %4 MHz, peak phasors:\n").arg(p.x()).arg(p.y()).arg(p.z()).arg(r.frequencyHz / 1e6);
    const char* axes = "xyz";
    for (int i = 0; i < 3; ++i)
        text += QStringLiteral("E%1 = %2 V/m (%3 deg)   H%1 = %4 A/m (%5 deg)\n").arg(QLatin1Char(axes[i])).arg(std::abs(c(r.e, i)), 0, 'g', 4)
                    .arg(std::arg(c(r.e, i)) / kDeg, 0, 'f', 1).arg(std::abs(c(r.h, i)), 0, 'g', 4).arg(std::arg(c(r.h, i)) / kDeg, 0, 'f', 1);
    text += QStringLiteral("|E| = %1 V/m, |H| = %2 A/m, power density %3 W/m^2").arg(std::abs(r.component(FieldComponent::EMagnitude, 0)), 0, 'g', 4)
                .arg(std::abs(r.component(FieldComponent::HMagnitude, 0)), 0, 'g', 4).arg(std::abs(r.component(FieldComponent::PowerDensity, 0)), 0, 'g', 4);
    ui_->probeText->setPlainText(text);
    return true;
}

bool EmPanel::planFullWave()
{
    refreshRegions();
    const QString id = ui_->regionCombo->currentData().toString();
    const EmPrepared p = controller_->planFullWave(Id(id.toStdString()));
    QString text = p.plan.allowed ? QString::fromStdString(p.plan.summary()) : QStringLiteral("REFUSED: ") + QString::fromStdString(p.plan.refusal);
    if (!p.plan.objects.empty())
        text += QStringLiteral("\nRepresented:\n") + lines(p.plan.objects);
    if (!p.plan.warnings.empty())
        text += QStringLiteral("Warnings:\n") + lines(p.plan.warnings);
    ui_->planText->setPlainText(text);
    return p.plan.allowed;
}

bool EmPanel::runFullWave()
{
    if (!planFullWave())
        return false;
    const QString id = ui_->regionCombo->currentData().toString();
    auto started = controller_->runFullWave(Id(id.toStdString()));
    if (!started) {
        fail(QString::fromStdString(started.message()));
        return false;
    }
    updateEnabled();
    return true;
}

bool EmPanel::calculateLine()
{
    const double a = ui_->lineASpin->value() * 1e-3, b = ui_->lineBSpin->value() * 1e-3, er = ui_->lineErSpin->value();
    const double f = ui_->lineFrequencySpin->value() * 1e6;
    LineParameters line;
    switch (ui_->lineTypeCombo->currentIndex()) {
    case 0:
        if (a >= b) {
            ui_->lineText->setPlainText(QStringLiteral("The inner radius (dimension 1) must be smaller than the outer radius (dimension 2)."));
            return false;
        }
        line = coaxialLine(a, b, er);
        break;
    case 1: line = microstripLine(a, b, er); break;
    case 2: line = striplineLine(a, b, er); break;
    default: line = coplanarLine(a, b, er); break;
    }
    const double guided = line.phaseVelocity / f;
    ui_->lineText->setPlainText(QStringLiteral("Z0 = %1 ohm, effective permittivity %2, phase velocity %3 c, wavelength on the line %4 mm at %5 "
                                               "MHz (quarter wave %6 mm).\nModel: %7")
                                    .arg(line.characteristicImpedance, 0, 'f', 2).arg(line.effectivePermittivity, 0, 'f', 4)
                                    .arg(line.phaseVelocity / kSpeedOfLight, 0, 'f', 4).arg(guided * 1e3, 0, 'f', 2).arg(f / 1e6, 0, 'g', 6)
                                    .arg(0.25 * guided * 1e3, 0, 'f', 2).arg(QString::fromStdString(line.model)));
    return true;
}

// --- accessors ----------------------------------------------------------------------------

QString EmPanel::reportText() const { return ui_->reportText->toPlainText(); }
QString EmPanel::patternText() const { return ui_->patternText->toPlainText(); }
QString EmPanel::arrayText() const { return ui_->arrayText->toPlainText(); }
QString EmPanel::planText() const { return ui_->planText->toPlainText(); }
QString EmPanel::probeText() const { return ui_->probeText->toPlainText(); }
QString EmPanel::lineText() const { return ui_->lineText->toPlainText(); }
QString EmPanel::displayText() const { return ui_->displayText->toPlainText(); }
PlotWidget& EmPanel::sweepPlot() { return *ui_->sweepPlot; }
PlotWidget& EmPanel::patternPlot() { return *ui_->patternPlot; }
PlotWidget& EmPanel::arrayPlot() { return *ui_->arrayPlot; }
