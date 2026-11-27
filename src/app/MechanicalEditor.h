#pragma once

// Property editor of the mechanical components: structure idealization,
// actuator, sensor and vibration target. Laid out in forms/MechanicalEditor.ui.
#include "AcousticEditors.h"
#include "PropertiesPanel.h"

class MechanicsController;

namespace Ui {
class MechanicalEditor;
}

class MechanicalEditor final : public ComponentEditor {
    Q_OBJECT
public:
    MechanicalEditor(resonant::Id id, AcousticController& acoustics, MechanicsController& mechanics, DisplayUnits& units, QWidget* parent);
    ~MechanicalEditor() override;

    QString studyText() const;

private:
    void refresh() override;
    void commit();
    void fillStructures(class QComboBox* combo, const resonant::Id& selected);

    std::unique_ptr<Ui::MechanicalEditor> ui_;
    MechanicsController& mechanics_;
};

PropertiesPanel::ComponentEditorFactory makeMechanicalEditorFactory(AcousticController& acoustics, MechanicsController& mechanics);
