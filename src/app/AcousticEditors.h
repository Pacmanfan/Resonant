// Property editors of the acoustic components, shown in the Properties
// panel under the object's common properties. Each is laid out in a Qt
// Designer form and edits one component through the AcousticController, so
// every change is an undoable command.
#pragma once

#include "PropertiesPanel.h"
#include "acoustics/Components.h"

#include <QWidget>

#include <memory>

class AcousticController;
class DisplayUnits;
class QDoubleSpinBox;
namespace Ui {
class EmitterEditor;
class ArrayEditor;
class TargetEditor;
class RegionEditor;
}  // namespace Ui

// Common behaviour: follow the object, refresh when it or the display unit
// changes, and never write back while refreshing.
class ComponentEditor : public QWidget {
    Q_OBJECT
public:
    ComponentEditor(resonant::Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent);

protected:
    virtual void refresh() = 0;
    const resonant::SceneObject* object() const;
    void report(const resonant::Status& status);
    // Copies a spin box's value into `field` (value / factor) only when the
    // user changed it, so untouched fields keep their full precision.
    void take(double& field, const QDoubleSpinBox* spin, double factor) const;
    void show(QDoubleSpinBox* spin, double value, double factor) const;
    // Length spin boxes follow the display unit.
    void showLength(QDoubleSpinBox* spin, double meters) const;
    void takeLength(double& meters, const QDoubleSpinBox* spin) const;

    resonant::Id id_;
    AcousticController& controller_;
    DisplayUnits& units_;
    bool updating_ = false;
};

class EmitterEditor final : public ComponentEditor {
    Q_OBJECT
public:
    EmitterEditor(resonant::Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent);
    ~EmitterEditor() override;

private:
    void refresh() override;
    void commit();
    std::unique_ptr<Ui::EmitterEditor> ui_;
};

class ArrayEditor final : public ComponentEditor {
    Q_OBJECT
public:
    ArrayEditor(resonant::Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent);
    ~ArrayEditor() override;

    // The table shows at most this many elements; larger arrays are edited
    // through the array parameters, focusing and inverse design.
    static constexpr int kMaxRows = 512;

private:
    void refresh() override;
    void commit();
    void onCellChanged(int row, int column);
    std::unique_ptr<Ui::ArrayEditor> ui_;
};

class TargetEditor final : public ComponentEditor {
    Q_OBJECT
public:
    TargetEditor(resonant::Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent);
    ~TargetEditor() override;

private:
    void refresh() override;
    void commit();
    std::unique_ptr<Ui::TargetEditor> ui_;
};

class RegionEditor final : public ComponentEditor {
    Q_OBJECT
public:
    RegionEditor(resonant::Id id, AcousticController& controller, DisplayUnits& units, QWidget* parent);
    ~RegionEditor() override;

private:
    void refresh() override;
    void commit();
    std::unique_ptr<Ui::RegionEditor> ui_;
};

// The factory the Properties panel uses to build these editors.
PropertiesPanel::ComponentEditorFactory makeAcousticEditorFactory(AcousticController& controller);
