#include "AutomationServerDialog.h"

#include "AutomationServer.h"
#include "DocumentController.h"
#include "services/Session.h"
#include "ui_AutomationServerDialog.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

using namespace resonant;

namespace {
// The home directory as ~ (paths shown on screen stay free of user names).
QString shown(const QString& path)
{
    const QString home = QDir::homePath();
    return path.startsWith(home + QLatin1Char('/')) ? QStringLiteral("~") + path.mid(home.size()) : path;
}

QString interfaceName(services::Interface i)
{
    switch (i) {
    case services::Interface::Gui: return QStringLiteral("GUI");
    case services::Interface::Python: return QStringLiteral("Python");
    case services::Interface::Grpc: return QStringLiteral("gRPC");
    case services::Interface::Mcp: return QStringLiteral("MCP");
    case services::Interface::Internal: return QStringLiteral("application");
    case services::Interface::Unspecified: break;
    }
    return QStringLiteral("-");
}

QString permissionNames(std::uint32_t p)
{
    QStringList names;
    for (auto [bit, name] : {std::pair{services::Permission::Read, "read"}, {services::Permission::Modify, "modify"},
                             {services::Permission::Execute, "execute"}, {services::Permission::Files, "files"},
                             {services::Permission::Admin, "admin"}})
        if (services::has(p, bit))
            names << QString::fromLatin1(name);
    return names.join(QStringLiteral(", "));
}
}  // namespace

AutomationServerDialog::AutomationServerDialog(AutomationServer& server, DocumentController& document, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::AutomationServerDialog>()), server_(server), document_(document)
{
    ui_->setupUi(this);
    setProperty("helpTopic", QStringLiteral("py.automation-server"));
    ui_->sessionTable->horizontalHeader()->setStretchLastSection(true);
    ui_->sessionTable->verticalHeader()->hide();
    ui_->directoryEdit->setText(document.filePath().isEmpty() ? QString() : QFileInfo(document.filePath()).absolutePath());
    connect(ui_->browseButton, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Directory for Remote Projects"), ui_->directoryEdit->text());
        if (!dir.isEmpty())
            ui_->directoryEdit->setText(dir);
    });
    connect(ui_->startButton, &QPushButton::clicked, this, [this] {
        if (const QString error = toggleServer(); !error.isEmpty())
            QMessageBox::warning(this, tr("Automation Server"), error);
    });
    connect(ui_->bridgeButton, &QPushButton::clicked, this, [this] {
        if (const QString error = toggleBridge(); !error.isEmpty())
            QMessageBox::warning(this, tr("gRPC Bridge"), error);
    });
    connect(ui_->copyTokenButton, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(server_.token()); });
    connect(ui_->buttonBox->button(QDialogButtonBox::Help), &QPushButton::clicked, this,
            [this] { emit helpRequested(QStringLiteral("py.automation-server")); });
    connect(&server_, &AutomationServer::stateChanged, this, &AutomationServerDialog::refresh);
    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &AutomationServerDialog::refresh);
    timer_->start();
    if (!AutomationServer::available()) {
        ui_->startButton->setEnabled(false);
        ui_->stateLabel->setText(tr("Not available: this build has no automation API (built without Protocol Buffers)"));
    }
    refresh();
}

AutomationServerDialog::~AutomationServerDialog() = default;

QString AutomationServerDialog::toggleServer()
{
    if (server_.running()) {
        server_.stop();
        refresh();
        return {};
    }
    AutomationServer::Options o;
    o.allowModify = ui_->modifyCheck->isChecked();
    o.allowExecute = ui_->executeCheck->isChecked();
    o.allowFiles = ui_->filesCheck->isChecked();
    o.projectDirectory = o.allowFiles ? ui_->directoryEdit->text().trimmed() : QString();
    o.allowAdmin = ui_->adminCheck->isChecked();
    const QString error = server_.start(o);
    refresh();
    return error;
}

QString AutomationServerDialog::toggleBridge()
{
    if (server_.bridgeRunning()) {
        server_.stopBridge();
        refresh();
        return {};
    }
    const QString error = server_.startBridge(ui_->portSpin->value());
    refresh();
    return error;
}

void AutomationServerDialog::refresh()
{
    const bool on = server_.running();
    if (AutomationServer::available())
        ui_->stateLabel->setText(on ? tr("Listening (%n connection(s) open)", nullptr, server_.activeConnections()) : tr("Stopped"));
    ui_->startButton->setText(on ? tr("Stop Server") : tr("Start Server"));
    ui_->socketEdit->setText(on ? shown(server_.socketPath()) : QString());
    ui_->tokenEdit->setText(on ? shown(server_.tokenFile()) : QString());
    ui_->copyTokenButton->setEnabled(on);
    ui_->permissionsGroup->setEnabled(!on);
    ui_->bridgeButton->setEnabled(on);
    ui_->portSpin->setEnabled(on && !server_.bridgeRunning());
    ui_->bridgeButton->setText(server_.bridgeRunning() ? tr("Stop Bridge") : tr("Start Bridge"));
    ui_->bridgeLabel->setText(server_.bridgeRunning() ? server_.bridgeAddress() : tr("not running (127.0.0.1 only)"));
    const auto sessions = document_.session().sessions();
    ui_->sessionTable->setRowCount(static_cast<int>(sessions.size()));
    const auto now = std::chrono::system_clock::now();
    for (int row = 0; row < static_cast<int>(sessions.size()); ++row) {
        const services::SessionInfo& s = sessions[static_cast<std::size_t>(row)];
        const auto ago = std::chrono::duration_cast<std::chrono::seconds>(now - s.lastSeen).count();
        const QStringList cells = {QString::fromStdString(s.clientName), interfaceName(s.source), permissionNames(s.permissions),
                                   ago < 1 ? tr("now") : tr("%1 s ago").arg(ago)};
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = ui_->sessionTable->item(row, column);
            if (!item) {
                item = new QTableWidgetItem;
                ui_->sessionTable->setItem(row, column, item);
            }
            item->setText(cells[column]);
        }
    }
}

int AutomationServerDialog::sessionRows() const
{
    return ui_->sessionTable->rowCount();
}

QString AutomationServerDialog::sessionCell(int row, int column) const
{
    const auto* item = ui_->sessionTable->item(row, column);
    return item ? item->text() : QString();
}

QString AutomationServerDialog::stateText() const
{
    return ui_->stateLabel->text();
}
