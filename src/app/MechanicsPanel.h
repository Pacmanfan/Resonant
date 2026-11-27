#pragma once

// The Mechanics panel: structure, preflight, analyses, targeting, band
// structure, sound radiation and the display of results. Laid out in
// forms/MechanicsPanel.ui.
#include "foundation/Id.h"

#include <QTimer>
#include <QWidget>

#include <memory>
#include <optional>

class AcousticController;
class MechanicsController;
class PlotWidget;

namespace resonant::mechanics {
struct TargetingSolution;
}

namespace Ui {
class MechanicsPanel;
}

class MechanicsPanel : public QWidget {
    Q_OBJECT
public:
    explicit MechanicsPanel(QWidget* parent = nullptr);
    ~MechanicsPanel() override;

    void setControllers(MechanicsController* mechanics, AcousticController* acoustics);

    void selectStructure(const resonant::Id& id);
    resonant::Id selectedStructure() const;
    bool runModal();
    bool runHarmonic();
    bool runTransient();
    bool runStatic();
    bool solveTargets();
    bool applyTargets();
    bool showBandStructure();
    bool radiate();
    void showTab(const QString& objectName);

    QString studyText() const;
    QString statusText() const;
    QString displayText() const;
    QString targetText() const;
    QString frfNote() const;
    int modeRowCount() const;
    QString modeCell(int row, int column) const;
    PlotWidget& frfPlot();
    PlotWidget& transientPlot();
    PlotWidget& bandPlot();

public slots:
    void check();

private:
    void refreshStructures();
    void refreshSettings();
    void refreshResults();
    void refreshItems();
    void refreshPlots();
    void refreshRegions();
    void commit();
    void updateEnabled();
    bool startRun(int analysis);

    std::unique_ptr<Ui::MechanicsPanel> ui_;
    MechanicsController* mechanics_ = nullptr;
    AcousticController* acoustics_ = nullptr;
    QTimer checkTimer_;
    bool updating_ = false;
    std::unique_ptr<resonant::mechanics::TargetingSolution> targeting_;
};
