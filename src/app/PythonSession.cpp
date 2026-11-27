#include "PythonSession.h"

#include "PythonEnvironment.h"
#include "ResourcePaths.h"
#include "foundation/Log.h"

#include <QDir>
#include <QJsonDocument>
#include <QProcessEnvironment>

#include <csignal>

PythonSession::PythonSession(QObject* parent) : QObject(parent) {}

PythonSession::~PythonSession()
{
    stop();
}

QString PythonSession::interpreter() const
{
    return pythonLaunch(ResourcePaths::locate()).program;
}

QString PythonSession::description() const
{
    const ResourcePaths paths = ResourcePaths::locate();
    const PythonLaunch launch = pythonLaunch(paths);
    return QStringLiteral("interpreter %1 (%2); package %3; native module %4")
        .arg(launch.program.isEmpty() ? QStringLiteral("(python3 not found)") : launch.program, launch.origin,
             paths.pythonPackage.isEmpty() ? QStringLiteral("(not found)") : paths.pythonPackage,
             paths.pythonModule.isEmpty() ? QStringLiteral("(in the package)") : paths.pythonModule);
}

bool PythonSession::start()
{
    if (isRunning())
        return true;
    startupError_.clear();
    const PythonLaunch launch = pythonLaunch(ResourcePaths::locate());
    if (!launch.usable() || !launch.error.isEmpty()) {
        startupError_ = launch.error;
        return false;
    }
    process_ = new QProcess(this);
    QProcessEnvironment env = launch.environment;
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    process_->setProcessEnvironment(env);
    process_->setProgram(launch.program);
    process_->setArguments({QStringLiteral("-u"), QStringLiteral("-m"), QStringLiteral("resonant.worker")});
    connect(process_, &QProcess::readyReadStandardOutput, this, &PythonSession::readOutput);
    connect(process_, &QProcess::readyReadStandardError, this, &PythonSession::readErrors);
    connect(process_, &QProcess::finished, this, &PythonSession::processEnded);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            startupError_ = QStringLiteral("The Python worker could not be started: ") + process_->errorString();
            emit crashed(startupError_);
        }
    });
    stopping_ = false;
    process_->start();
    resonant::logInfo("python", "worker started: " + description().toStdString());
    return true;
}

void PythonSession::stop()
{
    if (!process_)
        return;
    stopping_ = true;
    if (process_->state() != QProcess::NotRunning) {
        process_->closeWriteChannel();
        if (!process_->waitForFinished(1500)) {
            process_->kill();
            process_->waitForFinished(1000);
        }
    }
    failPending(QStringLiteral("the Python worker was stopped"));
    process_->deleteLater();
    process_ = nullptr;
    buffer_.clear();
    if (ready_) {
        ready_ = false;
        emit readyChanged(false);
    }
}

void PythonSession::restart()
{
    stop();
    start();
}

int PythonSession::request(QJsonObject r)
{
    if (!isRunning())
        return 0;
    const int id = nextId_++;
    r.insert(QStringLiteral("id"), id);
    const bool wasBusy = isBusy();
    pending_.insert(id);
    process_->write(QJsonDocument(r).toJson(QJsonDocument::Compact) + '\n');
    if (!wasBusy)
        emit busyChanged(true);
    return id;
}

int PythonSession::execute(const QString& code, const QString& mode, const QString& filename)
{
    return request({{QStringLiteral("op"), QStringLiteral("exec")}, {QStringLiteral("code"), code}, {QStringLiteral("mode"), mode},
                    {QStringLiteral("filename"), filename}});
}

int PythonSession::setProject(const QJsonObject& projectJson)
{
    return request({{QStringLiteral("op"), QStringLiteral("set_project")}, {QStringLiteral("project"), projectJson}});
}

void PythonSession::interrupt()
{
    if (isRunning() && isBusy())
        ::kill(static_cast<pid_t>(process_->processId()), SIGINT);
}

void PythonSession::readOutput()
{
    buffer_ += process_->readAllStandardOutput();
    int newline;
    while ((newline = buffer_.indexOf('\n')) >= 0) {
        const QByteArray line = buffer_.left(newline).trimmed();
        buffer_.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            emit output(-1, QStringLiteral("stdout"), QString::fromUtf8(line) + QLatin1Char('\n'));
            continue;
        }
        handle(doc.object());
    }
}

void PythonSession::readErrors()
{
    // What the interpreter itself writes (fatal errors before the worker runs).
    emit output(-1, QStringLiteral("stderr"), QString::fromUtf8(process_->readAllStandardError()));
}

void PythonSession::handle(const QJsonObject& m)
{
    const QString type = m.value(QStringLiteral("type")).toString();
    const int id = m.value(QStringLiteral("id")).toInt(-1);
    if (type == QLatin1String("ready")) {
        readyInfo_ = m;
        ready_ = true;
        if (m.value(QStringLiteral("api")).isNull())
            emit output(-1, QStringLiteral("stderr"),
                        QStringLiteral("The resonant package could not be imported: %1\n").arg(m.value(QStringLiteral("error")).toString()));
        emit readyChanged(true);
        return;
    }
    emit received(id, m);
    if (type == QLatin1String("stdout") || type == QLatin1String("stderr"))
        emit output(id, type, m.value(QStringLiteral("text")).toString());
    else if (type == QLatin1String("apply"))
        emit applyRequested(id, m.value(QStringLiteral("project")).toObject(), m.value(QStringLiteral("description")).toString());
    else if (type == QLatin1String("pull"))
        emit pullRequested(id);
    else if (type == QLatin1String("message"))
        emit statusMessage(m.value(QStringLiteral("text")).toString());
    else if (type == QLatin1String("done")) {
        pending_.remove(id);
        emit finished(id, m.value(QStringLiteral("ok")).toBool());
        if (!isBusy())
            emit busyChanged(false);
    }
}

void PythonSession::failPending(const QString& reason)
{
    const QSet<int> pending = pending_;
    pending_.clear();
    for (int id : pending) {
        emit received(id, {{QStringLiteral("type"), QStringLiteral("error")}, {QStringLiteral("error"), QStringLiteral("WorkerEnded")},
                           {QStringLiteral("message"), reason}, {QStringLiteral("traceback"), QString()}});
        // Every request ends with "done", also when the worker could not answer it.
        emit received(id, {{QStringLiteral("type"), QStringLiteral("done")}, {QStringLiteral("ok"), false}});
        emit finished(id, false);
    }
    if (!pending.isEmpty())
        emit busyChanged(false);
}

void PythonSession::processEnded(int exitCode, QProcess::ExitStatus status)
{
    if (stopping_)
        return;
    const QString reason = status == QProcess::CrashExit ? QStringLiteral("the Python worker crashed")
                                                         : QStringLiteral("the Python worker ended (exit status %1)").arg(exitCode);
    resonant::logWarning("python", reason.toStdString());
    ready_ = false;
    emit readyChanged(false);
    failPending(reason);
    emit crashed(reason + QStringLiteral("; the application and the project are unaffected. Restart the console to continue."));
}
