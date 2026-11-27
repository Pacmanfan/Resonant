// The Python console (RES-PY-007): an interactive interpreter in the Python
// worker process with the open project as `project`, the `resonant` API and
// `app` (app.apply(project, "what") hands changes back as one undoable step).
// Multi-line input, highlighting, Tab completion, history, `name?` help,
// running script files, Stop (KeyboardInterrupt). The interface is in
// forms/PythonConsole.ui.
#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <memory>

class PythonSession;
namespace Ui {
class PythonConsole;
}

class PythonConsole : public QWidget {
    Q_OBJECT
public:
    explicit PythonConsole(QWidget* parent = nullptr);
    ~PythonConsole() override;

    void setSession(PythonSession* session);
    // The open project, as JSON, for "Send Project" (and when the worker starts).
    void setProjectProvider(std::function<QJsonObject()> provider) { projectProvider_ = std::move(provider); }

    // As if typed and Enter pressed: starts the worker if needed; false when it cannot start.
    bool submit(const QString& code);
    bool runFile(const QString& path);
    // Runs a block of code (exec mode) as a script would; returns its request id (0: busy or no worker).
    int runCode(const QString& code, const QString& label = QStringLiteral("script"));
    void sendProject(bool announce = true);  // announce: say so in the output
    void clear();
    QString outputText() const;
    QString inputText() const;
    bool isBusy() const;
    const QStringList& history() const { return history_; }
    void appendOutput(const QString& text, const QString& kind = QStringLiteral("stdout"));

private:
    void ensureStarted();
    void submitInput();
    void requestCompletion(const QString& prefix);
    void recall(int direction);
    void updateState();
    void onReceived(int id, const QJsonObject& message);
    void echoPending();

    std::unique_ptr<Ui::PythonConsole> ui_;
    PythonSession* session_ = nullptr;
    std::function<QJsonObject()> projectProvider_;
    QStringList history_;
    int historyPosition_ = 0;
    int pendingExec_ = 0;
    QString pendingSource_;
    int pendingCompletion_ = 0;
    QString completionPrefix_;
    bool projectSent_ = false;
    bool echoed_ = false;
};
