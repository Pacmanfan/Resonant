// The Acoustics dock: medium and model, focusing, inverse design, field
// simulation and display. Laid out in forms/AcousticsPanel.ui; all work is
// done by the AcousticController.
#pragma once

#include "AcousticController.h"

#include <QWidget>

#include <memory>
#include <optional>

class DisplayUnits;
namespace Ui {
class AcousticsPanel;
}

class AcousticsPanel : public QWidget {
    Q_OBJECT
public:
    explicit AcousticsPanel(QWidget* parent = nullptr);
    ~AcousticsPanel() override;

    void setController(AcousticController* controller, DisplayUnits* units);

    // The operations behind the buttons, callable from menus and tests.
    void showTab(const QString& objectName);
    bool calculateFocus();   // fills the table; false when it could not
    bool applyFocus();
    bool solveInverse();
    bool applyInverse();
    bool runSimulation();
    void selectRegion(const resonant::Id& id);
    void selectFocusArray(const resonant::Id& id);
    void selectFocusTarget(const resonant::Id& id);
    void selectInverseArray(const resonant::Id& id);
    QString estimateText() const;
    QString inverseSummaryText() const;

private:
    void refreshObjectLists();
    void refreshMedium();
    void commitMedium();
    void refreshEstimate();
    void refreshResults();
    void refreshDisplay();
    void updateFocusPosition();
    resonant::Id comboId(const class QComboBox* combo) const;
    void say(const QString& text);

    std::unique_ptr<Ui::AcousticsPanel> ui_;
    AcousticController* controller_ = nullptr;
    DisplayUnits* units_ = nullptr;
    bool updating_ = false;
    std::optional<resonant::acoustics::FocusSolution> focus_;
    resonant::Id focusArray_;
    std::optional<AcousticController::InverseRun> inverse_;
    resonant::Id inverseArray_;
};
