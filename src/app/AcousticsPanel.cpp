#include "AcousticsPanel.h"

#include "DisplayUnits.h"
#include "DocumentController.h"
#include "foundation/Units.h"
#include "ui_AcousticsPanel.h"

#include <QSignalBlocker>

#include <cmath>

using namespace resonant;
using namespace resonant::acoustics;

namespace {
constexpr double kZeroCelsius = 273.15;

void fillCombo(QComboBox* combo, const std::vector<std::pair<Id, QString>>& entries, const QString& extra = {})
{
    const QString previous = combo->currentData().toString();
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const auto& [id, name] : entries)
        combo->addItem(name, QString::fromStdString(id.str()));
    if (!extra.isEmpty())
        combo->addItem(extra, QString());
    const int index = combo->findData(previous);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void selectById(QComboBox* combo, const Id& id)
{
    const int index = combo->findData(QString::fromStdString(id.str()));
    if (index >= 0)
        combo->setCurrentIndex(index);
}

QTableWidgetItem* cell(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
}  // namespace

AcousticsPanel::AcousticsPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::AcousticsPanel>())
{
    ui_->setupUi(this);
    ui_->warningLabel->setStyleSheet(QStringLiteral("color: #b9770e;"));

    // --- medium ---
    for (QDoubleSpinBox* spin : {ui_->temperatureSpin, ui_->densitySpin, ui_->soundSpeedSpin, ui_->attenuationSpin,
                                 ui_->frequencySpin, ui_->minDistanceSpin})
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { commitMedium(); });
    connect(ui_->mediumPreset, &QComboBox::activated, this, [this] { commitMedium(); });
    connect(ui_->directivityCheck, &QCheckBox::clicked, this, [this] { commitMedium(); });
    connect(ui_->attenuationCheck, &QCheckBox::clicked, this, [this] { commitMedium(); });

    // --- focus ---
    connect(ui_->focusTargetCombo, &QComboBox::currentIndexChanged, this, [this] { updateFocusPosition(); });
    connect(ui_->focusCalculateButton, &QPushButton::clicked, this, [this] { calculateFocus(); });
    connect(ui_->focusApplyButton, &QPushButton::clicked, this, [this] { applyFocus(); });

    // --- inverse design ---
    ui_->regularizationEdit->setText(QStringLiteral("1e-6"));
    connect(ui_->inverseSolveButton, &QPushButton::clicked, this, [this] { solveInverse(); });
    connect(ui_->inverseApplyButton, &QPushButton::clicked, this, [this] { applyInverse(); });

    // --- simulation ---
    connect(ui_->regionCombo, &QComboBox::currentIndexChanged, this, [this] { refreshEstimate(); });
    connect(ui_->runButton, &QPushButton::clicked, this, [this] { runSimulation(); });
    connect(ui_->resultList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updating_ || !controller_)
            return;
        controller_->displayResult(row >= 0 ? Id(ui_->resultList->item(row)->data(Qt::UserRole).toString().toStdString()) : Id());
        ui_->removeResultButton->setEnabled(row >= 0);
    });
    connect(ui_->removeResultButton, &QPushButton::clicked, this, [this] {
        if (const QListWidgetItem* item = ui_->resultList->currentItem())
            controller_->removeResult(Id(item->data(Qt::UserRole).toString().toStdString()));
    });
    connect(ui_->showFieldCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (controller_)
            controller_->setFieldVisible(on);
    });

    // --- display ---
    connect(ui_->quantityCombo, &QComboBox::activated, this, [this](int index) {
        controller_->setQuantity(static_cast<render::FieldQuantity>(index));
    });
    const auto normalization = [this] {
        if (updating_)
            return;
        controller_->setNormalization(ui_->normalizationCombo->currentIndex() == 0
                                          ? AcousticController::Normalization::Normalized
                                          : AcousticController::Normalization::Absolute,
                                      ui_->absoluteScaleSpin->value());
    };
    connect(ui_->normalizationCombo, &QComboBox::activated, this, normalization);
    connect(ui_->absoluteScaleSpin, &QDoubleSpinBox::valueChanged, this, normalization);
    connect(ui_->animateCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            controller_->setQuantity(render::FieldQuantity::Instantaneous);
        controller_->setAnimating(on);
    });
    connect(ui_->animationRateSpin, &QDoubleSpinBox::valueChanged, this,
            [this](double rate) { controller_->setAnimationRate(rate); });
    const auto slices = [this] {
        if (updating_)
            return;
        controller_->setSlices(ui_->sliceUSlider->value() / 1000.0, ui_->sliceVSlider->value() / 1000.0,
                               ui_->sliceWSlider->value() / 1000.0, ui_->sliceUCheck->isChecked(),
                               ui_->sliceVCheck->isChecked(), ui_->sliceWCheck->isChecked());
    };
    for (QSlider* slider : {ui_->sliceUSlider, ui_->sliceVSlider, ui_->sliceWSlider})
        connect(slider, &QSlider::valueChanged, this, slices);
    for (QCheckBox* box : {ui_->sliceUCheck, ui_->sliceVCheck, ui_->sliceWCheck})
        connect(box, &QCheckBox::toggled, this, slices);
}

AcousticsPanel::~AcousticsPanel() = default;

void AcousticsPanel::setController(AcousticController* controller, DisplayUnits* units)
{
    controller_ = controller;
    units_ = units;
    DocumentController* document = &controller_->document();
    connect(document, &DocumentController::sceneChanged, this, [this](const SceneEvent& event) {
        if (event.kind != SceneEvent::Kind::Changed)
            refreshObjectLists();
        else
            updateFocusPosition();
        refreshEstimate();
    });
    connect(document, &DocumentController::projectReplaced, this, [this] {
        focus_.reset();
        inverse_.reset();
        refreshObjectLists();
        refreshMedium();
    });
    connect(controller_, &AcousticController::settingsChanged, this, [this] {
        refreshMedium();
        refreshEstimate();
    });
    connect(controller_, &AcousticController::resultsChanged, this, &AcousticsPanel::refreshResults);
    connect(controller_, &AcousticController::displayChanged, this, &AcousticsPanel::refreshDisplay);
    connect(units_, &DisplayUnits::changed, this, [this] { updateFocusPosition(); });
    refreshObjectLists();
    refreshMedium();
    refreshResults();
    refreshDisplay();
}

void AcousticsPanel::say(const QString& text)
{
    emit controller_->message(text);
}

Id AcousticsPanel::comboId(const QComboBox* combo) const
{
    return Id(combo->currentData().toString().toStdString());
}

void AcousticsPanel::showTab(const QString& objectName)
{
    if (QWidget* tab = ui_->tabs->findChild<QWidget*>(objectName))
        ui_->tabs->setCurrentWidget(tab);
}

void AcousticsPanel::selectRegion(const Id& id) { selectById(ui_->regionCombo, id); }
void AcousticsPanel::selectFocusArray(const Id& id) { selectById(ui_->focusArrayCombo, id); }
void AcousticsPanel::selectFocusTarget(const Id& id) { selectById(ui_->focusTargetCombo, id); }
void AcousticsPanel::selectInverseArray(const Id& id) { selectById(ui_->inverseArrayCombo, id); }
QString AcousticsPanel::estimateText() const { return ui_->estimateLabel->text() + QLatin1Char('\n') + ui_->warningLabel->text(); }
QString AcousticsPanel::inverseSummaryText() const { return ui_->inverseSummary->text() + QLatin1Char('\n') + ui_->inverseNotes->toPlainText(); }

void AcousticsPanel::refreshObjectLists()
{
    if (!controller_)
        return;
    const auto arrays = controller_->objectsOfType(acoustics::object_types::Array);
    fillCombo(ui_->focusArrayCombo, arrays);
    fillCombo(ui_->inverseArrayCombo, arrays);
    fillCombo(ui_->focusTargetCombo, controller_->objectsOfType(acoustics::object_types::Target), QStringLiteral("Coordinates below"));
    fillCombo(ui_->regionCombo, controller_->objectsOfType(acoustics::object_types::Region));
    ui_->focusCalculateButton->setEnabled(!arrays.empty());
    ui_->inverseSolveButton->setEnabled(!arrays.empty());
    updateFocusPosition();
    refreshEstimate();
}

void AcousticsPanel::updateFocusPosition()
{
    if (!controller_)
        return;
    const Id target = comboId(ui_->focusTargetCombo);
    const SceneObject* object = controller_->document().scene().find(target);
    const QString suffix = QLatin1Char(' ') + units_->lengthSymbol();
    QDoubleSpinBox* spins[3] = {ui_->focusX, ui_->focusY, ui_->focusZ};
    for (int axis = 0; axis < 3; ++axis) {
        const QSignalBlocker blocker(spins[axis]);
        spins[axis]->setSuffix(suffix);
        spins[axis]->setEnabled(object == nullptr);
        if (object)
            spins[axis]->setValue(units_->lengthFromSI(
                transformPoint(controller_->document().scene().worldMatrix(*object), Vec3::Zero())[axis]));
    }
}

void AcousticsPanel::refreshMedium()
{
    if (!controller_)
        return;
    updating_ = true;
    const AcousticSettings s = controller_->settings();
    const int preset = s.medium.name == "Air" ? 0 : s.medium.name == "Water" ? 1 : 2;
    ui_->mediumPreset->setCurrentIndex(preset);
    ui_->temperatureSpin->setValue(s.medium.temperatureK - kZeroCelsius);
    ui_->densitySpin->setValue(s.medium.density);
    ui_->soundSpeedSpin->setValue(s.medium.soundSpeed);
    ui_->attenuationSpin->setValue(s.medium.attenuation);
    // The ideal-gas model derives density and speed from the temperature;
    // the nominal water values are fixed; only a custom medium is free.
    ui_->temperatureSpin->setEnabled(preset != 1);
    ui_->densitySpin->setEnabled(preset == 2);
    ui_->soundSpeedSpin->setEnabled(preset == 2);
    ui_->basisLabel->setText(QString::fromStdString(s.medium.basis));
    ui_->impedanceValue->setText(QStringLiteral("%1 Pa s/m").arg(s.medium.impedance(), 0, 'g', 6));
    ui_->frequencySpin->setValue(s.frequencyHz);
    ui_->wavelengthValue->setText(QStringLiteral("%1 %2").arg(units_->lengthFromSI(s.medium.wavelength(s.frequencyHz)), 0, 'g', 5)
                                      .arg(units_->lengthSymbol()));
    ui_->directivityCheck->setChecked(s.propagation.includeDirectivity);
    ui_->attenuationCheck->setChecked(s.propagation.includeAttenuation);
    ui_->minDistanceSpin->setValue(s.propagation.minimumDistance * 1000.0);
    updating_ = false;
}

void AcousticsPanel::commitMedium()
{
    if (updating_ || !controller_)
        return;
    AcousticSettings s = controller_->settings();
    const int preset = ui_->mediumPreset->currentIndex();
    const double temperature = ui_->temperatureSpin->value() + kZeroCelsius;
    const double attenuation = ui_->attenuationSpin->value();
    if (preset == 0) {
        s.medium = airAtTemperature(temperature);
    } else if (preset == 1) {
        s.medium = waterNominal();
    } else {
        s.medium.name = "Custom";
        s.medium.temperatureK = temperature;
        s.medium.density = ui_->densitySpin->value();
        s.medium.soundSpeed = ui_->soundSpeedSpin->value();
        s.medium.basis = "Values entered by the user.";
    }
    s.medium.attenuation = attenuation;
    s.frequencyHz = ui_->frequencySpin->value();
    s.propagation.includeDirectivity = ui_->directivityCheck->isChecked();
    s.propagation.includeAttenuation = ui_->attenuationCheck->isChecked();
    s.propagation.minimumDistance = ui_->minDistanceSpin->value() / 1000.0;
    if (Status status = controller_->setSettings(s); !status)
        say(QString::fromStdString(status.message()));
    refreshMedium();
}

bool AcousticsPanel::calculateFocus()
{
    focus_.reset();
    ui_->focusApplyButton->setEnabled(false);
    ui_->focusTable->setRowCount(0);
    const Vec3 target(units_->lengthToSI(ui_->focusX->value()), units_->lengthToSI(ui_->focusY->value()),
                      units_->lengthToSI(ui_->focusZ->value()));
    focusArray_ = comboId(ui_->focusArrayCombo);
    auto solution = controller_->computeFocus(focusArray_, target,
                                              static_cast<FocusControl>(ui_->focusControlCombo->currentIndex()),
                                              static_cast<Taper>(ui_->focusTaperCombo->currentIndex()));
    if (!solution) {
        say(QString::fromStdString(solution.message()));
        return false;
    }
    focus_ = solution.take();
    const int rows = std::min<int>(static_cast<int>(focus_->elements.size()), 512);
    ui_->focusTable->setRowCount(rows);
    for (int row = 0; row < rows; ++row) {
        const FocusElement& e = focus_->elements[static_cast<size_t>(row)];
        ui_->focusTable->setItem(row, 0, cell(QString::number(e.index)));
        ui_->focusTable->setItem(row, 1, cell(QStringLiteral("%1 %2").arg(units_->lengthFromSI(e.distance), 0, 'g', 6).arg(units_->lengthSymbol())));
        ui_->focusTable->setItem(row, 2, cell(focus_->control == FocusControl::Phase ? QString::number(units::radiansToDegrees(e.phaseRad), 'f', 2) : QStringLiteral("-")));
        ui_->focusTable->setItem(row, 3, cell(focus_->control == FocusControl::TimeDelay ? QString::number(e.delaySec * 1e6, 'f', 3) : QStringLiteral("-")));
        ui_->focusTable->setItem(row, 4, cell(QString::number(e.weight, 'f', 3)));
    }
    ui_->focusApplyButton->setEnabled(true);
    ui_->focusNote->setText(focus_->control == FocusControl::Phase
                                ? QStringLiteral("Phases for %1 Hz in a medium with c = %2 m/s. They are valid at this frequency only. Apply them, then run a field simulation to see the field that results.")
                                      .arg(focus_->frequencyHz, 0, 'g', 8).arg(focus_->soundSpeed, 0, 'g', 6)
                                : QStringLiteral("Time delays for c = %1 m/s. They align the arrivals at any frequency. Apply them, then run a field simulation to see the field that results.")
                                      .arg(focus_->soundSpeed, 0, 'g', 6));
    return true;
}

bool AcousticsPanel::applyFocus()
{
    if (!focus_)
        return false;
    const Status status = controller_->applyFocus(focusArray_, *focus_, static_cast<Taper>(ui_->focusTaperCombo->currentIndex()));
    if (!status) {
        say(QString::fromStdString(status.message()));
        return false;
    }
    say(QStringLiteral("Focus settings applied to %1 elements.").arg(focus_->elements.size()));
    return true;
}

bool AcousticsPanel::solveInverse()
{
    inverse_.reset();
    ui_->inverseApplyButton->setEnabled(false);
    ui_->inverseTable->setRowCount(0);
    ui_->inverseNotes->clear();

    InverseOptions options;
    bool ok = true;
    options.regularization = ui_->regularizationEdit->text().trimmed().toDouble(&ok);
    if (!ok || options.regularization < 0.0) {
        ui_->inverseSummary->setText(QStringLiteral("The regularization must be a number that is not negative, for example 1e-6."));
        return false;
    }
    const QString limit = ui_->amplitudeLimitEdit->text().trimmed();
    if (!limit.isEmpty()) {
        options.maxAmplitudePa = limit.toDouble(&ok);
        if (!ok || options.maxAmplitudePa <= 0.0) {
            ui_->inverseSummary->setText(QStringLiteral("The amplitude limit must be a positive number, or empty for no limit."));
            return false;
        }
    }
    options.residualTolerance = ui_->toleranceSpin->value() / 100.0;

    inverseArray_ = comboId(ui_->inverseArrayCombo);
    AcousticController::InverseRun run = controller_->solveInverse(inverseArray_, options);
    const InverseSolution& solution = run.solution;
    if (!solution.solved) {
        ui_->inverseSummary->setText(QStringLiteral("Not solved: %1.").arg(QString::fromStdString(solution.message)));
        return false;
    }
    ui_->inverseSummary->setText(
        QStringLiteral("%1  Residual %2 Pa (%3 % of the requested field), %4 sources, %5 targets, rank %6, condition number %7, largest strength %8 Pa at 1 m.")
            .arg(solution.accurate ? QStringLiteral("Targets achieved within tolerance.") : QStringLiteral("TARGETS NOT ACHIEVED ACCURATELY."))
            .arg(solution.residualNorm, 0, 'g', 4)
            .arg(100.0 * solution.relativeResidual, 0, 'g', 3)
            .arg(run.sources.size())
            .arg(run.targets.points.size())
            .arg(solution.rank)
            .arg(solution.conditionNumber, 0, 'g', 3)
            .arg(solution.weights.cwiseAbs().maxCoeff(), 0, 'g', 4));
    ui_->inverseTable->setRowCount(static_cast<int>(run.targets.points.size()));
    for (int row = 0; row < static_cast<int>(run.targets.points.size()); ++row) {
        const TargetPoint& target = run.targets.points[static_cast<size_t>(row)];
        const Complex achieved = solution.achieved(row);
        const double wanted = std::abs(target.desired);
        ui_->inverseTable->setItem(row, 0, cell(QString::fromStdString(target.label)));
        ui_->inverseTable->setItem(row, 1, cell(QString::number(wanted, 'g', 5)));
        ui_->inverseTable->setItem(row, 2, cell(QString::number(std::abs(achieved), 'g', 5)));
        ui_->inverseTable->setItem(row, 3, cell(wanted > 0.0 ? QString::number(100.0 * (std::abs(achieved) - wanted) / wanted, 'f', 2) : QStringLiteral("-")));
        ui_->inverseTable->setItem(row, 4, cell(wanted > 0.0 ? QString::number(units::radiansToDegrees(std::remainder(std::arg(achieved) - std::arg(target.desired), 2.0 * units::pi)), 'f', 2) : QStringLiteral("-")));
        ui_->inverseTable->setItem(row, 5, cell(QString::number(target.weight, 'g', 4)));
    }
    QStringList notes;
    for (const std::string& note : solution.notes)
        notes.append(QString::fromStdString(note));
    notes.append(QStringLiteral("Model: analytical free field; the solution assumes the propagation matrix of that model."));
    ui_->inverseNotes->setPlainText(notes.join(QLatin1Char('\n')));
    inverse_ = std::move(run);
    ui_->inverseApplyButton->setEnabled(true);
    return true;
}

bool AcousticsPanel::applyInverse()
{
    if (!inverse_)
        return false;
    const Status status = controller_->applyInverse(inverseArray_, *inverse_);
    if (!status) {
        say(QString::fromStdString(status.message()));
        return false;
    }
    say(QStringLiteral("Inverse-design strengths applied to the array."));
    return true;
}

void AcousticsPanel::refreshEstimate()
{
    if (!controller_)
        return;
    auto estimated = controller_->estimate(comboId(ui_->regionCombo));
    if (!estimated) {
        ui_->estimateLabel->setText(QStringLiteral("Add a field region to the scene (Acoustics menu), then choose it here."));
        ui_->warningLabel->clear();
        ui_->runButton->setEnabled(false);
        return;
    }
    const FieldEstimate& e = estimated.value();
    const AcousticSettings s = controller_->settings();
    ui_->estimateLabel->setText(
        QStringLiteral("%1 samples, %2 MiB, %3 source evaluations.\nSample spacing %4 %5; wavelength %6 %5 (%7 samples per wavelength).\n%8 Hz in %9; free-field model: objects in the scene are ignored.")
            .arg(e.points)
            .arg(e.bytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(e.evaluations, 0, 'g', 3)
            .arg(units_->lengthFromSI(e.spacing), 0, 'g', 4)
            .arg(units_->lengthSymbol())
            .arg(units_->lengthFromSI(e.wavelength), 0, 'g', 4)
            .arg(e.samplesPerWavelength, 0, 'f', 1)
            .arg(s.frequencyHz, 0, 'g', 8)
            .arg(QString::fromStdString(s.medium.name)));
    QStringList warnings;
    if (!e.allowed)
        warnings.append(QString::fromStdString(e.refusal));
    for (const std::string& warning : e.warnings)
        warnings.append(QString::fromStdString(warning));
    ui_->warningLabel->setText(warnings.join(QLatin1Char('\n')));
    ui_->runButton->setEnabled(e.allowed);
}

bool AcousticsPanel::runSimulation()
{
    auto started = controller_->runField(comboId(ui_->regionCombo));
    if (!started) {
        say(QString::fromStdString(started.message()));
        return false;
    }
    say(QStringLiteral("Field simulation started."));
    return true;
}

void AcousticsPanel::refreshResults()
{
    updating_ = true;
    ui_->resultList->clear();
    const auto displayed = controller_->displayedResult();
    for (const auto& result : controller_->results()) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  -  peak %2 Pa, %3 s").arg(QString::fromStdString(result->name)).arg(result->statistics.maxMagnitude, 0, 'g', 4).arg(result->metadata.elapsedSeconds, 0, 'f', 2),
            ui_->resultList);
        item->setData(Qt::UserRole, QString::fromStdString(result->id.str()));
        if (displayed && displayed->id == result->id)
            ui_->resultList->setCurrentItem(item);
    }
    ui_->removeResultButton->setEnabled(ui_->resultList->currentRow() >= 0);
    updating_ = false;
}

void AcousticsPanel::refreshDisplay()
{
    if (!controller_)
        return;
    updating_ = true;
    ui_->quantityCombo->setCurrentIndex(static_cast<int>(controller_->display().quantity));
    ui_->normalizationCombo->setCurrentIndex(controller_->normalization() == AcousticController::Normalization::Normalized ? 0 : 1);
    ui_->absoluteScaleSpin->setEnabled(controller_->normalization() == AcousticController::Normalization::Absolute);
    ui_->absoluteScaleSpin->setValue(controller_->absoluteScale());
    ui_->displayDescription->setText(controller_->displayDescription());
    const auto displayed = controller_->displayedResult();
    ui_->sliceGroup->setEnabled(displayed && displayed->frame.isVolume());
    updating_ = false;
}
