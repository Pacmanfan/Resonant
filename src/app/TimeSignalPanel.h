#pragma once

// Recorded signals of full-wave runs: pressure over time at the probes,
// their spectra with what the record can resolve, the energy in the domain,
// and the recorded instants of the field. Laid out in
// forms/TimeSignalPanel.ui.
#include <QWidget>

#include <memory>

class FdtdController;
class PlotWidget;

namespace Ui {
class TimeSignalPanel;
}

class TimeSignalPanel : public QWidget {
    Q_OBJECT
public:
    explicit TimeSignalPanel(QWidget* parent = nullptr);
    ~TimeSignalPanel() override;

    void setController(FdtdController* controller);

    enum View { PressureOverTime, SpectrumMagnitude, SpectrumPhase, EnergyOverTime };
    void setView(View view);
    void selectProbe(int index);
    // Shows recorded instant `index` of the selected run in the viewport.
    bool showSnapshot(int index);
    PlotWidget& plot();
    QString infoText() const;
    int resultCount() const;
    int probeCount() const;
    int snapshotCount() const;

public slots:
    void refresh();

private:
    void refreshProbes();
    void refreshPlot();

    std::unique_ptr<Ui::TimeSignalPanel> ui_;
    FdtdController* controller_ = nullptr;
};
