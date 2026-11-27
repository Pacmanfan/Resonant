// The Workflows dialog (RES-AUTO-*): the workflow templates (built in and
// from plugins), their parameters, a run in the background on a copy of the
// project, its result, and the runs recorded in the project. The workflow's
// changes come back as one undoable step. Interface: forms/WorkflowsDialog.ui.
#pragma once

#include "foundation/Id.h"
#include "foundation/Result.h"

#include <QDialog>

#include <functional>
#include <memory>

class DocumentController;
namespace resonant {
class Project;
struct JobInfo;
namespace automation {
class WorkflowJob;
}
}  // namespace resonant
namespace Ui {
class WorkflowsDialog;
}

class WorkflowsDialog : public QDialog {
    Q_OBJECT
public:
    using Apply = std::function<resonant::Status(const resonant::Project& changed, const QString& description)>;
    WorkflowsDialog(DocumentController& document, Apply apply, QWidget* parent = nullptr);
    ~WorkflowsDialog() override;

    QStringList workflowIds() const;
    bool selectWorkflow(const QString& id);
    QString selectedWorkflow() const;
    bool setParameter(const QString& id, const QString& jsonValue);
    void setOutputDirectory(const QString& directory);
    bool run();
    void stop();
    bool isRunning() const;
    QString resultText() const;
    int historyCount() const;

signals:
    void runFinished(bool ok, const QString& message);
    void helpRequested(const QString& topic);

private:
    void fillList();
    void fillHistory();
    void showWorkflow();
    void onJob(const resonant::JobInfo& info);

    std::unique_ptr<Ui::WorkflowsDialog> ui_;
    DocumentController& document_;
    Apply apply_;
    std::shared_ptr<resonant::automation::WorkflowJob> job_;
    resonant::Id jobId_;
};
