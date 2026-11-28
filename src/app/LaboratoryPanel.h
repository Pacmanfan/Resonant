// The "Laboratory" panel (REQ-CHAT-20261126-023, RES-LAB-017): a study with its objective,
// models (and their staleness), deviations and datasets; the study's files imported with an
// import report; datasets plotted with their metadata; a comparison with a model, overlaid
// and with its residual; a calibration of physical parameters (a job) with estimates,
// intervals and identifiability, kept, validated on the study's validation data and applied
// only on request; the study's report and an evidence summary.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class LaboratoryController;
namespace Ui {
class LaboratoryPanel;
}
class QPushButton;

class LaboratoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit LaboratoryPanel(QWidget* parent = nullptr);
    ~LaboratoryPanel() override;

    void setController(LaboratoryController* controller);

    // For tests.
    int studyCount() const;
    void selectStudy(const QString& id);
    QPushButton* importStudyButton() const;
    QPushButton* compareButton() const;
    QPushButton* calibrateButton() const;
    QPushButton* keepButton() const;
    QPushButton* validateButton() const;
    QPushButton* reportButton() const;
    QPushButton* explainButton() const;
    int datasetCount() const;
    void selectDataset(int row);
    int plottedPoints() const;
    int issueRows() const;
    QString studyText() const;
    QString verdictText() const;
    int comparisonCurves() const;
    int residualPoints() const;
    void setParameterEnabled(const QString& id, bool enabled);
    int estimateRows() const;
    QString estimate(const QString& id) const;  // "<estimate> [<lower>, <upper>] <identifiability>"
    QString calibrationText() const;
    QString reportText() const;
    QString statusText() const;

public slots:
    void refresh();

private:
    void showStudy();
    void showDataset();
    void fillParameters();
    nlohmann::json currentStudy() const;
    nlohmann::json modelSpec() const;  // the study's {"id", "configuration"}
    std::vector<std::string> datasetsWithRole(const std::string& role) const;

    std::unique_ptr<Ui::LaboratoryPanel> ui_;
    LaboratoryController* controller_ = nullptr;
    QString resultId_, calibrationId_, lastRecord_;
    nlohmann::json lastCalibration_ = nlohmann::json::object();
};
