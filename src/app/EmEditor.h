#pragma once

// Property editor of the electromagnetic components: antenna feed, ideal
// source, RF array (layout and per-element excitation) and waveguide.
// Laid out in forms/EmEditor.ui.
#include "AcousticEditors.h"
#include "PropertiesPanel.h"

class EmController;

namespace Ui {
class EmEditor;
}

class EmEditor final : public ComponentEditor {
    Q_OBJECT
public:
    EmEditor(resonant::Id id, AcousticController& acoustics, EmController& em, DisplayUnits& units, QWidget* parent);
    ~EmEditor() override;

    QString antennaText() const;

private:
    void refresh() override;
    void commit();
    void commitElement(int row, int column);

    std::unique_ptr<Ui::EmEditor> ui_;
    EmController& em_;
};

PropertiesPanel::ComponentEditorFactory makeEmEditorFactory(AcousticController& acoustics, EmController& em);

// The type shown in the scene tree and the properties panel.
QString emTypeLabel(const resonant::SceneObject& object);
