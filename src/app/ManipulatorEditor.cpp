#include "ManipulatorEditor.h"

#include "AcousticController.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "ManipulatorController.h"
#include "ui_ManipulatorEditor.h"

using namespace resonant;
using namespace resonant::manipulators;

namespace {
const ManipulatorComponent* componentOfObject(const SceneObject* object)
{
    return object ? dynamic_cast<const ManipulatorComponent*>(object->state.component.get()) : nullptr;
}

QString categoryLabel(Category category)
{
    switch (category) {
    case Category::Reflector: return QStringLiteral("Reflector");
    case Category::Lens: return QStringLiteral("Lens");
    case Category::Waveguide: return QStringLiteral("Waveguide");
    case Category::ResonantStructure: return QStringLiteral("Resonant structure");
    case Category::DiffractionStructure: return QStringLiteral("Diffraction structure");
    }
    return {};
}
}  // namespace

ManipulatorEditor::ManipulatorEditor(Id id, AcousticController& acoustics, ManipulatorController& manipulators,
                                     DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), acoustics, units, parent), ui_(std::make_unique<Ui::ManipulatorEditor>()),
      manipulators_(manipulators)
{
    ui_->setupUi(this);
    const auto edited = [this] { commit(); };
    connect(ui_->boundaryCombo, &QComboBox::activated, this, edited);
    connect(ui_->focusCheck, &QCheckBox::clicked, this, edited);
    connect(ui_->sourceCheck, &QCheckBox::clicked, this, edited);
    connect(ui_->notesEdit, &QLineEdit::editingFinished, this, edited);
    for (QDoubleSpinBox* spin : {ui_->focusX, ui_->focusY, ui_->focusZ, ui_->sourceX, ui_->sourceY, ui_->sourceZ})
        connect(spin, &QDoubleSpinBox::valueChanged, this, edited);
    connect(ui_->optimizeButton, &QPushButton::clicked, this, [this] { optimize(); });
    connect(ui_->applyBestButton, &QPushButton::clicked, this, [this] { applyBest(); });
    connect(ui_->optimizeParameterCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_)
            refresh();
    });
    connect(&controller_, &AcousticController::settingsChanged, this, [this] { refresh(); });
    refresh();
}

ManipulatorEditor::~ManipulatorEditor() = default;

QString ManipulatorEditor::optimizeResultText() const { return ui_->optimizeResult->text(); }
QString ManipulatorEditor::estimates() const { return ui_->estimatesText->toPlainText(); }

void ManipulatorEditor::refresh()
{
    const SceneObject* current = object();
    const auto* manipulator = componentOfObject(current);
    if (!manipulator || !current->state.geometry)
        return;
    updating_ = true;
    const ManipulatorSettings& s = manipulator->settings;
    const Geometry& geometry = *current->state.geometry;
    const bool reflector = isReflectorGeometry(geometry);
    ui_->categoryValue->setText(categoryLabel(s.category));
    ui_->supportValue->setText(QString::fromStdString(supportDescription(geometry)));
    ui_->boundaryCombo->setCurrentIndex(static_cast<int>(s.boundary));
    ui_->boundaryCombo->setEnabled(reflector);
    ui_->boundaryCombo->setToolTip(reflector ? QString()
                                             : QStringLiteral("Stored with the design; no model uses it for this kind of manipulator yet."));

    const QString suffix = QLatin1Char(' ') + units_.lengthSymbol();
    const auto showPoint = [&](QCheckBox* check, QDoubleSpinBox* spins[3], const std::optional<Vec3>& point) {
        check->setChecked(point.has_value());
        for (int axis = 0; axis < 3; ++axis) {
            spins[axis]->setSuffix(suffix);
            spins[axis]->setEnabled(point.has_value());
            if (point)
                spins[axis]->setValue(units_.lengthFromSI((*point)[axis]));
        }
    };
    QDoubleSpinBox* focus[3] = {ui_->focusX, ui_->focusY, ui_->focusZ};
    QDoubleSpinBox* source[3] = {ui_->sourceX, ui_->sourceY, ui_->sourceZ};
    showPoint(ui_->focusCheck, focus, s.intendedFocus);
    showPoint(ui_->sourceCheck, source, s.intendedSource);
    ui_->notesEdit->setText(QString::fromStdString(s.notes));
    ui_->estimatesText->setPlainText(manipulators_.estimatesText(*current, units_));

    // Optimization: reflectors only, over one length parameter.
    ui_->optimizeGroup->setVisible(reflector);
    if (reflector) {
        const QString selected = ui_->optimizeParameterCombo->currentData().toString();
        ui_->optimizeParameterCombo->clear();
        for (const ParameterDef& def : geometry.kind().parameters)
            if (def.key != "thickness")
                ui_->optimizeParameterCombo->addItem(QString::fromStdString(def.label), QString::fromStdString(def.key));
        const int index = ui_->optimizeParameterCombo->findData(selected);
        ui_->optimizeParameterCombo->setCurrentIndex(std::max(0, index));
        const std::string key = ui_->optimizeParameterCombo->currentData().toString().toStdString();
        if (const ParameterDef* def = geometry.kind().findParameter(key)) {
            const QString siUnit = QString::fromStdString(def->unit);
            const QString symbol = units_.symbolFor(siUnit);
            const double value = geometry.parameter(key);
            for (QDoubleSpinBox* spin : {ui_->optimizeLower, ui_->optimizeUpper})
                spin->setSuffix(symbol.isEmpty() ? QString() : QLatin1Char(' ') + symbol);
            // A default search range around the current value, unless the user set one for this parameter.
            if (ui_->optimizeLower->property("parameter").toString() != QString::fromStdString(key)) {
                ui_->optimizeLower->setValue(units_.fromSI(def->unit == "m" ? 0.5 * value : std::max(def->minValue, value - 0.5), siUnit));
                ui_->optimizeUpper->setValue(units_.fromSI(def->unit == "m" ? 2.0 * value : std::min(def->maxValue, value + 0.5), siUnit));
                ui_->optimizeLower->setProperty("parameter", QString::fromStdString(key));
                outcome_.reset();
            }
        }
        ui_->applyBestButton->setEnabled(outcome_.has_value() && outcome_->improved);
    }
    updating_ = false;
}

void ManipulatorEditor::commit()
{
    const auto* manipulator = componentOfObject(object());
    if (updating_ || !manipulator)
        return;
    ManipulatorSettings s = manipulator->settings;
    s.boundary = static_cast<BoundaryCondition>(ui_->boundaryCombo->currentIndex());
    const auto takePoint = [&](const QCheckBox* check, QDoubleSpinBox* spins[3], std::optional<Vec3>& point) {
        if (!check->isChecked()) {
            point.reset();
            return;
        }
        Vec3 value = point.value_or(Vec3::Zero());
        if (point)  // only what the user changed, at full precision otherwise
            for (int axis = 0; axis < 3; ++axis)
                takeLength(value[axis], spins[axis]);
        point = value;
    };
    QDoubleSpinBox* focus[3] = {ui_->focusX, ui_->focusY, ui_->focusZ};
    QDoubleSpinBox* source[3] = {ui_->sourceX, ui_->sourceY, ui_->sourceZ};
    takePoint(ui_->focusCheck, focus, s.intendedFocus);
    takePoint(ui_->sourceCheck, source, s.intendedSource);
    s.notes = ui_->notesEdit->text().toStdString();
    report(manipulators_.setSettings(id_, s));
    refresh();
}

bool ManipulatorEditor::optimize()
{
    outcome_.reset();
    ui_->applyBestButton->setEnabled(false);
    const SceneObject* current = object();
    if (!current || !current->state.geometry)
        return false;
    const std::string key = ui_->optimizeParameterCombo->currentData().toString().toStdString();
    const ParameterDef* def = current->state.geometry->kind().findParameter(key);
    if (!def)
        return false;
    const QString siUnit = QString::fromStdString(def->unit);
    auto result = manipulators_.optimizeReflector(id_, key, units_.toSI(ui_->optimizeLower->value(), siUnit),
                                                  units_.toSI(ui_->optimizeUpper->value(), siUnit));
    if (!result) {
        ui_->optimizeResult->setText(QStringLiteral("Not optimized: %1.").arg(QString::fromStdString(result.message())));
        return false;
    }
    outcome_ = result.take();
    const QString symbol = units_.symbolFor(siUnit);
    QStringList notes;
    for (const std::string& note : outcome_->notes)
        notes.append(QString::fromStdString(note));
    ui_->optimizeResult->setText(
        QStringLiteral("%1: %2 %3 -> %4 %3.\n|P| at the intended focus: %5 Pa -> %6 Pa (%7 candidates evaluated with the reflector model).\n%8\n%9")
            .arg(QString::fromStdString(def->label))
            .arg(units_.fromSI(outcome_->initial.values[0], siUnit), 0, 'g', 5)
            .arg(symbol)
            .arg(units_.fromSI(outcome_->best.values[0], siUnit), 0, 'g', 5)
            .arg(outcome_->initial.objective, 0, 'g', 4)
            .arg(outcome_->best.objective, 0, 'g', 4)
            .arg(outcome_->history.size())
            .arg(outcome_->improved ? QStringLiteral("Improved. The scene is unchanged until you apply the best geometry.")
                                    : QStringLiteral("No better candidate than the current design was found."))
            .arg(notes.join(QLatin1Char('\n'))));
    ui_->applyBestButton->setEnabled(outcome_->improved);
    return true;
}

bool ManipulatorEditor::applyBest()
{
    if (!outcome_ || !outcome_->bestGeometry)
        return false;
    const Status status = manipulators_.applyGeometry(id_, *outcome_->bestGeometry);
    report(status);
    if (status) {
        outcome_.reset();
        refresh();
    }
    return status.ok();
}

PropertiesPanel::ComponentEditorFactory makeManipulatorEditorFactory(AcousticController& acoustics,
                                                                     ManipulatorController& manipulators)
{
    return [&acoustics, &manipulators](const SceneObject& object, DocumentController&, DisplayUnits& units,
                                       QWidget* parent) -> QWidget* {
        if (dynamic_cast<const ManipulatorComponent*>(object.state.component.get()))
            return new ManipulatorEditor(object.id(), acoustics, manipulators, units, parent);
        return nullptr;
    };
}
