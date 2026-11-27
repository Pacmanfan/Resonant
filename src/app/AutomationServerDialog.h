// The Automation Server dialog (REQ-CHAT-20261126-012): start and stop the
// GUI's automation server, choose what remote sessions may do, start the
// gRPC bridge, and see the sessions. Interface: forms/AutomationServerDialog.ui.
#pragma once

#include <QDialog>

#include <memory>

class AutomationServer;
class DocumentController;
class QTimer;
namespace Ui {
class AutomationServerDialog;
}

class AutomationServerDialog : public QDialog {
    Q_OBJECT
public:
    AutomationServerDialog(AutomationServer& server, DocumentController& document, QWidget* parent = nullptr);
    ~AutomationServerDialog() override;

    void refresh();
    // As the buttons do (tests): returns the error shown, or empty.
    QString toggleServer();
    QString toggleBridge();
    int sessionRows() const;
    QString sessionCell(int row, int column) const;
    QString stateText() const;

signals:
    void helpRequested(const QString& topic);

private:
    std::unique_ptr<Ui::AutomationServerDialog> ui_;
    AutomationServer& server_;
    DocumentController& document_;
    QTimer* timer_ = nullptr;
};
