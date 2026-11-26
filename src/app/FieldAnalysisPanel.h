// The "Field Analysis" tab of the analysis area: what the displayed result
// is (frequency, medium, sources, sampling, peak, time, assumptions) and the
// field quantities at the scene's probes and targets, with export.
#pragma once

#include <QWidget>

#include <memory>

class AcousticController;
class DisplayUnits;
namespace Ui {
class FieldAnalysisPanel;
}

class FieldAnalysisPanel : public QWidget {
    Q_OBJECT
public:
    explicit FieldAnalysisPanel(QWidget* parent = nullptr);
    ~FieldAnalysisPanel() override;

    void setController(AcousticController* controller, DisplayUnits* units);

    // For tests: "Quantity=Value" lines of the summary, and the point table.
    QStringList summaryItems() const;
    int pointRowCount() const;
    QString pointCell(int row, int column) const;

public slots:
    void refresh();

private:
    std::unique_ptr<Ui::FieldAnalysisPanel> ui_;
    AcousticController* controller_ = nullptr;
    DisplayUnits* units_ = nullptr;
};
