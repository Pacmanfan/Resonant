// The "Functional Structures" panel (REQ-CHAT-20261126-017, RES-FUNC-015):
// the project's structures, their parameters (editing one changes the design,
// undoably), the analysis run as a job with its metrics, modes, model,
// assumptions and staleness, the result's curves and a one-parameter sweep.
// Switching what is shown never recomputes.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class FunctionalController;
namespace Ui {
class FunctionalPanel;
}
class QPushButton;

class FunctionalPanel : public QWidget {
    Q_OBJECT
public:
    explicit FunctionalPanel(QWidget* parent = nullptr);
    ~FunctionalPanel() override;

    void setController(FunctionalController* controller);

    // For tests.
    int structureCount() const;
    void selectStructure(const QString& id);
    QString currentStructure() const;
    int parameterRowCount() const;
    QString parameterId(int row) const;
    void editParameter(int row, double value);  // as typing a value into the table
    QString metricText(const QString& key) const;
    int modeRowCount() const;
    QString modeLabel(int row) const;
    QString modeCell(int row, int column) const;  // 1 nature, 2 frequency, 3 Q, 4 losses
    QString statusText() const;
    QString detailsText() const;
    QStringList curveChoices() const;
    void showCurve(int index);
    int plottedPoints() const;
    QPushButton* analyzeButton() const;
    QPushButton* saveMaterialButton() const;
    void startSweep(const QString& parameter, double from, double to, int points, const QString& metric);

public slots:
    void refresh();

private:
    void showParameters();
    void showResult(const QString& resultId);
    void showStaleness();
    void plotCurve();

    std::unique_ptr<Ui::FunctionalPanel> ui_;
    FunctionalController* controller_ = nullptr;
    nlohmann::json analysis_ = nlohmann::json::object();  // the last analysis result shown
    nlohmann::json sweep_;     // the last sweep result
    QString resultId_;
    bool filling_ = false;
};
