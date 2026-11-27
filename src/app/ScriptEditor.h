// The script editor (RES-PY-008): Python files with highlighting, line
// numbers, indentation and completion; Run, Run Selection and Stop in the
// console's Python worker (the same `project`, `resonant` and `app`); errors
// listed with their lines; templates; example scripts; scripts stored in the
// project. User scripts live in the user's data directory, never among the
// application's files. The interface is in forms/ScriptEditor.ui.
#pragma once

#include <QJsonObject>
#include <QMainWindow>

#include <functional>
#include <memory>

class DocumentController;
class PythonSession;
namespace Ui {
class ScriptEditor;
}

class ScriptEditor : public QMainWindow {
    Q_OBJECT
public:
    explicit ScriptEditor(QWidget* parent = nullptr);
    ~ScriptEditor() override;

    void setSession(PythonSession* session);
    void setDocument(DocumentController* document);
    // Starts the worker and gives it the open project when it is not running yet.
    void setStarter(std::function<bool()> starter) { starter_ = std::move(starter); }

    QString text() const;
    void setText(const QString& text);
    QString currentFile() const { return file_; }
    bool openFile(const QString& path);
    bool saveFile(const QString& path);
    void newScript(const QString& text = {});

    bool run();
    bool runSelection();
    void stop();
    bool isRunning() const { return running_ != 0; }

    QString outputText() const;
    QStringList problems() const;
    int errorLine() const;

    static QString userScriptDirectory();
    QStringList templateFiles() const;
    QStringList exampleFiles() const;

    // Scripts stored in the project ("scripts" extension: [{name, code}]).
    bool attachToProject(const QString& name);
    QStringList projectScripts() const;
    bool openProjectScript(const QString& name);

signals:
    void helpRequested(const QString& topic);

private:
    bool execute(const QString& code, const QString& filename, int lineOffset);
    void onReceived(int id, const QJsonObject& message);
    void fillMenus();
    void updateTitle();
    void updateActions();
    void appendOutput(const QString& text, bool error = false);

    std::unique_ptr<Ui::ScriptEditor> ui_;
    PythonSession* session_ = nullptr;
    DocumentController* document_ = nullptr;
    std::function<bool()> starter_;
    QString file_;
    int running_ = 0;
    int lineOffset_ = 0;
    QString runName_;
    int pendingCompletion_ = 0;
    QString completionPrefix_;
};
