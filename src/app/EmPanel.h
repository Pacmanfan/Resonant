#pragma once

// The Electromagnetics panel: the selected antenna, array, source or guide
// and its report; impedance sweeps and measured one-port data; radiation
// patterns (2D cuts and a 3D surface); array steering, tapering and beam
// synthesis; fields on regions (analytical or full-wave) and probes; a
// transmission-line calculator; and the display of field results. Laid out
// in forms/EmPanel.ui.
#include "foundation/Id.h"

#include <QWidget>

#include <array>
#include <memory>
#include <vector>

class EmController;
class PlotWidget;

namespace Ui {
class EmPanel;
}

class EmPanel : public QWidget {
    Q_OBJECT
public:
    explicit EmPanel(QWidget* parent = nullptr);
    ~EmPanel() override;

    void setController(EmController* controller);

    void selectObject(const resonant::Id& id);
    resonant::Id selectedObject() const;
    void selectRegion(const resonant::Id& id);
    void showTab(const QString& objectName);

    bool analyse();
    bool calculateSweep();
    bool calculatePattern();
    bool showPattern3D();
    bool steer();
    bool applyChebyshev();
    bool analyseArray();
    bool synthesize(bool apply);
    bool evaluateRegion();
    bool showGuideField();
    bool probe();
    bool planFullWave();
    bool runFullWave();
    bool calculateLine();

    QString reportText() const;
    QString patternText() const;
    QString arrayText() const;
    QString planText() const;
    QString probeText() const;
    QString lineText() const;
    QString displayText() const;
    PlotWidget& sweepPlot();
    PlotWidget& patternPlot();
    PlotWidget& arrayPlot();

private:
    void refreshObjects();
    void refreshRegions();
    void refreshSettings();
    void refreshResults();
    void refreshMeasurements();
    void refreshDisplay();
    void updateEnabled();
    void commitSettings();
    void commitFullWave();
    void plotSweep();
    void fail(const QString& text);

    std::unique_ptr<Ui::EmPanel> ui_;
    EmController* controller_ = nullptr;
    bool updating_ = false;
    struct SweepPoint {
        double frequencyHz, resistance, reactance, reflection;  // ohm, ohm, |Gamma|
    };
    std::vector<SweepPoint> lastSweep_;
};
