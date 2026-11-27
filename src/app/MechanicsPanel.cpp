#include "MechanicsPanel.h"

#include "AcousticController.h"
#include "DocumentController.h"
#include "MechanicsController.h"
#include "PlotWidget.h"
#include "acoustics/Components.h"
#include "ui_MechanicsPanel.h"

#include <QFileDialog>
#include <QStandardItemModel>

#include <cmath>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr DampingKind kDamping[] = {DampingKind::None, DampingKind::Rayleigh, DampingKind::LossFactor, DampingKind::ModalRatio};

}  // namespace

MechanicsPanel::MechanicsPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::MechanicsPanel>())
{
    ui_->setupUi(this);
    checkTimer_.setSingleShot(true);
    checkTimer_.setInterval(300);
    connect(&checkTimer_, &QTimer::timeout, this, &MechanicsPanel::check);
    for (QDoubleSpinBox* s : {ui_->fMinSpin, ui_->fMaxSpin, ui_->alphaSpin, ui_->betaSpin, ui_->etaSpin, ui_->zetaSpin, ui_->durationSpin,
                              ui_->stepsPerPeriodSpin, ui_->timeStepSpin, ui_->rayleighAlphaSpin, ui_->rayleighBetaSpin, ui_->targetFrequencySpin,
                              ui_->regularizationSpin})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { commit(); });
    for (QSpinBox* s : {ui_->modeCountSpin, ui_->pointsSpin, ui_->harmonicModesSpin, ui_->snapshotsSpin})
        connect(s, &QSpinBox::valueChanged, this, [this] { commit(); });
    for (QComboBox* c : {ui_->methodCombo, ui_->dampingCombo})
        connect(c, &QComboBox::activated, this, [this] { commit(); });
    for (QCheckBox* c : {ui_->logCheck, ui_->materialLossCheck})
        connect(c, &QCheckBox::clicked, this, [this] { commit(); });
    connect(ui_->structureCombo, &QComboBox::activated, this, [this] { check(); });
    connect(ui_->checkButton, &QPushButton::clicked, this, &MechanicsPanel::check);
    connect(ui_->runModalButton, &QPushButton::clicked, this, [this] { runModal(); });
    connect(ui_->runHarmonicButton, &QPushButton::clicked, this, [this] { runHarmonic(); });
    connect(ui_->runTransientButton, &QPushButton::clicked, this, [this] { runTransient(); });
    connect(ui_->runStaticButton, &QPushButton::clicked, this, [this] { runStatic(); });
    connect(ui_->solveTargetsButton, &QPushButton::clicked, this, [this] { solveTargets(); });
    connect(ui_->applyTargetsButton, &QPushButton::clicked, this, [this] { applyTargets(); });
    connect(ui_->bandButton, &QPushButton::clicked, this, [this] { showBandStructure(); });
    connect(ui_->radiateButton, &QPushButton::clicked, this, [this] { radiate(); });
    connect(ui_->cancelButton, &QPushButton::clicked, this, [this] {
        if (mechanics_)
            mechanics_->cancel();
    });
    connect(ui_->importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Measured Response"), QString(), QStringLiteral("CSV (*.csv)"));
        if (path.isEmpty() || !mechanics_)
            return;
        if (const Status status = mechanics_->importMeasurement(path); !status)
            ui_->statusLabel->setText(QString::fromStdString(status.message()));
    });
    for (QComboBox* c : {ui_->responseCombo, ui_->harmonicQuantityCombo, ui_->harmonicViewCombo, ui_->transientSensorCombo, ui_->transientQuantityCombo})
        connect(c, &QComboBox::activated, this, [this] { refreshPlots(); });
    connect(ui_->resultCombo, &QComboBox::activated, this, [this](int index) {
        mechanics_->display(index, 0);
        refreshItems();
        refreshPlots();
    });
    connect(ui_->itemCombo, &QComboBox::activated, this, [this](int item) { mechanics_->display(mechanics_->displayedIndex(), item); });
    connect(ui_->quantityCombo, &QComboBox::activated, this,
            [this](int index) { mechanics_->setQuantity(static_cast<MechanicsController::Quantity>(index)); });
    connect(ui_->autoScaleCheck, &QCheckBox::clicked, this, [this](bool on) {
        ui_->scaleSpin->setEnabled(!on);
        mechanics_->setScale(on, ui_->scaleSpin->value());
    });
    connect(ui_->scaleSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!updating_ && !ui_->autoScaleCheck->isChecked())
            mechanics_->setScale(false, value);
    });
    connect(ui_->animateCheck, &QCheckBox::clicked, this, [this](bool on) { mechanics_->setAnimating(on); });
    connect(ui_->removeResultButton, &QPushButton::clicked, this, [this] { mechanics_->removeResult(ui_->resultCombo->currentIndex()); });
}

MechanicsPanel::~MechanicsPanel() = default;

void MechanicsPanel::setControllers(MechanicsController* mechanics, AcousticController* acoustics)
{
    mechanics_ = mechanics;
    acoustics_ = acoustics;
    connect(mechanics_, &MechanicsController::settingsChanged, this, [this] {
        refreshSettings();
        checkTimer_.start();
    });
    connect(mechanics_, &MechanicsController::resultsChanged, this, &MechanicsPanel::refreshResults);
    connect(mechanics_, &MechanicsController::runningChanged, this, [this] {
        updateEnabled();
        if (!mechanics_->isRunning() && ui_->statusLabel->text().startsWith(QStringLiteral("Running")))
            ui_->statusLabel->clear();
    });
    connect(mechanics_, &MechanicsController::message, this, [this](const QString& text) { ui_->statusLabel->setText(text); });
    connect(mechanics_, &MechanicsController::displayChanged, this, [this] {
        ui_->displayText->setPlainText(mechanics_->displayDescription());
        const QSignalBlocker blocker(ui_->scaleSpin);
        if (ui_->autoScaleCheck->isChecked())
            ui_->scaleSpin->setValue(mechanics_->scaleFactor());
        if (ui_->itemCombo->currentIndex() != mechanics_->displayedItem() && mechanics_->displayedItem() < ui_->itemCombo->count())
            ui_->itemCombo->setCurrentIndex(mechanics_->displayedItem());
        if (ui_->resultCombo->currentIndex() != mechanics_->displayedIndex())
            ui_->resultCombo->setCurrentIndex(mechanics_->displayedIndex());
        ui_->quantityCombo->setCurrentIndex(static_cast<int>(mechanics_->quantity()));
    });
    DocumentController& document = acoustics_->document();
    connect(&document, &DocumentController::sceneChanged, this, [this](const SceneEvent& event) {
        // Any edit can make an object structural or change a name.
        refreshStructures();
        if (event.kind != SceneEvent::Kind::Changed)
            refreshRegions();
        checkTimer_.start();
    });
    connect(&document, &DocumentController::projectReplaced, this, [this] {
        refreshStructures();
        refreshRegions();
        refreshSettings();
        refreshResults();
        check();
    });
    ui_->autoScaleCheck->setChecked(true);
    ui_->scaleSpin->setEnabled(false);
    refreshStructures();
    refreshRegions();
    refreshSettings();
    refreshResults();
    check();
}

void MechanicsPanel::refreshStructures()
{
    const Id current = selectedStructure();
    ui_->structureCombo->clear();
    for (const auto& [id, name] : mechanics_->structures())
        ui_->structureCombo->addItem(name, QString::fromStdString(id.str()));
    if (!current.isNull())
        selectStructure(current);
}

void MechanicsPanel::refreshRegions()
{
    ui_->radiateRegionCombo->clear();
    for (const auto& [id, name] : acoustics_->objectsOfType(acoustics::object_types::Region))
        ui_->radiateRegionCombo->addItem(name, QString::fromStdString(id.str()));
}

void MechanicsPanel::selectStructure(const Id& id)
{
    const int index = ui_->structureCombo->findData(QString::fromStdString(id.str()));
    if (index >= 0)
        ui_->structureCombo->setCurrentIndex(index);
}

Id MechanicsPanel::selectedStructure() const
{
    const QString stored = ui_->structureCombo->currentData().toString();
    return stored.isEmpty() ? Id() : Id(stored.toStdString());
}

void MechanicsPanel::refreshSettings()
{
    updating_ = true;
    const MechanicsSettings s = mechanics_->settings();
    ui_->modeCountSpin->setValue(s.modeCount);
    ui_->fMinSpin->setValue(s.frequencyMin);
    ui_->fMaxSpin->setValue(s.frequencyMax);
    ui_->pointsSpin->setValue(s.frequencyPoints);
    ui_->logCheck->setChecked(s.logSpacing);
    ui_->methodCombo->setCurrentIndex(s.harmonicMethod == HarmonicMethod::Direct ? 0 : 1);
    ui_->harmonicModesSpin->setValue(s.harmonicModes);
    ui_->dampingCombo->setCurrentIndex(static_cast<int>(s.damping.kind));
    ui_->alphaSpin->setValue(s.damping.alpha);
    ui_->betaSpin->setValue(s.damping.beta);
    ui_->etaSpin->setValue(s.damping.eta);
    ui_->materialLossCheck->setChecked(s.damping.materialLossFactors);
    ui_->zetaSpin->setValue(s.damping.zeta);
    ui_->durationSpin->setValue(s.transientDuration * 1e3);
    ui_->stepsPerPeriodSpin->setValue(s.stepsPerPeriod);
    ui_->timeStepSpin->setValue(s.timeStep * 1e6);
    ui_->rayleighAlphaSpin->setValue(s.transientDamping.alpha);
    ui_->rayleighBetaSpin->setValue(s.transientDamping.beta);
    ui_->snapshotsSpin->setValue(s.snapshotCount);
    ui_->targetFrequencySpin->setValue(s.targetFrequency);
    ui_->regularizationSpin->setValue(s.regularization);
    updating_ = false;
    updateEnabled();
}

void MechanicsPanel::updateEnabled()
{
    const DampingKind kind = kDamping[ui_->dampingCombo->currentIndex()];
    ui_->alphaSpin->setEnabled(kind == DampingKind::Rayleigh);
    ui_->betaSpin->setEnabled(kind == DampingKind::Rayleigh);
    ui_->etaSpin->setEnabled(kind == DampingKind::LossFactor);
    ui_->materialLossCheck->setEnabled(kind == DampingKind::LossFactor);
    ui_->zetaSpin->setEnabled(kind == DampingKind::ModalRatio);
    ui_->harmonicModesSpin->setEnabled(ui_->methodCombo->currentIndex() == 1);
    const bool running = mechanics_ && mechanics_->isRunning();
    for (QPushButton* b : {ui_->runModalButton, ui_->runHarmonicButton, ui_->runTransientButton, ui_->runStaticButton})
        b->setEnabled(!running);
    ui_->cancelButton->setEnabled(running);
    ui_->applyTargetsButton->setEnabled(targeting_ != nullptr);
}

void MechanicsPanel::commit()
{
    if (updating_ || !mechanics_)
        return;
    MechanicsSettings s = mechanics_->settings();
    s.modeCount = ui_->modeCountSpin->value();
    s.frequencyMin = ui_->fMinSpin->value();
    s.frequencyMax = ui_->fMaxSpin->value();
    s.frequencyPoints = ui_->pointsSpin->value();
    s.logSpacing = ui_->logCheck->isChecked();
    s.harmonicMethod = ui_->methodCombo->currentIndex() == 0 ? HarmonicMethod::Direct : HarmonicMethod::Modal;
    s.harmonicModes = ui_->harmonicModesSpin->value();
    s.damping.kind = kDamping[ui_->dampingCombo->currentIndex()];
    s.damping.alpha = ui_->alphaSpin->value();
    s.damping.beta = ui_->betaSpin->value();
    s.damping.eta = ui_->etaSpin->value();
    s.damping.materialLossFactors = ui_->materialLossCheck->isChecked();
    s.damping.zeta = ui_->zetaSpin->value();
    s.transientDuration = ui_->durationSpin->value() * 1e-3;
    s.stepsPerPeriod = ui_->stepsPerPeriodSpin->value();
    s.timeStep = ui_->timeStepSpin->value() * 1e-6;
    s.transientDamping.kind = ui_->rayleighAlphaSpin->value() > 0.0 || ui_->rayleighBetaSpin->value() > 0.0 ? DampingKind::Rayleigh : DampingKind::None;
    s.transientDamping.alpha = ui_->rayleighAlphaSpin->value();
    s.transientDamping.beta = ui_->rayleighBetaSpin->value();
    s.snapshotCount = ui_->snapshotsSpin->value();
    s.targetFrequency = ui_->targetFrequencySpin->value();
    s.regularization = ui_->regularizationSpin->value();
    if (const Status status = mechanics_->setSettings(s); !status) {
        ui_->statusLabel->setText(QString::fromStdString(status.message()));
        refreshSettings();
    }
    updateEnabled();
}

void MechanicsPanel::check()
{
    checkTimer_.stop();
    const Id id = selectedStructure();
    if (id.isNull()) {
        ui_->studyText->setPlainText(QStringLiteral("Make an object structural first: select a box, cylinder, plane or rod and choose "
                                                    "Mechanics > Make Beam / Plate / Membrane / Solid."));
        return;
    }
    const StructureStudy study = mechanics_->study(id);
    QStringList lines;
    if (!study.allowed) {
        lines << QStringLiteral("CANNOT BE ANALYSED: ") + QString::fromStdString(study.refusal);
    } else {
        lines << QString::fromStdString(study.summary());
        lines << QStringLiteral("Attached: %1 actuator(s), %2 sensor(s), %3 target(s).").arg(study.forces.size()).arg(study.sensors.size()).arg(study.targets.size());
        const MechanicsSettings s = mechanics_->settings();
        const double perWave = study.elementsPerWavelength(s.frequencyMax);
        lines << QStringLiteral("At %1 Hz (top of the sweep): %2 elements per %3%4.")
                     .arg(s.frequencyMax, 0, 'g', 5)
                     .arg(perWave, 0, 'g', 3)
                     .arg(QString::fromStdString(study.waveDescription()))
                     .arg(perWave < 6.0 ? QStringLiteral(" - too coarse, refine the mesh or lower the range") : QString());
        if (!study.forces.empty()) {
            std::string basis;
            transientTimeStep(study, s, &basis);
            lines << QStringLiteral("Transient time step: ") + QString::fromStdString(basis);
        }
    }
    for (const std::string& w : study.warnings)
        lines << QStringLiteral("WARNING: ") + QString::fromStdString(w);
    for (const std::string& n : study.notes)
        lines << QString::fromStdString(n);
    ui_->studyText->setPlainText(lines.join(QLatin1Char('\n')));
}

bool MechanicsPanel::startRun(int analysis)
{
    if (!mechanics_)
        return false;
    const Id id = selectedStructure();
    if (id.isNull()) {
        ui_->statusLabel->setText(QStringLiteral("Select a structure first."));
        return false;
    }
    const auto started = mechanics_->run(id, static_cast<Analysis>(analysis));
    if (!started) {
        ui_->statusLabel->setText(QStringLiteral("Not started: %1").arg(QString::fromStdString(started.message())));
        return false;
    }
    ui_->statusLabel->setText(QStringLiteral("Running. Progress is shown under Simulation Status."));
    updateEnabled();
    return true;
}

bool MechanicsPanel::runModal() { return startRun(static_cast<int>(Analysis::Modal)); }
bool MechanicsPanel::runHarmonic() { return startRun(static_cast<int>(Analysis::Harmonic)); }
bool MechanicsPanel::runTransient() { return startRun(static_cast<int>(Analysis::Transient)); }
bool MechanicsPanel::runStatic() { return startRun(static_cast<int>(Analysis::Static)); }

void MechanicsPanel::refreshResults()
{
    {
        const QSignalBlocker blocker(ui_->resultCombo);
        ui_->resultCombo->clear();
        for (const auto& r : mechanics_->results())
            ui_->resultCombo->addItem(QString::fromStdString(r->name));
        ui_->resultCombo->setCurrentIndex(mechanics_->displayedIndex());
    }
    // The newest of each kind fills its tab.
    ui_->modeTable->setRowCount(0);
    ui_->staticText->clear();
    for (const auto& r : mechanics_->results()) {
        if (r->modal) {
            ui_->modeTable->setRowCount(static_cast<int>(r->modal->modes.size()));
            for (size_t i = 0; i < r->modal->modes.size(); ++i) {
                const Mode& m = r->modal->modes[i];
                const QStringList cells = {QString::number(i + 1), QString::number(m.frequencyHz, 'g', 6),
                                           QString::number(100 * m.effectiveMassFraction.x(), 'f', 1),
                                           QString::number(100 * m.effectiveMassFraction.y(), 'f', 1),
                                           QString::number(100 * m.effectiveMassFraction.z(), 'f', 1), QString::number(m.residual, 'g', 2)};
                for (int c = 0; c < cells.size(); ++c)
                    ui_->modeTable->setItem(static_cast<int>(i), c, new QTableWidgetItem(cells[c]));
            }
        }
        if (r->statics) {
            QStringList lines;
            lines << QString::fromStdString(r->name);
            lines << QStringLiteral("Largest displacement: %1 m").arg(r->statics->maxTranslation, 0, 'g', 5);
            lines << QStringLiteral("Compliance f.u: %1 J").arg(r->statics->compliance, 0, 'g', 5);
            if (!r->stresses.empty()) {
                double largest = 0.0;
                for (const ElementStress& s : r->stresses)
                    largest = std::max(largest, s.vonMises);
                lines << QStringLiteral("Largest stress: %1 MPa (%2)").arg(largest / 1e6, 0, 'g', 5).arg(QString::fromStdString(r->stresses.front().description));
            } else {
                lines << QStringLiteral("No stress is calculated for this idealization.");
            }
            for (const std::string& w : r->warnings)
                lines << QStringLiteral("WARNING: ") + QString::fromStdString(w);
            ui_->staticText->setPlainText(lines.join(QLatin1Char('\n')));
        }
    }
    ui_->modeTable->resizeColumnsToContents();
    refreshItems();
    refreshPlots();
}

void MechanicsPanel::refreshItems()
{
    const QSignalBlocker blocker(ui_->itemCombo);
    ui_->itemCombo->clear();
    const int index = mechanics_->displayedIndex();
    for (int i = 0; i < mechanics_->itemCount(index); ++i)
        ui_->itemCombo->addItem(mechanics_->itemLabel(index, i));
    ui_->itemCombo->setCurrentIndex(mechanics_->displayedItem());
    // Quantities a result does not have are not offered.
    auto* model = qobject_cast<QStandardItemModel*>(ui_->quantityCombo->model());
    for (int q = 0; q < ui_->quantityCombo->count(); ++q) {
        const bool ok = index >= 0 && MechanicsController::available(*mechanics_->results()[static_cast<size_t>(index)],
                                                                     static_cast<MechanicsController::Quantity>(q));
        if (model)
            model->item(q)->setEnabled(ok || index < 0);
    }
}

void MechanicsPanel::refreshPlots()
{
    // Frequency response: the newest harmonic result.
    std::shared_ptr<const StructuralResult> harmonic, transient;
    for (const auto& r : mechanics_->results()) {
        if (r->harmonic)
            harmonic = r;
        if (r->transient)
            transient = r;
    }
    if (harmonic) {
        const int keep = ui_->responseCombo->currentIndex();
        const QSignalBlocker blocker(ui_->responseCombo);
        ui_->responseCombo->clear();
        for (const ResponsePoint& p : harmonic->study->sensors)
            ui_->responseCombo->addItem(QString::fromStdString(p.label));
        for (const TargetPoint& t : harmonic->study->targets)
            ui_->responseCombo->addItem(QString::fromStdString(t.label));
        ui_->responseCombo->setCurrentIndex(std::clamp(keep, 0, ui_->responseCombo->count() - 1));
        const int point = ui_->responseCombo->currentIndex();
        if (point >= 0 && static_cast<size_t>(point) < harmonic->harmonic->responses.size()) {
            const int power = ui_->harmonicQuantityCombo->currentIndex();  // 0 displacement, 1 velocity, 2 acceleration
            const bool phase = ui_->harmonicViewCombo->currentIndex() == 1;
            QVector<double> x, y;
            const auto& f = harmonic->harmonic->frequenciesHz;
            const auto& r = harmonic->harmonic->responses[static_cast<size_t>(point)];
            std::vector<std::complex<double>> values;
            for (size_t i = 0; i < f.size(); ++i) {
                const std::complex<double> factor = std::pow(std::complex<double>(0.0, 2.0 * pi * f[i]), power);
                const std::complex<double> v = r[i] * factor;
                values.push_back(v);
                x.push_back(f[i]);
                y.push_back(phase ? std::arg(v) * 180.0 / pi : 20.0 * std::log10(std::max(1e-300, std::abs(v))));
            }
            const QString units[3] = {QStringLiteral("m/N"), QStringLiteral("(m/s)/N"), QStringLiteral("(m/s^2)/N")};
            ui_->frfPlot->setCurve(x, y, QStringLiteral("Response per newton of the first actuator's drive at ") + ui_->responseCombo->currentText(),
                                   QStringLiteral("frequency (Hz)"), phase ? QStringLiteral("phase (deg)") : QStringLiteral("dB re 1 ") + units[power]);
            ui_->frfPlot->setLogX(harmonic->study ? mechanics_->settings().logSpacing : false);
            QVector<double> markers;
            for (const auto& r2 : mechanics_->results())
                if (r2->modal && r2->study->objectId == harmonic->study->objectId)
                    for (const Mode& m : r2->modal->modes)
                        markers.push_back(m.frequencyHz);
            ui_->frfPlot->setMarkers(markers);
            QStringList notes;
            notes << QString::fromStdString(harmonic->harmonic->method) + QLatin1Char('.');
            notes << QStringLiteral("All active actuators drive together with their own amplitudes and phases; the values are the response "
                                    "divided by 1 N. Dotted lines: natural frequencies of the latest modal result.");
            // Measured responses of the same quantity.
            const QColor colours[3] = {QColor(220, 60, 60), QColor(40, 160, 70), QColor(150, 80, 200)};
            int shown = 0;
            for (const MeasuredResponse& m : mechanics_->measurements()) {
                if (static_cast<int>(m.quantity) != power || phase)
                    continue;
                QVector<double> mx, my;
                for (size_t i = 0; i < m.frequencyHz.size(); ++i) {
                    mx.push_back(m.frequencyHz[i]);
                    my.push_back(20.0 * std::log10(std::max(1e-300, std::abs(m.values[i]))));
                }
                ui_->frfPlot->addCurve(mx, my, QStringLiteral("measured: ") + QString::fromStdString(m.name), colours[shown++ % 3]);
                const ResponseComparison c = compareResponses(f, values, m);
                notes << QStringLiteral("Measured '%1': RMS level difference %2 dB over %3 points; largest peak measured at %4 Hz, simulated at %5 Hz.")
                             .arg(QString::fromStdString(m.name))
                             .arg(c.rmsLevelDifferenceDb, 0, 'f', 2)
                             .arg(c.points)
                             .arg(c.largestPeakMeasuredHz, 0, 'g', 5)
                             .arg(c.largestPeakSimulatedHz, 0, 'g', 5);
            }
            if (shown)
                ui_->frfPlot->curveCount();
            ui_->frfNote->setText(notes.join(QLatin1Char(' ')));
        }
    } else {
        ui_->frfPlot->clear();
        ui_->frfNote->clear();
    }
    if (transient) {
        const int keep = ui_->transientSensorCombo->currentIndex();
        const QSignalBlocker blocker(ui_->transientSensorCombo);
        ui_->transientSensorCombo->clear();
        for (const ResponsePoint& p : transient->study->sensors)
            ui_->transientSensorCombo->addItem(QString::fromStdString(p.label));
        ui_->transientSensorCombo->setCurrentIndex(std::clamp(keep, 0, ui_->transientSensorCombo->count() - 1));
        const TransientSolution& t = *transient->transient;
        QVector<double> x, y;
        const int q = ui_->transientQuantityCombo->currentIndex();
        if (q == 3) {
            for (size_t i = 0; i < t.energyTimes.size(); ++i) {
                x.push_back(t.energyTimes[i] * 1e3);
                y.push_back(t.kinetic[i] + t.strain[i]);
            }
            ui_->transientPlot->setCurve(x, y, QStringLiteral("Kinetic + strain energy"), QStringLiteral("time (ms)"), QStringLiteral("energy (J)"));
            QVector<double> w;
            for (const double e : t.externalWork)
                w.push_back(e);
            ui_->transientPlot->addCurve(x, w, QStringLiteral("work done by the forces"), QColor(220, 60, 60));
        } else {
            const int sensor = ui_->transientSensorCombo->currentIndex();
            if (sensor >= 0) {
                const auto& series = q == 0 ? t.displacement : q == 1 ? t.velocity : t.acceleration;
                for (size_t i = 0; i < t.time.size(); ++i) {
                    x.push_back(t.time[i] * 1e3);
                    y.push_back(series[static_cast<size_t>(sensor)][i]);
                }
                const QString units[3] = {QStringLiteral("m"), QStringLiteral("m/s"), QStringLiteral("m/s^2")};
                ui_->transientPlot->setCurve(x, y, ui_->transientSensorCombo->currentText(), QStringLiteral("time (ms)"),
                                             ui_->transientQuantityCombo->currentText().toLower() + QStringLiteral(" (") + units[q] + QLatin1Char(')'));
            } else {
                ui_->transientPlot->clear();
            }
        }
    } else {
        ui_->transientPlot->clear();
    }
}

bool MechanicsPanel::solveTargets()
{
    const auto solved = mechanics_->solveTargets(selectedStructure());
    if (!solved) {
        ui_->targetText->setPlainText(QStringLiteral("Not solved: ") + QString::fromStdString(solved.message()));
        targeting_.reset();
        updateEnabled();
        return false;
    }
    targeting_ = std::make_unique<TargetingSolution>(solved.value());
    const StructureStudy study = mechanics_->study(selectedStructure());
    ui_->targetTable->setRowCount(static_cast<int>(solved.value().achieved.size()));
    for (size_t i = 0; i < solved.value().achieved.size(); ++i) {
        const QStringList cells = {QString::fromStdString(study.targets[i].label), QString::number(std::abs(solved.value().desired[i]) * 1e6, 'g', 4),
                                   QString::number(std::abs(solved.value().achieved[i]) * 1e6, 'g', 4),
                                   QString::number(std::arg(solved.value().achieved[i]) * 180.0 / pi, 'f', 1)};
        for (int c = 0; c < cells.size(); ++c)
            ui_->targetTable->setItem(static_cast<int>(i), c, new QTableWidgetItem(cells[c]));
    }
    QStringList lines;
    lines << QStringLiteral("At %1 Hz: relative residual %2.").arg(solved.value().frequencyHz, 0, 'g', 5).arg(solved.value().relativeResidual, 0, 'g', 3);
    for (size_t j = 0; j < solved.value().forces.size(); ++j)
        lines << QStringLiteral("%1: %2 N at %3 deg")
                     .arg(QString::fromStdString(study.forces[j].label))
                     .arg(std::abs(solved.value().forces[j]), 0, 'g', 5)
                     .arg(std::arg(solved.value().forces[j]) * 180.0 / pi, 0, 'f', 1);
    lines << QString::fromStdString(solved.value().notes);
    lines << QStringLiteral("Apply writes these inputs into the actuators (one undo step); nothing changes until then.");
    ui_->targetText->setPlainText(lines.join(QLatin1Char('\n')));
    updateEnabled();
    return true;
}

bool MechanicsPanel::applyTargets()
{
    if (!targeting_)
        return false;
    const Status status = mechanics_->applyTargets(selectedStructure(), *targeting_);
    if (!status) {
        ui_->statusLabel->setText(QString::fromStdString(status.message()));
        return false;
    }
    ui_->statusLabel->setText(QStringLiteral("Actuator inputs applied."));
    return true;
}

bool MechanicsPanel::showBandStructure()
{
    const auto bands = mechanics_->bandStructure(selectedStructure());
    if (!bands) {
        ui_->bandNote->setText(QStringLiteral("Not calculated: ") + QString::fromStdString(bands.message()));
        ui_->bandPlot->clear();
        return false;
    }
    const BandStructure& b = bands.value();
    const QColor colours[4] = {QColor(30, 110, 200), QColor(220, 60, 60), QColor(40, 160, 70), QColor(150, 80, 200)};
    ui_->bandPlot->clear();
    for (size_t band = 0; band < b.frequencies.size(); ++band) {
        QVector<double> x, y;
        for (size_t i = 0; i < b.reducedWavenumber.size(); ++i) {
            x.push_back(b.reducedWavenumber[i]);
            y.push_back(b.frequencies[band][i] / 1e3);
        }
        if (band == 0)
            ui_->bandPlot->setCurve(x, y, QStringLiteral("Longitudinal Bloch waves of the unit cell"), QStringLiteral("wavenumber q a / pi"),
                                    QStringLiteral("frequency (kHz)"));
        else
            ui_->bandPlot->addCurve(x, y, QString(), colours[band % 4]);
    }
    QStringList gaps;
    for (const auto& [lo, hi] : b.stopBands)
        gaps << QStringLiteral("%1 - %2 Hz").arg(lo, 0, 'f', 0).arg(hi, 0, 'f', 0);
    ui_->bandNote->setText(QString::fromStdString(b.method) + QStringLiteral(". Stop bands (no propagating longitudinal wave): ")
                           + (gaps.isEmpty() ? QStringLiteral("none") : gaps.join(QStringLiteral(", ")))
                           + QStringLiteral(". 1D rod theory; bending and torsion are not part of this band structure."));
    return true;
}

bool MechanicsPanel::radiate()
{
    const QString region = ui_->radiateRegionCombo->currentData().toString();
    const Status status = mechanics_->radiate(mechanics_->displayedIndex(), mechanics_->displayedItem(), region.isEmpty() ? Id() : Id(region.toStdString()));
    ui_->statusLabel->setText(status ? QStringLiteral("Radiated sound added to the acoustic results.") : QString::fromStdString(status.message()));
    return status.ok();
}

void MechanicsPanel::showTab(const QString& objectName)
{
    if (QWidget* tab = ui_->tabs->findChild<QWidget*>(objectName))
        ui_->tabs->setCurrentWidget(tab);
}

QString MechanicsPanel::studyText() const { return ui_->studyText->toPlainText(); }
QString MechanicsPanel::statusText() const { return ui_->statusLabel->text(); }
QString MechanicsPanel::displayText() const { return ui_->displayText->toPlainText(); }
QString MechanicsPanel::targetText() const { return ui_->targetText->toPlainText(); }
QString MechanicsPanel::frfNote() const { return ui_->frfNote->text(); }
int MechanicsPanel::modeRowCount() const { return ui_->modeTable->rowCount(); }
QString MechanicsPanel::modeCell(int row, int column) const
{
    const QTableWidgetItem* item = ui_->modeTable->item(row, column);
    return item ? item->text() : QString();
}
PlotWidget& MechanicsPanel::frfPlot() { return *ui_->frfPlot; }
PlotWidget& MechanicsPanel::transientPlot() { return *ui_->transientPlot; }
PlotWidget& MechanicsPanel::bandPlot() { return *ui_->bandPlot; }
