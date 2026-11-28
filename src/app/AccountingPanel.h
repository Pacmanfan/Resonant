// The "Accounting" panel (REQ-CHAT-20261126-022, RES-ACC-016): a study run as a job, its
// system boundary chosen from the study's systems or ticked member by member (the same
// exchanges are reclassified without running again), the flows with calculated widths, the
// conservation checks with their statuses and missing contributions, the breakdown, the
// forces with their origins, the histories, a record kept in the project and an export.
#pragma once

#include <QWidget>

#include <nlohmann/json.hpp>

#include <memory>

class AccountingController;
class EnergyFlowView;
namespace Ui {
class AccountingPanel;
}
class QPushButton;

class AccountingPanel : public QWidget {
    Q_OBJECT
public:
    explicit AccountingPanel(QWidget* parent = nullptr);
    ~AccountingPanel() override;

    void setController(AccountingController* controller);

    // For tests.
    int studyCount() const;
    void selectStudy(const QString& id);
    QPushButton* runButton() const;
    QPushButton* applyButton() const;
    QPushButton* saveButton() const;
    QStringList systemChoices() const;
    void selectSystem(const QString& id);
    void tickMembers(const QStringList& ids);
    QStringList familyChoices() const;
    void selectFamily(const QString& id);
    QString checkStatus(const QString& model) const;
    const EnergyFlowView& flowView() const;
    int forceRows() const;
    QStringList seriesChoices() const;
    void showSeries(int index);
    int plottedPoints() const;
    QString statusText() const;
    bool exportTo(const QString& path);

public slots:
    void refresh();

private:
    void showResult(const QString& resultId);
    void showReport(const nlohmann::json& report);
    void showFlows();
    void plotSeries();

    std::unique_ptr<Ui::AccountingPanel> ui_;
    AccountingController* controller_ = nullptr;
    QString resultId_;
    nlohmann::json result_ = nlohmann::json::object();
    nlohmann::json report_ = nlohmann::json::object();
    bool filling_ = false;
};
