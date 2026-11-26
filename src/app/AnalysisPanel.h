// The bottom analysis area: Console (the application log), Diagnostics
// (what the application is running on and working with) and Simulation
// Status (background jobs). Later milestones add plot and field-analysis
// tabs through addTab().
#pragma once

#include "foundation/Log.h"
#include "simulation/Job.h"

#include <QHash>
#include <QWidget>

#include <memory>

class DocumentController;
namespace Ui {
class AnalysisPanel;
}

class AnalysisPanel : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisPanel(QWidget* parent = nullptr);
    ~AnalysisPanel() override;

    void setController(DocumentController* controller);
    // Text shown under "Graphics" in the diagnostics tab.
    void setGraphicsDescription(const QString& description);

    int addTab(QWidget* widget, const QString& title);
    void showTab(QWidget* widget);
    void showSimulationStatus();

    // For tests and for the Tools menu.
    QString consoleText() const;
    int jobRowCount() const;
    QString jobState(int row) const;
    QStringList diagnosticsItems() const;

public slots:
    void clearConsole();
    void refreshDiagnostics();

private:
    void appendEntry(const resonant::LogEntry& entry);
    void reloadConsole();
    void onJobChanged(const resonant::JobInfo& info);
    void updateJobSummary();

    std::unique_ptr<Ui::AnalysisPanel> ui_;
    DocumentController* controller_ = nullptr;
    int logSink_ = 0;
    QString graphics_ = QStringLiteral("not initialized yet");
    QHash<QString, int> jobRows_;
};
