#include "AutomationServer.h"

#include "DocumentController.h"
#include "PythonEnvironment.h"
#include "ResourcePaths.h"
#include "services/Services.h"

#ifdef RESONANT_HAVE_AUTOMATION_API
#include "Dispatcher.h"
#include "LocalEndpoint.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>

#include <sys/stat.h>

using namespace resonant;

AutomationServer::AutomationServer(DocumentController& document, QObject* parent) : QObject(parent), document_(document) {}

AutomationServer::~AutomationServer()
{
    stopBridge();
    stop();
}

bool AutomationServer::available()
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    return true;
#else
    return false;
#endif
}

QString AutomationServer::start(const Options& options)
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    if (running())
        return tr("The automation server is already running.");
    if (options.allowFiles && options.projectDirectory.isEmpty())
        return tr("Choose the directory in which clients may open and save projects.");
    policy_ = static_cast<std::uint32_t>(services::Permission::Read);
    if (options.allowModify)
        policy_ |= static_cast<std::uint32_t>(services::Permission::Modify);
    if (options.allowExecute)
        policy_ |= static_cast<std::uint32_t>(services::Permission::Execute);
    if (options.allowFiles)
        policy_ |= static_cast<std::uint32_t>(services::Permission::Files);
    if (options.allowAdmin)
        policy_ |= static_cast<std::uint32_t>(services::Permission::Admin);
    document_.services().project.setProjectDirectory(options.projectDirectory.isEmpty() ? std::string()
                                                                                       : QDir(options.projectDirectory).absolutePath().toStdString());
    // Calls run on this (the user-interface) thread; the caller's thread waits.
    dispatcher_ = std::make_unique<automation_api::Dispatcher>(
        document_.session(),
        [this](const std::function<void()>& f) {
            if (QThread::currentThread() == thread())
                f();
            else
                QMetaObject::invokeMethod(this, [&f] { f(); }, Qt::BlockingQueuedConnection);
        },
        policy_);
    endpoint_ = std::make_unique<automation_api::LocalEndpoint>(*dispatcher_);
    token_ = QString::fromStdString(automation_api::LocalEndpoint::newToken());
    const std::string path = options.socketPath.isEmpty() ? automation_api::LocalEndpoint::defaultSocketPath() : options.socketPath.toStdString();
    if (const std::string error = endpoint_->start(path, token_.toStdString()); !error.empty()) {
        endpoint_.reset();
        dispatcher_.reset();
        return QString::fromStdString(error);
    }
    tokenFile_ = QFileInfo(QString::fromStdString(path)).dir().absoluteFilePath(QStringLiteral("automation.token"));
    const mode_t previous = ::umask(0177);
    QFile file(tokenFile_);
    const bool written = file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(token_.toUtf8() + '\n') > 0;
    file.close();
    ::umask(previous);
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!written) {
        stop();
        return tr("The token file could not be written.");
    }
    emit stateChanged();
    return {};
#else
    Q_UNUSED(options);
    return tr("This build of Resonant has no automation API (it was built without Protocol Buffers).");
#endif
}

void AutomationServer::stop()
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    if (!endpoint_)
        return;
    stopBridge();
    // Calls waiting for this thread are run while the connections end.
    endpoint_->stop([] { QCoreApplication::processEvents(QEventLoop::AllEvents, 5); });
    endpoint_.reset();
    dispatcher_.reset();
    QFile::remove(tokenFile_);
    tokenFile_.clear();
    token_.clear();
    emit stateChanged();
#endif
}

bool AutomationServer::running() const
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    return endpoint_ && endpoint_->running();
#else
    return false;
#endif
}

QString AutomationServer::socketPath() const
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    return endpoint_ ? QString::fromStdString(endpoint_->path()) : QString();
#else
    return {};
#endif
}

QString AutomationServer::tokenFile() const
{
    return tokenFile_;
}

int AutomationServer::activeConnections() const
{
#ifdef RESONANT_HAVE_AUTOMATION_API
    return endpoint_ ? endpoint_->activeConnections() : 0;
#else
    return 0;
#endif
}

QString AutomationServer::startBridge(int port)
{
    if (!running())
        return tr("Start the automation server first.");
    if (bridgeRunning())
        return tr("The gRPC bridge is already running.");
    const ResourcePaths paths = ResourcePaths::locate();
    if (paths.pythonPackage.isEmpty())
        return tr("The Python package (resonant_api) was not found.");
    const PythonLaunch launch = pythonLaunch(paths);
    if (!launch.usable())
        return launch.error;
    bridge_ = new QProcess(this);
    QProcessEnvironment env = launch.environment;
    if (!paths.apiDescriptors.isEmpty())
        env.insert(QStringLiteral("RESONANT_API_DESCRIPTORS"), paths.apiDescriptors);
    env.insert(QStringLiteral("RESONANT_AUTOMATION_TOKEN"), token_);
    bridge_->setProcessEnvironment(env);
    bridge_->setProgram(launch.program);
    bridge_->setArguments({QStringLiteral("-m"), QStringLiteral("resonant_api.bridge"), QStringLiteral("--socket"), socketPath(), QStringLiteral("--port"),
                           QString::number(port)});
    bridge_->start();
    if (!bridge_->waitForStarted(5000) || !bridge_->waitForReadyRead(15000)) {
        const QString error = QString::fromUtf8(bridge_->readAllStandardError()).trimmed();
        stopBridge();
        return tr("The gRPC bridge did not start%1").arg(error.isEmpty() ? QStringLiteral(".") : QStringLiteral(": ") + error);
    }
    const QJsonObject line = QJsonDocument::fromJson(bridge_->readLine()).object();
    bridgeAddress_ = line.value(QStringLiteral("grpc")).toString();
    if (bridgeAddress_.isEmpty()) {
        const QString error = QString::fromUtf8(bridge_->readAllStandardError()).trimmed();
        stopBridge();
        return tr("The gRPC bridge did not report its address%1").arg(error.isEmpty() ? QStringLiteral(".") : QStringLiteral(": ") + error);
    }
    connect(bridge_, &QProcess::finished, this, [this] {
        bridgeAddress_.clear();
        emit stateChanged();
    });
    emit stateChanged();
    return {};
}

void AutomationServer::stopBridge()
{
    if (!bridge_)
        return;
    bridge_->disconnect(this);
    if (bridge_->state() != QProcess::NotRunning) {
        bridge_->terminate();
        if (!bridge_->waitForFinished(3000))
            bridge_->kill();
        bridge_->waitForFinished(1000);
    }
    bridge_->deleteLater();
    bridge_ = nullptr;
    bridgeAddress_.clear();
    emit stateChanged();
}

bool AutomationServer::bridgeRunning() const
{
    return bridge_ && bridge_->state() == QProcess::Running && !bridgeAddress_.isEmpty();
}
