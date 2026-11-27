// The GUI's automation server (REQ-CHAT-20261126-012, ADR-000216): the local
// endpoint of the automation API on the open document, with every call run
// on the user-interface thread (so a remote change is exactly a GUI change:
// same services, same undo history, same notifications), and optionally the
// gRPC bridge as a child process. Off until the user starts it; the GUI never
// needs it. Not a sandbox: a client holding the token acts on the project
// with the permissions its session was granted.
#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>

class DocumentController;
class QProcess;

namespace resonant::automation_api {
class Dispatcher;
class LocalEndpoint;
}  // namespace resonant::automation_api

class AutomationServer : public QObject {
    Q_OBJECT
public:
    explicit AutomationServer(DocumentController& document, QObject* parent = nullptr);
    ~AutomationServer() override;

    // Whether this build has the automation API (protobuf).
    static bool available();

    struct Options {
        QString socketPath;          // empty: the per-user default
        bool allowModify = true;     // remote sessions may change the project (undoable)
        bool allowExecute = true;    // remote sessions may start and cancel jobs
        bool allowFiles = false;     // remote sessions may open / save within projectDirectory
        QString projectDirectory;
        bool allowAdmin = false;     // remote sessions may list all sessions and read all audit records
    };
    // Starts the local endpoint; returns an error message, or an empty string.
    QString start(const Options& options);
    void stop();
    bool running() const;
    QString socketPath() const;
    QString tokenFile() const;
    QString token() const { return token_; }
    int activeConnections() const;
    std::uint32_t policy() const { return policy_; }

    // The gRPC bridge (python3 -m resonant_api.bridge) on 127.0.0.1:port; returns an error message, or empty.
    QString startBridge(int port);
    void stopBridge();
    bool bridgeRunning() const;
    QString bridgeAddress() const { return bridgeAddress_; }

signals:
    void stateChanged();

private:
    DocumentController& document_;
    std::unique_ptr<resonant::automation_api::Dispatcher> dispatcher_;
    std::unique_ptr<resonant::automation_api::LocalEndpoint> endpoint_;
    QString token_, tokenFile_;
    std::uint32_t policy_ = 0;
    QProcess* bridge_ = nullptr;
    QString bridgeAddress_;
};
