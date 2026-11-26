#include "AcousticEditors.h"

#include "AcousticController.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "foundation/Units.h"
#include "ui_ArrayEditor.h"
#include "ui_EmitterEditor.h"
#include "ui_RegionEditor.h"
#include "ui_TargetEditor.h"

#include <QSignalBlocker>

#include <cmath>

using namespace resonant;
using namespace resonant::acoustics;

namespace {
constexpr double kDegrees = 180.0 / units::pi;  // displayed = stored * factor
constexpr double kMicroseconds = 1e6;

template <typename Editor, typename Function>
void onEdited(Editor* editor, const QList<QWidget*>& widgets, Function&& commit)
{
    for (QWidget* widget : widgets) {
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget))
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, editor, commit);
        else if (auto* integer = qobject_cast<QSpinBox*>(widget))
            QObject::connect(integer, &QSpinBox::valueChanged, editor, commit);
        else if (auto* combo = qobject_cast<QComboBox*>(widget))
            QObject::connect(combo, &QComboBox::activated, editor, commit);
        else if (auto* check = qobject_cast<QCheckBox*>(widget))
            QObject::connect(check, &QCheckBox::clicked, editor, commit);
        else if (auto* line = qobject_cast<QLineEdit*>(widget))
            QObject::connect(line, &QLineEdit::editingFinished, editor, commit);
    }
}
}  // namespace

// --- base -----------------------------------------------------------------------

ComponentEditor::ComponentEditor(Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent)
    : QWidget(parent), id_(std::move(id)), controller_(controller), units_(units)
{
    connect(&controller_.document(), &DocumentController::sceneChanged, this, [this](const SceneEvent& event) {
        if (event.kind == SceneEvent::Kind::Changed && event.id == id_)
            refresh();
    });
    connect(&units_, &DisplayUnits::changed, this, [this] { refresh(); });
}

const SceneObject* ComponentEditor::object() const
{
    return controller_.document().scene().find(id_);
}

void ComponentEditor::report(const Status& status)
{
    if (!status)
        emit controller_.message(QString::fromStdString(status.message()));
}

void ComponentEditor::take(double& field, const QDoubleSpinBox* spin, double factor) const
{
    if (std::abs(spin->value() - field * factor) > 0.51 * std::pow(10.0, -spin->decimals()))
        field = spin->value() / factor;
}

void ComponentEditor::show(QDoubleSpinBox* spin, double value, double factor) const
{
    spin->setValue(value * factor);
}

void ComponentEditor::showLength(QDoubleSpinBox* spin, double meters) const
{
    spin->setSuffix(QLatin1Char(' ') + units_.lengthSymbol());
    spin->setValue(units_.lengthFromSI(meters));
}

void ComponentEditor::takeLength(double& meters, const QDoubleSpinBox* spin) const
{
    if (std::abs(spin->value() - units_.lengthFromSI(meters)) > 0.51 * std::pow(10.0, -spin->decimals()))
        meters = units_.lengthToSI(spin->value());
}

// --- emitter ---------------------------------------------------------------------

EmitterEditor::EmitterEditor(Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), controller, units, parent), ui_(std::make_unique<Ui::EmitterEditor>())
{
    ui_->setupUi(this);
    onEdited(this,
             {ui_->frequencySpin, ui_->amplitudeSpin, ui_->phaseSpin, ui_->delaySpin, ui_->waveformEdit,
              ui_->directivityCombo, ui_->orderSpin, ui_->pistonRadiusSpin, ui_->radiusSpin, ui_->activeCheck,
              ui_->physicalCheck, ui_->profileEdit},
             [this] { commit(); });
    refresh();
}

EmitterEditor::~EmitterEditor() = default;

void EmitterEditor::refresh()
{
    const SceneObject* current = object();
    const auto* emitter = current ? componentOf<EmitterComponent>(*current) : nullptr;
    if (!emitter)
        return;
    updating_ = true;
    const EmitterSettings& s = emitter->settings;
    ui_->frequencySpin->setValue(s.frequencyHz);
    ui_->amplitudeSpin->setValue(s.amplitudePa);
    show(ui_->phaseSpin, s.phaseRad, kDegrees);
    show(ui_->delaySpin, s.delaySec, kMicroseconds);
    ui_->waveformEdit->setText(QString::fromStdString(s.waveform));
    ui_->directivityCombo->setCurrentIndex(static_cast<int>(s.directivity.model));
    ui_->orderSpin->setValue(s.directivity.order);
    showLength(ui_->pistonRadiusSpin, s.directivity.pistonRadius);
    showLength(ui_->radiusSpin, s.radius);
    ui_->orderSpin->setEnabled(s.directivity.model == DirectivityModel::Cardioid);
    ui_->pistonRadiusSpin->setEnabled(s.directivity.model == DirectivityModel::BaffledPiston);
    ui_->activeCheck->setChecked(s.active);
    ui_->physicalCheck->setChecked(s.physicalTransducer);
    ui_->profileEdit->setText(QString::fromStdString(s.transducerProfile));
    updating_ = false;
}

void EmitterEditor::commit()
{
    const SceneObject* current = object();
    const auto* emitter = current ? componentOf<EmitterComponent>(*current) : nullptr;
    if (updating_ || !emitter)
        return;
    EmitterSettings s = emitter->settings;
    take(s.frequencyHz, ui_->frequencySpin, 1.0);
    take(s.amplitudePa, ui_->amplitudeSpin, 1.0);
    take(s.phaseRad, ui_->phaseSpin, kDegrees);
    take(s.delaySec, ui_->delaySpin, kMicroseconds);
    s.waveform = ui_->waveformEdit->text().toStdString();
    s.directivity.model = static_cast<DirectivityModel>(ui_->directivityCombo->currentIndex());
    take(s.directivity.order, ui_->orderSpin, 1.0);
    takeLength(s.directivity.pistonRadius, ui_->pistonRadiusSpin);
    takeLength(s.radius, ui_->radiusSpin);
    s.active = ui_->activeCheck->isChecked();
    s.physicalTransducer = ui_->physicalCheck->isChecked();
    s.transducerProfile = ui_->profileEdit->text().toStdString();
    if (Status status = s.validate(); !status) {
        report(status);
        refresh();
        return;
    }
    report(controller_.setComponent(id_, std::make_shared<EmitterComponent>(s), QStringLiteral("Edit")));
    refresh();
}

// --- array --------------------------------------------------------------------------

ArrayEditor::ArrayEditor(Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), controller, units, parent), ui_(std::make_unique<Ui::ArrayEditor>())
{
    ui_->setupUi(this);
    ui_->elementTable->setMinimumHeight(220);
    onEdited(this,
             {ui_->layoutCombo, ui_->countXSpin, ui_->countYSpin, ui_->spacingXSpin, ui_->spacingYSpin,
              ui_->radiusSpin, ui_->facingCombo, ui_->frequencySpin, ui_->amplitudeSpin, ui_->phaseSpin,
              ui_->delaySpin, ui_->directivityCombo, ui_->elementRadiusSpin},
             [this] { commit(); });
    connect(ui_->elementTable, &QTableWidget::cellChanged, this, &ArrayEditor::onCellChanged);
    connect(ui_->clearOverridesButton, &QPushButton::clicked, this, [this] {
        const SceneObject* current = object();
        const auto* array = current ? componentOf<ArrayComponent>(*current) : nullptr;
        if (!array)
            return;
        ArraySettings s = array->settings;
        s.overrides.clear();
        report(controller_.setComponent(id_, std::make_shared<ArrayComponent>(s), QStringLiteral("Clear per-element settings of")));
    });
    refresh();
}

ArrayEditor::~ArrayEditor() = default;

void ArrayEditor::refresh()
{
    const SceneObject* current = object();
    const auto* array = current ? componentOf<ArrayComponent>(*current) : nullptr;
    if (!array)
        return;
    updating_ = true;
    const ArraySettings& s = array->settings;
    ui_->layoutCombo->setCurrentIndex(static_cast<int>(s.layout));
    const bool custom = s.layout == ArrayLayout::Custom;
    ui_->countXSpin->setValue(custom ? static_cast<int>(s.customPositions.size()) : s.countX);
    ui_->countYSpin->setValue(s.countY);
    showLength(ui_->spacingXSpin, s.spacingX);
    showLength(ui_->spacingYSpin, s.spacingY);
    showLength(ui_->radiusSpin, s.radius);
    showLength(ui_->elementRadiusSpin, s.element.radius);
    ui_->facingCombo->setCurrentIndex(static_cast<int>(s.facing));
    ui_->frequencySpin->setValue(s.element.frequencyHz);
    ui_->amplitudeSpin->setValue(s.element.amplitudePa);
    show(ui_->phaseSpin, s.element.phaseRad, kDegrees);
    show(ui_->delaySpin, s.element.delaySec, kMicroseconds);
    ui_->directivityCombo->setCurrentIndex(static_cast<int>(s.element.directivity.model));

    const bool rows = s.layout == ArrayLayout::RectangularPlanar || s.layout == ArrayLayout::Cylindrical;
    const bool ring = s.layout == ArrayLayout::Circular || s.layout == ArrayLayout::Cylindrical || s.layout == ArrayLayout::Spherical;
    ui_->countYSpin->setEnabled(rows);
    ui_->spacingXSpin->setEnabled(s.layout == ArrayLayout::Linear || s.layout == ArrayLayout::RectangularPlanar);
    ui_->spacingYSpin->setEnabled(rows);
    ui_->radiusSpin->setEnabled(ring);
    ui_->facingCombo->setEnabled(ring || custom);

    const auto elements = generateElements(s);
    const int shown = std::min<int>(static_cast<int>(elements.size()), kMaxRows);
    ui_->elementsLabel->setText(static_cast<int>(elements.size()) > shown
                                    ? QStringLiteral("Elements (first %1 of %2 shown)").arg(shown).arg(elements.size())
                                    : QStringLiteral("Elements (%1)").arg(elements.size()));
    ui_->elementTable->setRowCount(shown);
    const QString unit = units_.lengthSymbol();
    ui_->elementTable->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("X (%1)").arg(unit),
                                                  QStringLiteral("Y (%1)").arg(unit), QStringLiteral("Z (%1)").arg(unit),
                                                  QStringLiteral("Amplitude (Pa)"), QStringLiteral("Phase (deg)"),
                                                  QStringLiteral("Delay (us)"), QStringLiteral("Active")});
    for (int row = 0; row < shown; ++row) {
        const ArrayElement& e = elements[static_cast<size_t>(row)];
        const auto cell = [&](int column, const QString& text, bool editable) {
            auto* item = new QTableWidgetItem(text);
            if (!editable)
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui_->elementTable->setItem(row, column, item);
        };
        cell(0, QString::number(e.index), false);
        for (int axis = 0; axis < 3; ++axis)
            cell(1 + axis, QString::number(units_.lengthFromSI(e.localPosition[axis]), 'g', 8), custom);
        cell(4, QString::number(e.settings.amplitudePa, 'g', 8), true);
        cell(5, QString::number(e.settings.phaseRad * kDegrees, 'g', 8), true);
        cell(6, QString::number(e.settings.delaySec * kMicroseconds, 'g', 8), true);
        auto* active = new QTableWidgetItem();
        active->setFlags((active->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        active->setCheckState(e.settings.active ? Qt::Checked : Qt::Unchecked);
        ui_->elementTable->setItem(row, 7, active);
    }
    ui_->elementTable->resizeColumnsToContents();
    updating_ = false;
}

void ArrayEditor::commit()
{
    const SceneObject* current = object();
    const auto* array = current ? componentOf<ArrayComponent>(*current) : nullptr;
    if (updating_ || !array)
        return;
    ArraySettings s = array->settings;
    s.layout = static_cast<ArrayLayout>(ui_->layoutCombo->currentIndex());
    if (s.layout == ArrayLayout::Custom) {
        // The count sets how many manually placed elements there are; new
        // ones start beside the last one.
        const auto wanted = static_cast<size_t>(ui_->countXSpin->value());
        while (s.customPositions.size() < wanted)
            s.customPositions.push_back(s.customPositions.empty() ? Vec3::Zero()
                                                                  : Vec3(s.customPositions.back() + Vec3(s.spacingX, 0, 0)));
        s.customPositions.resize(wanted);
    } else {
        s.countX = ui_->countXSpin->value();
    }
    s.countY = ui_->countYSpin->value();
    takeLength(s.spacingX, ui_->spacingXSpin);
    takeLength(s.spacingY, ui_->spacingYSpin);
    takeLength(s.radius, ui_->radiusSpin);
    takeLength(s.element.radius, ui_->elementRadiusSpin);
    s.facing = static_cast<ElementFacing>(ui_->facingCombo->currentIndex());
    take(s.element.frequencyHz, ui_->frequencySpin, 1.0);
    take(s.element.amplitudePa, ui_->amplitudeSpin, 1.0);
    take(s.element.phaseRad, ui_->phaseSpin, kDegrees);
    take(s.element.delaySec, ui_->delaySpin, kMicroseconds);
    s.element.directivity.model = static_cast<DirectivityModel>(ui_->directivityCombo->currentIndex());
    if (Status status = s.validate(); !status) {
        report(status);
        refresh();
        return;
    }
    report(controller_.setComponent(id_, std::make_shared<ArrayComponent>(s), QStringLiteral("Edit")));
    refresh();
}

void ArrayEditor::onCellChanged(int row, int column)
{
    const SceneObject* current = object();
    const auto* array = current ? componentOf<ArrayComponent>(*current) : nullptr;
    if (updating_ || !array)
        return;
    ArraySettings s = array->settings;
    const QTableWidgetItem* item = ui_->elementTable->item(row, column);
    bool ok = true;
    const double value = column == 7 ? 0.0 : item->text().toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        report(Status::failure("enter a number"));
        refresh();
        return;
    }
    if (column >= 1 && column <= 3) {
        if (s.layout == ArrayLayout::Custom && row < static_cast<int>(s.customPositions.size()))
            s.customPositions[static_cast<size_t>(row)][column - 1] = units_.lengthToSI(value);
    } else {
        ElementOverride& o = s.overrides[row];
        switch (column) {
        case 4: o.amplitudePa = value; break;
        case 5: o.phaseRad = value / kDegrees; break;
        case 6: o.delaySec = value / kMicroseconds; break;
        case 7: o.active = item->checkState() == Qt::Checked; break;
        default: break;
        }
        if ((o.amplitudePa && *o.amplitudePa < 0.0) || (o.delaySec && *o.delaySec < 0.0)) {
            report(Status::failure("amplitude and delay must not be negative"));
            refresh();
            return;
        }
    }
    report(controller_.setComponent(id_, std::make_shared<ArrayComponent>(s),
                                    QStringLiteral("Edit element %1 of").arg(row)));
    refresh();
}

// --- target ---------------------------------------------------------------------------

TargetEditor::TargetEditor(Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), controller, units, parent), ui_(std::make_unique<Ui::TargetEditor>())
{
    ui_->setupUi(this);
    onEdited(this, {ui_->kindCombo, ui_->amplitudeSpin, ui_->phaseSpin, ui_->weightSpin, ui_->suppressCheck},
             [this] { commit(); });
    refresh();
}

TargetEditor::~TargetEditor() = default;

void TargetEditor::refresh()
{
    const SceneObject* current = object();
    const auto* target = current ? componentOf<TargetComponent>(*current) : nullptr;
    if (!target)
        return;
    updating_ = true;
    const TargetSettings& s = target->settings;
    ui_->kindCombo->setCurrentIndex(static_cast<int>(s.kind));
    ui_->amplitudeSpin->setValue(s.desiredAmplitudePa);
    show(ui_->phaseSpin, s.desiredPhaseRad, kDegrees);
    ui_->weightSpin->setValue(s.weight);
    ui_->suppressCheck->setChecked(s.suppress);
    ui_->amplitudeSpin->setEnabled(!s.suppress);
    ui_->phaseSpin->setEnabled(!s.suppress);
    updating_ = false;
}

void TargetEditor::commit()
{
    const SceneObject* current = object();
    const auto* target = current ? componentOf<TargetComponent>(*current) : nullptr;
    if (updating_ || !target)
        return;
    TargetSettings s = target->settings;
    s.kind = static_cast<TargetKind>(ui_->kindCombo->currentIndex());
    take(s.desiredAmplitudePa, ui_->amplitudeSpin, 1.0);
    take(s.desiredPhaseRad, ui_->phaseSpin, kDegrees);
    take(s.weight, ui_->weightSpin, 1.0);
    s.suppress = ui_->suppressCheck->isChecked();
    if (Status status = s.validate(); !status) {
        report(status);
        refresh();
        return;
    }
    report(controller_.setComponent(id_, std::make_shared<TargetComponent>(s), QStringLiteral("Edit")));
    refresh();
}

// --- region -----------------------------------------------------------------------------

RegionEditor::RegionEditor(Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent)
    : ComponentEditor(std::move(id), controller, units, parent), ui_(std::make_unique<Ui::RegionEditor>())
{
    ui_->setupUi(this);
    onEdited(this,
             {ui_->kindCombo, ui_->sizeXSpin, ui_->sizeYSpin, ui_->sizeZSpin, ui_->countUSpin, ui_->countVSpin,
              ui_->countWSpin},
             [this] { commit(); });
    refresh();
}

RegionEditor::~RegionEditor() = default;

void RegionEditor::refresh()
{
    const SceneObject* current = object();
    const auto* region = current ? componentOf<RegionComponent>(*current) : nullptr;
    if (!region)
        return;
    updating_ = true;
    const RegionSettings& s = region->settings;
    ui_->kindCombo->setCurrentIndex(static_cast<int>(s.kind));
    showLength(ui_->sizeXSpin, s.size.x());
    showLength(ui_->sizeYSpin, s.size.y());
    showLength(ui_->sizeZSpin, s.size.z());
    ui_->countUSpin->setValue(s.countU);
    ui_->countVSpin->setValue(s.countV);
    ui_->countWSpin->setValue(s.countW);
    ui_->sizeXSpin->setEnabled(s.kind != RegionKind::PlaneYZ);
    ui_->sizeYSpin->setEnabled(s.kind != RegionKind::PlaneXZ);
    ui_->sizeZSpin->setEnabled(s.kind != RegionKind::PlaneXY);
    ui_->countWSpin->setEnabled(s.isVolume());
    updating_ = false;
}

void RegionEditor::commit()
{
    const SceneObject* current = object();
    const auto* region = current ? componentOf<RegionComponent>(*current) : nullptr;
    if (updating_ || !region)
        return;
    RegionSettings s = region->settings;
    s.kind = static_cast<RegionKind>(ui_->kindCombo->currentIndex());
    takeLength(s.size.x(), ui_->sizeXSpin);
    takeLength(s.size.y(), ui_->sizeYSpin);
    takeLength(s.size.z(), ui_->sizeZSpin);
    s.countU = ui_->countUSpin->value();
    s.countV = ui_->countVSpin->value();
    s.countW = ui_->countWSpin->value();
    if (Status status = s.validate(); !status) {
        report(status);
        refresh();
        return;
    }
    report(controller_.setComponent(id_, std::make_shared<RegionComponent>(s), QStringLiteral("Edit")));
    refresh();
}

// --- factory -------------------------------------------------------------------------------

PropertiesPanel::ComponentEditorFactory makeAcousticEditorFactory(AcousticController& controller)
{
    return [&controller](const SceneObject& object, DocumentController&, DisplayUnits& units, QWidget* parent) -> QWidget* {
        if (componentOf<EmitterComponent>(object))
            return new EmitterEditor(object.id(), controller, units, parent);
        if (componentOf<ArrayComponent>(object))
            return new ArrayEditor(object.id(), controller, units, parent);
        if (componentOf<TargetComponent>(object))
            return new TargetEditor(object.id(), controller, units, parent);
        if (componentOf<RegionComponent>(object))
            return new RegionEditor(object.id(), controller, units, parent);
        return nullptr;
    };
}
