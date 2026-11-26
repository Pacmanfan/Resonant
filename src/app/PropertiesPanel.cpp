#include "PropertiesPanel.h"

#include "DisplayUnits.h"
#include "DocumentController.h"
#include "ObjectLabels.h"
#include "ui_PropertiesPanel.h"

#include <QDoubleSpinBox>
#include <QLabel>

using namespace resonant;

namespace {
enum TransformGroup { Position = 0, Rotation = 1, Scale = 2 };

void clearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}
}  // namespace

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::PropertiesPanel>())
{
    ui_->setupUi(this);
    ui_->stack->setCurrentWidget(ui_->emptyPage);

    QDoubleSpinBox* spins[3][3] = {{ui_->positionX, ui_->positionY, ui_->positionZ},
                                   {ui_->rotationX, ui_->rotationY, ui_->rotationZ},
                                   {ui_->scaleX, ui_->scaleY, ui_->scaleZ}};
    for (int group = 0; group < 3; ++group)
        for (int axis = 0; axis < 3; ++axis)
            connectTransformSpin(spins[group][axis], group, axis);

    connect(ui_->nameEdit, &QLineEdit::editingFinished, this, [this] {
        if (updating_ || !controller_)
            return;
        report(controller_->renameObject(shown_, ui_->nameEdit->text()));
        refreshValues();
    });
    connect(ui_->visibleCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!updating_ && controller_)
            report(controller_->setVisible(shown_, checked));
    });
    connect(ui_->materialCombo, &QComboBox::activated, this, [this](int index) {
        if (!updating_ && controller_)
            report(controller_->setMaterial(shown_, Id(ui_->materialCombo->itemData(index).toString().toStdString())));
    });
}

PropertiesPanel::~PropertiesPanel() = default;

void PropertiesPanel::setController(DocumentController* controller, DisplayUnits* units)
{
    controller_ = controller;
    units_ = units;
    connect(controller_, &DocumentController::selectionChanged, this, &PropertiesPanel::showSelection);
    connect(controller_, &DocumentController::projectReplaced, this, &PropertiesPanel::showSelection);
    connect(controller_, &DocumentController::materialsChanged, this, [this] {
        refreshMaterialChoices();
        refreshValues();
    });
    connect(controller_, &DocumentController::sceneChanged, this, [this](const SceneEvent& event) {
        if (event.kind == SceneEvent::Kind::Changed && event.id == shown_)
            refreshValues();
    });
    connect(units_, &DisplayUnits::changed, this, [this] {
        shownGeometryKind_.clear();  // rebuild the editors with the new unit
        refreshValues();
    });
    refreshMaterialChoices();
    showSelection();
}

void PropertiesPanel::report(const Status& status)
{
    if (!status)
        emit operationFailed(QString::fromStdString(status.message()));
}

void PropertiesPanel::connectTransformSpin(QDoubleSpinBox* spin, int group, int axis)
{
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, group, axis](double value) {
        if (updating_ || !controller_)
            return;
        const SceneObject* object = controller_->scene().find(shown_);
        if (!object)
            return;
        // Change only the edited component, so the other eight keep their
        // full stored precision.
        Transform transform = object->state.local;
        QString verb;
        switch (group) {
        case Position:
            transform.position[axis] = units_->lengthToSI(value);
            verb = QStringLiteral("Move");
            break;
        case Rotation:
            transform.rotation[axis] = units::degreesToRadians(value);
            verb = QStringLiteral("Rotate");
            break;
        default:
            transform.scale[axis] = value;
            verb = QStringLiteral("Scale");
            break;
        }
        report(controller_->setTransform(shown_, transform, verb));
        refreshValues();
    });
}

void PropertiesPanel::refreshMaterialChoices()
{
    if (!controller_)
        return;
    const bool wasUpdating = updating_;
    updating_ = true;
    ui_->materialCombo->clear();
    ui_->materialCombo->addItem(QStringLiteral("(none)"), QString());
    for (const Material& material : controller_->project().materials.materials())
        ui_->materialCombo->addItem(QString::fromStdString(material.name()), QString::fromStdString(material.id().str()));
    updating_ = wasUpdating;
}

void PropertiesPanel::showSelection()
{
    if (!controller_)
        return;
    const Id id = controller_->primarySelection();
    shown_ = controller_->scene().find(id) ? id : Id();
    shownGeometryKind_.clear();
    shownComponentType_ = "\x01";  // force the component editors to be rebuilt
    refreshValues();
}

void PropertiesPanel::rebuildGeometryEditors(const SceneObject& object)
{
    clearLayout(ui_->geometryForm);
    geometrySpins_.clear();
    shownGeometryKind_ = object.state.geometry ? object.state.geometry->kindId() : std::string();
    ui_->geometryGroup->setVisible(object.state.geometry.has_value());
    if (!object.state.geometry)
        return;

    const GeometryKind& kind = object.state.geometry->kind();
    ui_->geometryGroup->setTitle(QStringLiteral("Geometry - ") + QString::fromStdString(kind.displayName));
    for (const ParameterDef& def : kind.parameters) {
        const QString siUnit = QString::fromStdString(def.unit);
        auto* spin = new QDoubleSpinBox(ui_->geometryGroup);
        spin->setKeyboardTracking(false);
        spin->setDecimals(def.integer ? 0 : 5);
        spin->setRange(units_->fromSI(def.minValue, siUnit), units_->fromSI(def.maxValue, siUnit));
        spin->setSingleStep(def.integer ? 1.0 : units_->fromSI(def.defaultValue, siUnit) / 10.0);
        const QString symbol = units_->symbolFor(siUnit);
        if (!symbol.isEmpty())
            spin->setSuffix(QLatin1Char(' ') + symbol);
        const std::string key = def.key;
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key, siUnit](double value) {
            if (updating_ || !controller_)
                return;
            report(controller_->setGeometryParameter(shown_, key, units_->toSI(value, siUnit)));
            refreshValues();  // shows the stored value again if the edit was refused
        });
        ui_->geometryForm->addRow(QString::fromStdString(def.label), spin);
        geometrySpins_.emplace_back(def.key, spin);
    }
}

void PropertiesPanel::rebuildComponentEditors(const SceneObject& object)
{
    clearLayout(ui_->componentLayout);
    shownComponentType_ = object.state.component ? object.state.component->typeId() : std::string();
    for (const ComponentEditorFactory& factory : factories_)
        if (QWidget* editor = factory(object, *controller_, *units_, ui_->componentContainer))
            ui_->componentLayout->addWidget(editor);
}

void PropertiesPanel::refreshValues()
{
    if (!controller_)
        return;
    const SceneObject* object = controller_->scene().find(shown_);
    if (!object) {
        shown_ = Id();
        ui_->stack->setCurrentWidget(ui_->emptyPage);
        const qsizetype count = controller_->selection().size();
        ui_->emptyLabel->setText(count > 1 ? QStringLiteral("%1 objects selected.").arg(count)
                                           : QStringLiteral("Select an object to see its properties."));
        return;
    }
    updating_ = true;
    ui_->stack->setCurrentWidget(ui_->objectPage);

    const qsizetype count = controller_->selection().size();
    ui_->objectGroup->setTitle(count > 1 ? QStringLiteral("Object (last of %1 selected)").arg(count)
                                         : QStringLiteral("Object"));
    ui_->nameEdit->setText(QString::fromStdString(object->state.name));
    ui_->typeValue->setText(objectTypeLabel(*object));
    ui_->idValue->setText(QString::fromStdString(object->id().str()));
    ui_->visibleCheck->setChecked(object->state.visible);
    const int materialIndex = ui_->materialCombo->findData(QString::fromStdString(object->state.materialId.str()));
    ui_->materialCombo->setCurrentIndex(std::max(0, materialIndex));

    const Transform& t = object->state.local;
    const QString lengthSuffix = QLatin1Char(' ') + units_->lengthSymbol();
    QDoubleSpinBox* position[3] = {ui_->positionX, ui_->positionY, ui_->positionZ};
    QDoubleSpinBox* rotation[3] = {ui_->rotationX, ui_->rotationY, ui_->rotationZ};
    QDoubleSpinBox* scale[3] = {ui_->scaleX, ui_->scaleY, ui_->scaleZ};
    for (int axis = 0; axis < 3; ++axis) {
        position[axis]->setSuffix(lengthSuffix);
        position[axis]->setValue(units_->lengthFromSI(t.position[axis]));
        rotation[axis]->setValue(units::radiansToDegrees(t.rotation[axis]));
        scale[axis]->setValue(t.scale[axis]);
    }

    const std::string kind = object->state.geometry ? object->state.geometry->kindId() : std::string();
    if (kind != shownGeometryKind_ || (object->state.geometry && geometrySpins_.empty()))
        rebuildGeometryEditors(*object);
    ui_->geometryGroup->setVisible(object->state.geometry.has_value());
    if (object->state.geometry) {
        for (const auto& [key, spin] : geometrySpins_) {
            const ParameterDef* def = object->state.geometry->kind().findParameter(key);
            spin->setValue(units_->fromSI(object->state.geometry->parameter(key), QString::fromStdString(def->unit)));
        }
    }

    const std::string componentType = object->state.component ? object->state.component->typeId() : std::string();
    if (componentType != shownComponentType_)
        rebuildComponentEditors(*object);
    updating_ = false;
}
