// Property editor of a wave manipulator: boundary condition, design intent
// (intended focus and source), what is calculated for it, analytical
// estimates, and for reflectors a one-parameter geometry optimization.
#pragma once

#include "AcousticEditors.h"
#include "manipulators/GeometryOptimization.h"

#include <optional>

class ManipulatorController;
namespace Ui {
class ManipulatorEditor;
}

class ManipulatorEditor final : public ComponentEditor {
    Q_OBJECT
public:
    ManipulatorEditor(resonant::Id id, AcousticController& acoustics, ManipulatorController& manipulators,
                      DisplayUnits& units, QWidget* parent);
    ~ManipulatorEditor() override;

    // The operations behind the two buttons, for menus and tests.
    bool optimize();
    bool applyBest();
    QString optimizeResultText() const;
    QString estimates() const;

private:
    void refresh() override;
    void commit();

    std::unique_ptr<Ui::ManipulatorEditor> ui_;
    ManipulatorController& manipulators_;
    std::optional<resonant::manipulators::OptimizationOutcome> outcome_;
};

PropertiesPanel::ComponentEditorFactory makeManipulatorEditorFactory(AcousticController& acoustics,
                                                                     ManipulatorController& manipulators);
