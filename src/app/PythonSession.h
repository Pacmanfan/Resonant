// The application's side of the Python worker process (ADR-000215,
// docs/python/PYTHON_EXECUTION_ARCHITECTURE.md). Python runs in its own
// process (`python3 -m resonant.worker`), so a script never blocks the
// user interface and a crash in a script or extension never takes the
// application down. The worker has the open project as a copy; changes come
// back as an "apply" message, which the application turns into one undoable
// change. The worker cannot reach the application's devices: scripts and
// plugins never start a physical output.
//
// Not a sandbox: the worker runs with the user's permissions.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>

class PythonSession : public QObject {
    Q_OBJECT
public:
    explicit PythonSession(QObject* parent = nullptr);
    ~PythonSession() override;

    // Starts the worker (once; restart() starts a fresh one). False, with
    // startupError(), when Python or the resonant package cannot be found.
    bool start();
    void stop();
    void restart();
    bool isRunning() const { return process_ && process_->state() != QProcess::NotRunning; }
    bool isReady() const { return ready_; }
    bool isBusy() const { return !pending_.isEmpty(); }
    QString startupError() const { return startupError_; }
    QJsonObject readyInfo() const { return readyInfo_; }  // protocol, api, core versions (or error)
    QString interpreter() const;
    QString description() const;  // for the user: interpreter, package, module

    // Sends a request ({"op": ...}); returns its id (0 when the worker is not running).
    int request(QJsonObject request);
    int execute(const QString& code, const QString& mode = QStringLiteral("exec"), const QString& filename = QStringLiteral("<console>"));
    int setProject(const QJsonObject& projectJson);
    // Interrupts the running request (KeyboardInterrupt in the worker).
    void interrupt();

signals:
    void readyChanged(bool ready);
    void busyChanged(bool busy);
    void output(int id, const QString& stream, const QString& text);  // "stdout" | "stderr"
    void received(int id, const QJsonObject& message);                 // every message
    void finished(int id, bool ok);
    void applyRequested(int id, const QJsonObject& project, const QString& description);
    void pullRequested(int id);
    void statusMessage(const QString& text);
    void crashed(const QString& reason);

private:
    void readOutput();
    void readErrors();
    void handle(const QJsonObject& message);
    void processEnded(int exitCode, QProcess::ExitStatus status);
    void failPending(const QString& reason);

    QProcess* process_ = nullptr;
    QByteArray buffer_;
    bool ready_ = false;
    bool stopping_ = false;
    int nextId_ = 1;
    QSet<int> pending_;
    QString startupError_;
    QJsonObject readyInfo_;
};
