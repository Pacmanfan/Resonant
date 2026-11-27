#pragma once

// The full-wave solver panel: region, signal, resolution, boundaries, the
// solver assessment, the preflight and the run controls. Laid out in
// forms/FdtdPanel.ui.
#include "foundation/Id.h"

#include <QTimer>
#include <QWidget>

#include <memory>

class AcousticController;
class FdtdController;

namespace Ui {
class FdtdPanel;
}

class FdtdPanel : public QWidget {
    Q_OBJECT
public:
    explicit FdtdPanel(QWidget* parent = nullptr);
    ~FdtdPanel() override;

    void setControllers(AcousticController* acoustics, FdtdController* solver);

    void selectRegion(const resonant::Id& id);
    resonant::Id selectedRegion() const;
    // Starts a run of the selected region; false (with the reason in the
    // status line) when it cannot start.
    bool run();
    QString planText() const;
    QString assessmentText() const;
    QString statusText() const;

public slots:
    // Recalculates the plan and the assessment now.
    void check();

private:
    void refreshRegions();
    void refreshSettings();
    void commit();
    void updateEnabled();

    std::unique_ptr<Ui::FdtdPanel> ui_;
    AcousticController* acoustics_ = nullptr;
    FdtdController* solver_ = nullptr;
    QTimer checkTimer_;
    bool updating_ = false;
};
