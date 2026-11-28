// The "Material State" panel (REQ-CHAT-20261126-021, RES-DMS-014): the project's studies and
// the constitutive models' cards, a run as a job with its response curve, spatial profiles and
// state history, records saved into the project, and an evolved state committed to a material.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class MaterialStateController;
namespace Ui {
class MaterialStatePanel;
}
class QPushButton;

class MaterialStatePanel : public QWidget {
    Q_OBJECT
public:
    explicit MaterialStatePanel(QWidget* parent = nullptr);
    ~MaterialStatePanel() override;

    void setController(MaterialStateController* controller);

    // For tests.
    int studyCount() const;
    void selectStudy(const QString& id);
    void selectModel(const QString& id);
    QString cardText() const;
    QPushButton* runStudyButton() const;
    QPushButton* runModelButton() const;
    QPushButton* saveButton() const;
    QPushButton* commitButton() const;
    QStringList curveChoices() const;
    void showCurve(int index);
    int plottedPoints() const;
    int historyRowCount() const;
    int snapshotCount() const;
    void chooseTarget(const QString& materialId);
    int recordCount() const;
    QString statusText() const;

public slots:
    void refresh();

private:
    void showCard();
    void showResult(const QString& resultId);
    void plotCurve();

    std::unique_ptr<Ui::MaterialStatePanel> ui_;
    MaterialStateController* controller_ = nullptr;
    nlohmann::json result_ = nlohmann::json::object();
    QString resultId_;
    bool filling_ = false;
};
